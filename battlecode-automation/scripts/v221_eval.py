#!/usr/bin/env python3
"""Paired v22.1 opening evaluation with unswbc 1.2.9 and authoritative replays.

Run from the repository root, for example:
  python3 battlecode-automation/scripts/v221_eval.py \
    --a bots/New/current/bot-v22.1.0 --b bots/New/current/bot-v22.0.0 \
    --maps maps/default_small.map maps/default.map --seeds 22101 \
    --out battlecode-automation/experiments/v221/iteration1 --sandbox

Every map/seed is played from both sides. Sources are copied to fingerprinted
temporary directories: the toolkit's wasm cache otherwise ignores header edits.
The output directory is immutable with respect to its manifest; use a new output
directory after changing sources. Existing successful games can be resumed.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import dataclasses
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import traceback
import zipfile

ROOT = Path(__file__).resolve().parents[2]
TOOLKIT = "1.2.9"
SNAPSHOTS = (25, 50, 75)

# This decoder is extracted from the exact installed viewer, never an old replay
# schema or third-party approximation. The prefix and exported function were
# inspected in unswbc 1.2.9. Fail closed if the bundled viewer changes.
METRICS_JS = r"""
const fs = require('fs');
require(process.argv[2]);
const d = decodeBattlecode(fs.readFileSync(process.argv[3]));
const earlyEnd = 75, repeatWindow = 12;
const bodies = d.match.roundAt(0).bodies.copy();
const teams = new Map([...bodies.dragons].map(([id,b]) => [id,b.team]));
const history = new Map(), stats = {};
for (const t of ['A','B']) stats[t] = {
  population:{}, first_split_round:null, splits:0, early_splits:0,
  pearls:0, early_pearls:0, deaths:{}, early_deaths:0,
  early_turns:0, early_repeat_turns:0, early_unique_head_tiles:0,
  early_peak_population:0, max_population:0
};
const visited = {A:new Set(), B:new Set()};
let round = -1, actor = null, removals = [], moved = new Set();
function flush() {
  if (!removals.length) return;
  if (actor === null || !teams.has(actor)) throw Error('Pearl removal outside a dragon turn');
  const s = stats[teams.get(actor)];
  for (const tile of removals) {
    // A consumed pearl must coincide with an actual movement update by this
    // actor. This also detects any unexpected new tile-removal semantics.
    if (!moved.has(tile)) throw Error('Pearl removal without matching head movement: '+tile);
    s.pearls++;
    if (round < earlyEnd) s.early_pearls++;
  }
  history.delete(actor);
  removals = [];
}
function peak() {
  for (const t of ['A','B']) {
    const n = [...bodies.dragons.values()].filter(b=>b.team===t).length;
    stats[t].max_population = Math.max(stats[t].max_population,n);
    if (round < earlyEnd) stats[t].early_peak_population = Math.max(stats[t].early_peak_population,n);
  }
}
peak();
for (const e of d.events) {
  if (e.type==='roundStart') {
    flush(); round=e.round; actor=null; moved=new Set();
  }
  if (e.type==='turnStart') {
    flush(); actor=e.id; moved=new Set();
    const b=bodies.getById(actor);
    if (!b) throw Error('Turn for absent dragon '+actor);
    if (round < earlyEnd) {
      const s=stats[b.team], h=b.body[0], key=h.x+','+h.y;
      const past=(history.get(actor)||[]).filter(x=>round-x.round<=repeatWindow);
      s.early_turns++;
      if (past.some(x=>x.key===key)) s.early_repeat_turns++;
      past.push({round,key}); history.set(actor,past); visited[b.team].add(key);
    }
  }
  if (e.type==='tileChange' && !e.hasPearl) removals.push(e.tile.x+','+e.tile.y);
  if (e.type==='dragonUpdate' && e.id===actor) moved.add(e.head.x+','+e.head.y);
  if (e.type==='dragonSplit') {
    const s=stats[e.team]; teams.set(e.childId,e.team);
    s.splits++; if(round<earlyEnd) s.early_splits++;
    if(s.first_split_round===null) s.first_split_round=round+1;
    history.delete(e.parentId); history.delete(e.childId);
  }
  if (e.type==='dragonDeath') {
    const s=stats[teams.get(e.id)];
    s.deaths[e.reason]=(s.deaths[e.reason]||0)+1;
    if(round<earlyEnd) s.early_deaths++;
    history.delete(e.id);
  }
  bodies.applyEvent(e);
  if(e.type==='dragonSplit'||e.type==='dragonDeath') peak();
}
flush();
for(const completed of [25,50,75]) {
  const state=d.match.roundAt(Math.min(completed,d.match.maxRound));
  for(const t of ['A','B']) stats[t].population[completed]=
    [...state.bodies.dragons.values()].filter(b=>b.team===t).length;
}
for(const t of ['A','B']) {
  const s=stats[t]; s.early_unique_head_tiles=visited[t].size;
  s.early_repeat_rate=s.early_turns?s.early_repeat_turns/s.early_turns:0;
}
console.log(JSON.stringify({rounds:d.match.maxRound,result:d.result,teams:stats}));
"""


def source_files(project: Path):
    return sorted(p for p in project.rglob("*") if p.is_file()
                  and not any(s.startswith(".") for s in p.relative_to(project).parts))


def fingerprint(project: Path) -> str:
    h = hashlib.sha256()
    for p in source_files(project):
        h.update(p.relative_to(project).as_posix().encode() + b"\0" + p.read_bytes() + b"\0")
    return h.hexdigest()


def prepare_decoder(out: Path) -> tuple[Path, Path]:
    import unswbc
    if importlib.metadata.version("unswbc") != TOOLKIT:
        raise RuntimeError(f"This replay evaluator requires inspected unswbc {TOOLKIT}; no upgrade is performed")
    if shutil.which("node") is None:
        raise RuntimeError("Node is needed to read the toolkit's authoritative replays")
    with zipfile.ZipFile(Path(unswbc.__file__).parent / "replay-viewer.vsix") as archive:
        code = archive.read("extension/dist/webview/webview.js").decode()
    marker = 'var Zf=V(`<canvas class="minimap'
    if marker not in code or 'function Xf(e){let t=Jf(e);return Yf(' not in code:
        raise RuntimeError("Viewer decoder changed; inspect the new parser before evaluation")
    decoder, metrics = out / "decoder.cjs", out / "replay_metrics.cjs"
    decoder.write_text(code[:code.index(marker)] + ";globalThis.decodeBattlecode=Xf;})();\n")
    metrics.write_text(METRICS_JS)
    subprocess.run(["node", "-e", "require(process.argv[1]); if(typeof decodeBattlecode!=='function') process.exit(1)",
                    str(decoder)], check=True, capture_output=True, text=True)
    return decoder, metrics


def play(task):
    from unswbc.bot import Bot, Pool
    from unswbc.engine import DEBUG_ALL, DEBUG_LIMITS, EngineModule
    from unswbc.run import _resolve

    row = {k: task[k] for k in ("id", "map", "a", "b", "seed", "sandbox", "candidate_side")}
    row["status"] = "error"
    pools, live, teams = {}, {}, {}
    points = {"A": [], "B": []}
    errors, notices = [], []
    started = time.monotonic()
    try:
        bot_type = Bot
        if task["sandbox"]:
            from unswbc.sandbox import SandboxBot, WasmPool
            bot_type = SandboxBot
        for team in "AB":
            argv, directory, kind = _resolve(task["staged_" + team.lower()], task["sandbox"])
            if task["sandbox"] and kind != "wasm":
                raise RuntimeError("This evaluator accepts C/C++ or wasm sandbox bots only")
            maker = WasmPool if task["sandbox"] else Pool
            pools[team] = maker(argv, cwd=str(directory), key=f"{task['seed']:016x}-{team.lower()}")

        def spawn(did, init):
            team = next(l.split()[1] for l in init.decode().splitlines() if l.startswith("TEAM "))
            teams[did] = team
            live[did] = bot_type(pools[team], init=init, name=str(did))

        def reply(did, block):
            bot = live[did]
            response = bot.ask(block)
            if bot.error is not None:
                errors.append({"id": did, "team": teams[did],
                               "round": int(block.split(b"\n", 1)[0].split()[1]),
                               "error": str(bot.error)})
            metrics = getattr(bot, "live", None)
            if metrics and metrics[0]:
                points[teams[did]].append(metrics[0])
            return response

        def death(did, _round, _reason):
            bot = live.pop(did, None)
            if bot is not None:
                bot.stop()

        engine = EngineModule()
        result = engine.run(Path(task["map_absolute"]).read_bytes(), reply, death, spawn,
                            notices.append, DEBUG_ALL | (DEBUG_LIMITS if task["sandbox"] else 0), task["seed"])
        row.update(dataclasses.asdict(result))
        row["rounds"] = result.rounds + 1
        replay = Path(task["out"]) / "replays" / (task["id"] + ".replay")
        replay.parent.mkdir(parents=True, exist_ok=True)
        replay.write_bytes(engine.replay(task["a"], task["b"]))
        decoded = subprocess.run(["node", task["metrics"], task["decoder"], str(replay)],
                                 check=True, capture_output=True, text=True)
        metrics = json.loads(decoded.stdout)
        expected_reason = "teamEliminated" if result.end_reason == 0 else "roundLimit"
        if metrics["rounds"] != row["rounds"] or metrics["result"]["winner"] != result.winner:
            raise RuntimeError("Engine/replay round or winner disagreement")
        if metrics["result"]["endReason"] != expected_reason:
            raise RuntimeError("Engine/replay end reason disagreement")
        for team in "AB":
            end = metrics["result"]["team" + team]
            for key, field in (("dragonCount", "dragons"), ("longestDragon", "longest"),
                               ("totalLength", "length"), ("queenLength", "queen")):
                if end[key] != getattr(result, team.lower() + "_" + field):
                    raise RuntimeError(f"Engine/replay final {team} {field} disagreement")
            vals = sorted(points[team])
            metrics["teams"][team]["points_max"] = max(vals, default=None)
            metrics["teams"][team]["points_p99"] = vals[max(0, (99 * len(vals) + 99) // 100 - 1)] if vals else None
        row.update(status="ok", metrics=metrics["teams"], runtime_errors=errors,
                   notices=notices, replay=str(replay), elimination=result.end_reason == 0)
    except Exception as exc:
        row.update(error=f"{type(exc).__name__}: {exc}", trace=traceback.format_exc()[-3000:])
        if isinstance(exc, subprocess.CalledProcessError):
            row["subprocess_stderr"] = exc.stderr
    finally:
        for bot in live.values():
            bot.stop()
        for pool in pools.values():
            pool.close()
    row["seconds"] = round(time.monotonic() - started, 2)
    return row


def write_report(out: Path, rows: list[dict], sandbox: bool):
    good = [r for r in rows if r["status"] == "ok"]
    summary = dict(games=len(good), errors=len(rows) - len(good), wins=0, draws=0,
                   losses=0, elimination_wins=0, elimination_losses=0, runtime_errors=0,
                   population={str(n): {"candidate": 0, "baseline": 0} for n in SNAPSHOTS})
    totals = {who: {key: 0 for key in ("early_splits", "early_pearls", "early_deaths", "early_turns", "early_repeat_turns")}
              for who in ("candidate", "baseline")}
    max_points = {"candidate": None, "baseline": None}
    table = []
    for r in good:
        us = r["candidate_side"]
        them = "B" if us == "A" else "A"
        verdict = "draws" if r["winner"] is None else "wins" if r["winner"] == us else "losses"
        summary[verdict] += 1
        if r["elimination"] and verdict != "draws":
            summary["elimination_" + verdict] += 1
        summary["runtime_errors"] += len(r["runtime_errors"])
        for who, team in (("candidate", us), ("baseline", them)):
            m = r["metrics"][team]
            for n in SNAPSHOTS:
                summary["population"][str(n)][who] += m["population"][str(n)]
            for key in totals[who]:
                totals[who][key] += m[key]
            if m["points_max"] is not None:
                max_points[who] = max(max_points[who] or 0, m["points_max"])
        c, b = r["metrics"][us], r["metrics"][them]
        pop = " / ".join(f"{c['population'][str(n)]}:{b['population'][str(n)]}" for n in SNAPSHOTS)
        label = {"wins": "win", "draws": "draw", "losses": "loss"}[verdict]
        table.append(f"| {Path(r['map']).stem} | {r['seed']} | {us} | {label} | {r['rounds']} | "
                     f"{'elimination' if r['elimination'] else 'queen / longest / total'} | {pop} | "
                     f"{c['early_splits']}:{b['early_splits']} | {c['early_pearls']}:{b['early_pearls']} | "
                     f"{c['early_repeat_rate']:.1%}:{b['early_repeat_rate']:.1%} |")
    for n in SNAPSHOTS:
        for who in totals:
            summary["population"][str(n)][who] /= max(1, len(good))
    for who in totals:
        totals[who]["early_repeat_rate"] = totals[who]["early_repeat_turns"] / max(1, totals[who]["early_turns"])
    summary.update(totals=totals, max_turn_points=max_points)
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    report = ["# v22.1 paired opening evaluation", "",
              f"Engine: unswbc {TOOLKIT}. Mode: {'judge sandbox' if sandbox else 'native screening'}. "
              f"{summary['games']} valid games; {summary['errors']} evaluation errors.", "",
              f"Candidate: {summary['wins']} wins, {summary['draws']} draws, {summary['losses']} losses. "
              f"Elimination: {summary['elimination_wins']} wins, {summary['elimination_losses']} losses. "
              f"Bot runtime errors (both teams): {summary['runtime_errors']}.", "",
              "Population is measured after 25 / 50 / 75 completed rounds. Matches ending earlier retain "
              "their terminal population. Early means the first 75 rounds; splits are successful replay "
              "events and pearls are removed pearls matched to that actor's actual head movements.", "",
              "Loop proxy: a turn starts on a head tile visited within the previous 12 rounds, with no "
              "pearl consumed or successful split since the earlier visit. It can include necessary survival "
              "maneuvers; it does not by itself prove an unwanted region loop.", "",
              "Native runs use wall-clock search cutoffs and are screening evidence. Sandbox runs use the "
              "judge's deterministic point clock and enforce its 100,000,000-point turn budget. Small paired "
              "samples do not establish a statistically reliable win-rate improvement.", "",
              "All row metrics below are candidate:baseline.", "",
              "| Map | Seed | Candidate side | Result | Rounds | End reason | Population 25 / 50 / 75 | Early splits | Early pearls | Loop proxy |",
              "|---|---:|:---:|---|---:|---|---|---|---|---|", *table, ""]
    for row in rows:
        if row["status"] != "ok":
            report.append(f"Evaluation error ({row['id']}): {row.get('error')}")
    (out / "report.md").write_text("\n".join(report))
    return summary


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--a", required=True, help="candidate project")
    ap.add_argument("--b", required=True, help="baseline project")
    ap.add_argument("--maps", nargs="+", required=True)
    ap.add_argument("--seeds", nargs="+", type=int, required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--workers", type=int, default=2)
    ap.add_argument("--sandbox", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--build-only", action="store_true", help="prepare parser and compile both bots without matches")
    args = ap.parse_args()
    if args.workers < 1 or any(s < 0 or s >= 1 << 64 for s in args.seeds):
        ap.error("workers must be positive and seeds must be unsigned 64-bit integers")
    projects = {p: (ROOT / p).resolve() for p in (args.a, args.b)}
    if projects[args.a] == projects[args.b]:
        ap.error("candidate and baseline must be different projects")
    for p in [*projects.values(), *(ROOT / m for m in args.maps)]:
        if not p.exists():
            ap.error(f"missing path: {p}")
    manifest = dict(schema=1, candidate=args.a, baseline=args.b, maps=args.maps, seeds=args.seeds,
                    sandbox=args.sandbox, toolkit=importlib.metadata.version("unswbc"),
                    fingerprints={p: fingerprint(d) for p, d in projects.items()},
                    maps_sha256={m: hashlib.sha256((ROOT / m).read_bytes()).hexdigest() for m in args.maps},
                    harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
    pairs = [(m, s, a, b) for m in args.maps for s in args.seeds for a, b in ((args.a, args.b), (args.b, args.a))]
    print(f"{len(pairs)} games: {len(args.maps)} maps x {len(args.seeds)} seeds x both sides", flush=True)
    if args.dry_run:
        print(json.dumps(manifest, indent=2))
        return 0
    out = (ROOT / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    manifest_path = out / "manifest.json"
    if manifest_path.exists() and json.loads(manifest_path.read_text()) != manifest:
        ap.error("output manifest differs; choose a new --out directory")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    # Both native build products and wasm cache remain in writable task/temp paths.
    os.environ.setdefault("XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "unswbc-v221-cache"))
    os.environ.setdefault("UNSWBC_WARM", "1")
    with tempfile.TemporaryDirectory(prefix="unswbc-v221-sources-") as td:
        decoder, metrics = prepare_decoder(Path(td))
        staged = {}
        from unswbc.run import _resolve
        for name, project in projects.items():
            dest = Path(td) / (project.name + "-" + manifest["fingerprints"][name][:16])
            for p in source_files(project):
                target = dest / p.relative_to(project)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(p, target)
            staged[name] = str(dest)
            _resolve(str(dest), args.sandbox)  # compile once before parallel workers
        if args.build_only:
            print("Both source snapshots compiled; replay decoder loaded successfully.")
            return 0
        ledger = out / "matches.jsonl"
        old = [json.loads(line) for line in ledger.read_text().splitlines()] if ledger.exists() else []
        rows_by_id = {r["id"]: r for r in old}
        tasks = []
        for m, seed, a, b in pairs:
            ident = hashlib.sha256(json.dumps([m, seed, a, b, manifest], sort_keys=True).encode()).hexdigest()[:20]
            if rows_by_id.get(ident, {}).get("status") == "ok":
                continue
            tasks.append(dict(id=ident, map=m, map_absolute=str((ROOT / m).resolve()), a=a, b=b, seed=seed,
                              candidate_side="A" if a == args.a else "B", sandbox=args.sandbox,
                              staged_a=staged[a], staged_b=staged[b], out=str(out), decoder=str(decoder), metrics=str(metrics)))
        with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers) as executor, ledger.open("a") as fh:
            futures = [executor.submit(play, task) for task in tasks]
            for i, future in enumerate(concurrent.futures.as_completed(futures), 1):
                row = future.result()
                fh.write(json.dumps(row) + "\n")
                fh.flush()
                rows_by_id[row["id"]] = row
                write_report(out, list(rows_by_id.values()), args.sandbox)
                result = "ERROR" if row["status"] != "ok" else "D" if row["winner"] is None else "W" if row["winner"] == row["candidate_side"] else "L"
                print(f"{i}/{len(tasks)} {Path(row['map']).stem} seed={row['seed']} side={row['candidate_side']} "
                      f"{result} rounds={row.get('rounds')} {row['seconds']}s {row.get('error','')}", flush=True)
        summary = write_report(out, list(rows_by_id.values()), args.sandbox)
        print(json.dumps(summary, indent=2))
        return 1 if summary["errors"] or summary["runtime_errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
