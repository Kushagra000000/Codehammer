#!/usr/bin/env python3
"""Instrumented head-to-head evaluator for bot-v12.0-kushagra (unswbc 1.1.0).

Runs matches through the installed unswbc engine exactly as `unswbc run` does
(same Pool/Bot/EngineModule; optional --sandbox), but also records, per game:
W/D/L from the engine result, elimination, total length (engine), longest living
length (last LENGTH each survivor reported: may miss growth in its final move),
every death (team, reason, last known length, logged intent), escape splits and
whether each escape child outlived its birth round, portal traversals by the
first steps of a move inside the 7x7 window and deaths on those turns, FEED
events with a recipient-growth proxy, and sandbox points maxima.

Errors (build failures, engine exceptions) are recorded as ERROR and never
counted as W/D/L. Nothing is submitted anywhere.

Example (repository root):
  python3 battlecode-automation/scripts/v12_eval.py --a bots/current/bot-v12.0-kushagra \
      --b bots/current/bot-v10.3.3 --seeds 101 102 --out battlecode-automation/experiments/v12/run1 --dry-run
"""
from __future__ import annotations

import argparse
import hashlib
import json
import multiprocessing as mp
import pathlib
import sys
import time
import traceback

ROOT = pathlib.Path(__file__).resolve().parents[2]
INTENTS = ("FEED", "REMNANT_CLEAR", "TRAPPED_SELF", "GROWER_REVERSE", "WORKER_REVERSE",
           "ATTACK_SPRINT", "ESCAPE_SPRINT")


def fingerprint(project: pathlib.Path) -> str:
    h = hashlib.sha256()
    for p in sorted(project.iterdir()):
        if p.is_file() and p.suffix in (".cpp", ".hpp", ".toml", ".h", ".c"):
            h.update(p.name.encode() + b"\0" + p.read_bytes() + b"\0")
    return h.hexdigest()


def parse_block(block: bytes):
    lines = block.decode(errors="replace").split("\n")
    info = {}
    i = 0
    while i < len(lines):
        parts = lines[i].split()
        if not parts:
            i += 1
            continue
        key = parts[0]
        if key in ("ROUND", "LENGTH", "UNIT_COUNT"):
            info[key] = int(parts[1])
        elif key == "DIR":
            info["DIR"] = parts[1]
        elif key == "NUM_MSGS":
            i += int(parts[1])
        elif key == "DRAGON_BODIES":
            n = int(parts[1])
            i += 1 + n
            # 8 lines horizontal edges, 7 lines vertical edges
            rest = [l.split() for l in lines[i:] if l.strip()]
            info["H"] = rest[0:8]
            info["V"] = rest[8:15]
            break
        i += 1
    return info


def portal_steps(info, steps: str) -> bool:
    """Does the move cross a portal edge while still inside the 7x7 window?"""
    H, V = info.get("H"), info.get("V")
    if not H or not V or len(H) < 8 or len(V) < 7:
        return False
    r, c = 3, 3
    for s in steps:
        try:
            if s == "N":
                e, r2, c2 = H[r][c], r - 1, c
            elif s == "S":
                e, r2, c2 = H[r + 1][c], r + 1, c
            elif s == "W":
                e, r2, c2 = V[r][c], r, c - 1
            else:
                e, r2, c2 = V[r][c + 1], r, c + 1
        except IndexError:
            return False
        if e not in (".", "w"):
            return True
        r, c = r2, c2
        if not (0 <= r < 7 and 0 <= c < 7):
            return False
    return False


def play(task):
    map_path, bot_a, bot_b, seed, sandbox = task
    sys.path.insert(0, str(pathlib.Path(__import__("unswbc").__file__).parent.parent))
    from unswbc import run as ur
    from unswbc.bot import Bot, Pool
    from unswbc.engine import DEBUG_ALL, DEBUG_LIMITS, EngineModule

    rec = dict(map=map_path, a=bot_a, b=bot_b, seed=seed, sandbox=sandbox, status="ok")
    t0 = time.time()
    pools = {}
    live = {}
    teams = {}
    last_len = {}
    last_round = {}
    pending_child = {}  # team -> (parent id, round, kind)
    children = {}  # child id -> dict(birth, team, kind, parent)
    deaths = []
    feeds = []
    intents = {}
    last_logs = {}
    portal_turn = {}
    portal_moves = {"A": 0, "B": 0}
    points = {"A": [], "B": []}
    len_hist = {}
    actor = [None]
    last_action = {}
    try:
        pool_type, bot_type = Pool, Bot
        if sandbox:
            from unswbc.sandbox import SandboxBot, SandboxPool
            pool_type, bot_type = SandboxPool, SandboxBot
        argv_a, dir_a, kind_a = ur._resolve(str(ROOT / bot_a), sandbox)
        argv_b, dir_b, kind_b = ur._resolve(str(ROOT / bot_b), sandbox)
        for team, argv, botdir, kind in (("A", argv_a, dir_a, kind_a), ("B", argv_b, dir_b, kind_b)):
            maker = pool_type
            if sandbox and kind == "wasm":  # as unswbc run: compiled C/C++ runs in the wasm pool
                from unswbc.sandbox import WasmPool
                maker = WasmPool
            pools[team] = maker(argv, cwd=str(botdir), key=f"{seed:016x}-{team.lower()}")
        engine = EngineModule()

        def spawn(did, init):
            team = next((l.split()[1] for l in init.decode().splitlines() if l.startswith("TEAM")), "A")
            teams[did] = team
            live[did] = bot_type(pools[team], init=init, name=str(did))
            pc = pending_child.pop(team, None)
            if pc is not None:
                children[did] = dict(parent=pc[0], birth=pc[1], team=team, kind=pc[2], died=None)

        def reply(did, block):
            info = parse_block(block)
            rnd = info.get("ROUND", -1)
            if "LENGTH" in info:
                last_len[did] = info["LENGTH"]
                last_round[did] = rnd
                len_hist.setdefault(did, {})[rnd] = info["LENGTH"]
            bot = live[did]
            out = bot.ask(block)
            metrics = getattr(bot, "live", None)
            if metrics and metrics[0]:
                points[teams[did]].append(metrics[0])
            text = out.decode(errors="replace") if out else ""
            logs = [l[4:] for l in text.split("\n") if l.startswith("LOG ")]
            last_logs[did] = (rnd, logs)
            actor[0] = did
            action = None
            for l in text.split("\n"):
                if l.startswith("MOVE ") or l.startswith("SPLIT "):
                    action = l.strip()
            for l in logs:
                k = l.split(" ")[0]
                if k in INTENTS:
                    intents.setdefault(teams[did], {}).setdefault(k, 0)
                    intents[teams[did]][k] += 1
                if k == "FEED":
                    parts = l.split()
                    feeds.append(dict(team=teams[did], round=rnd, donor=did, recipient=int(parts[1]),
                                      donor_len=int(parts[3])))
            if action and action.startswith("SPLIT"):
                kind = next((l.split(" ")[0] for l in logs if l.startswith(("GROWER_REVERSE", "WORKER_REVERSE"))), "worker_split")
                pending_child[teams[did]] = (did, rnd, kind)
            last_action[did] = (rnd, action)
            portal_turn.pop(did, None)
            if action and action.startswith("MOVE") and portal_steps(info, action.split()[1]):
                portal_moves[teams[did]] += 1
                portal_turn[did] = rnd
            return out

        def death(did, rnd, reason):
            team = teams.get(did, "?")
            lr, logs = last_logs.get(did, (None, []))
            intent = next((l.split(" ")[0] for l in logs if l.split(" ")[0] in INTENTS), None) if lr == rnd else None
            k = actor[0]
            killer = None
            if k is not None and k != did:
                ka = last_action.get(k, (None, None))
                killer = dict(id=k, team=teams.get(k), length=last_len.get(k),
                              action=ka[1] if ka[0] == rnd else None)
            deaths.append(dict(id=did, team=team, round=rnd, reason=reason, length=last_len.get(did),
                               intent=intent, portal=portal_turn.get(did) == rnd, killer=killer))
            if did in children and children[did]["died"] is None:
                children[did]["died"] = rnd
            b = live.pop(did, None)
            if b is not None:
                b.stop()

        debug = DEBUG_ALL | (DEBUG_LIMITS if sandbox else 0)
        result = engine.run(pathlib.Path(ROOT / map_path).read_bytes(), reply, death, spawn,
                            lambda line: None, debug, seed)
        rec.update(winner=result.winner, end_reason=result.end_reason, rounds=result.rounds + 1,
                   a_dragons=result.a_dragons, b_dragons=result.b_dragons,
                   a_total=result.a_length, b_total=result.b_length)
        for team in "AB":
            alive = [d for d in live if teams[d] == team]
            rec[f"{team.lower()}_longest"] = max((last_len.get(d, 0) for d in alive), default=0)
        # Recipient growth proxy: recipient length 8 rounds after the feed minus at the feed.
        for f in feeds:
            h = len_hist.get(f["recipient"], {})
            before = max((l for r, l in h.items() if r <= f["round"]), default=None, key=lambda x: x) if h else None
            rs = sorted(r for r in h if r <= f["round"])
            ra = sorted(r for r in h if f["round"] < r <= f["round"] + 8)
            f["recipient_gain8"] = (h[ra[-1]] - h[rs[-1]]) if rs and ra else None
            f["drops"] = (f["donor_len"] + 1) // 2
        rec.update(deaths=deaths, children=list(children.values()), feeds=feeds, intents=intents,
                   portal_moves=portal_moves,
                   points_max={t: (max(v) if v else None) for t, v in points.items()},
                   points_p99={t: (sorted(v)[int(len(v) * .99) - 1] if v else None) for t, v in points.items()})
    except Exception as exc:  # recorded, never converted into W/D/L
        rec["status"] = "error"
        rec["error"] = f"{type(exc).__name__}: {exc}"
        rec["trace"] = traceback.format_exc()[-2000:]
    finally:
        for b in live.values():
            try:
                b.stop()
            except Exception:
                pass
        for p in pools.values():
            try:
                p.close()
            except Exception:
                pass
    rec["seconds"] = round(time.time() - t0, 1)
    return rec


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--a", required=True, help="candidate project (relative to repo root)")
    ap.add_argument("--b", nargs="+", required=True, help="opponent projects")
    ap.add_argument("--maps", nargs="*", help="map files (default: every maps/**/*.map)")
    ap.add_argument("--seeds", nargs="+", type=int, required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--sandbox", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    maps = args.maps or sorted(str(p.relative_to(ROOT)) for p in (ROOT / "maps").rglob("*.map"))
    tasks = []
    for opp in args.b:
        for mi, m in enumerate(maps):
            for seed in args.seeds:
                # Balanced sides: every (opponent, map, seed) is played from both sides.
                tasks.append((m, args.a, opp, seed, args.sandbox))
                tasks.append((m, opp, args.a, seed, args.sandbox))
    print(f"{len(tasks)} games: {len(args.b)} opponents x {len(maps)} maps x {len(args.seeds)} seeds x 2 sides")
    if args.dry_run:
        for t in tasks:
            print(f"  {t[0]}  A={t[1]}  B={t[2]}  seed={t[3]}  sandbox={t[4]}")
        return 0
    out = ROOT / args.out
    out.mkdir(parents=True, exist_ok=True)
    manifest = dict(candidate=args.a, opponents=args.b, maps=maps, seeds=args.seeds, sandbox=args.sandbox,
                    fingerprints={p: fingerprint(ROOT / p) for p in [args.a, *args.b]},
                    maps_sha256={m: hashlib.sha256((ROOT / m).read_bytes()).hexdigest() for m in maps},
                    toolkit=__import__("importlib.metadata").metadata.version("unswbc"),
                    started=time.strftime("%Y-%m-%dT%H:%M:%S%z"))
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2))
    ledger = out / "matches.jsonl"
    done = set()
    if ledger.exists():
        for line in ledger.read_text().splitlines():
            r = json.loads(line)
            if r["status"] == "ok":
                done.add((r["map"], r["a"], r["b"], r["seed"], r["sandbox"]))
    todo = [t for t in tasks if t not in done]
    # Build every project once up front so parallel games never race on a build dir.
    from unswbc import run as ur
    for proj in [args.a, *args.b]:
        ur._resolve(str(ROOT / proj), args.sandbox)
    print(f"{len(todo)} to run ({len(tasks) - len(todo)} already in ledger)", flush=True)
    with mp.get_context("spawn").Pool(args.workers) as pool, ledger.open("a") as fh:
        for i, rec in enumerate(pool.imap_unordered(play, todo), 1):
            fh.write(json.dumps(rec) + "\n")
            fh.flush()
            side = "A" if rec["a"] == args.a else "B"
            res = "ERROR" if rec["status"] != "ok" else ("D" if rec["winner"] is None else ("W" if rec["winner"] == side else "L"))
            opp = rec["b"] if side == "A" else rec["a"]
            print(f"{i}/{len(todo)} {pathlib.Path(rec['map']).stem:16s} vs {pathlib.Path(opp).name:14s} "
                  f"side {side} seed {rec['seed']}: {res} "
                  f"({rec.get('a_longest')}/{rec.get('b_longest')} longest, {rec['seconds']}s) {rec.get('error', '')}",
                  flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
