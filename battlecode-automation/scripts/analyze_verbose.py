#!/usr/bin/env python3
"""Summarise an `unswbc run -v` log (e.g. analysis.txt) for one team.

python3 analyze_verbose.py analysis.txt --team B [--start-line N]
Reports: result, deaths by team/reason, enemy deaths caused by our moves (the
actor is the dragon whose output precedes the death line) with the intent it
logged, v12.x behaviour logs (DENIAL_SPRINT, ASSIST_KILL, ATTACK_SPRINT,
TURN_RULE), turn-rule compliance before round 250 (reversals and same-side
turns less than 6 rounds apart, for both teams), and alive dragons per round.
"""
import argparse, collections, re

HDR = re.compile(r"^round (\d+): bot (\d+) \(team ([AB])\) stdout:$")
DIE = re.compile(r"^round (\d+): bot (\d+) \(team ([AB])\) died: (.*)$")
LEFT = {"N": "W", "W": "S", "S": "E", "E": "N"}
RIGHT = {v: k for k, v in LEFT.items()}
OPP = {"N": "S", "S": "N", "E": "W", "W": "E"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--team", default="B")
    ap.add_argument("--start-line", type=int, default=0, help="skip earlier runs appended to the same file")
    ap.add_argument("--explore-end", type=int, default=250)
    ap.add_argument("--gap", type=int, default=6)
    a = ap.parse_args()
    us = a.team
    them = "A" if us == "B" else "B"
    lines = open(a.log, errors="replace").read().split("\n")[a.start_line:]
    team = {}
    cur = None  # (round, id, team, logs, move)
    actor = None
    deaths = []
    logs = collections.Counter()
    moves = collections.defaultdict(list)  # id -> [(round, steps)]
    kills = collections.Counter()
    result = None
    born = {}
    for ln in lines:
        m = HDR.match(ln)
        if m:
            r, i, t = int(m[1]), int(m[2]), m[3]
            team[i] = t
            born.setdefault(i, r)
            cur = dict(round=r, id=i, team=t, logs=[], move=None)
            actor = cur
            continue
        m = DIE.match(ln)
        if m:
            r, i, t, reason = int(m[1]), int(m[2]), m[3], m[4]
            team[i] = t
            act = actor if actor and actor["round"] == r else None
            deaths.append(dict(round=r, id=i, team=t, reason=reason,
                               actor=act["id"] if act else None, actor_team=act["team"] if act else None,
                               actor_logs=list(act["logs"]) if act else []))
            cur = None
            continue
        if ln.startswith(("team A wins", "team B wins", "draw after")):
            result = ln
        if cur is None:
            continue
        if ln.startswith("LOG "):
            k = ln[4:].split(" ")[0]
            cur["logs"].append(k)
            logs[(cur["team"], k)] += 1
        elif ln.startswith("MOVE "):
            cur["move"] = ln.split()[1]
            moves[cur["id"]].append((cur["round"], cur["move"]))
        elif ln.startswith("SPLIT"):
            moves[cur["id"]].append((cur["round"], None))

    print("RESULT:", result)
    print(f"dragons seen: us({us})={sum(1 for t in team.values() if t == us)} them({them})={sum(1 for t in team.values() if t == them)}")
    by = collections.Counter((d["team"], d["reason"]) for d in deaths)
    for t in (us, them):
        print(f"deaths team {t}: total {sum(v for (tt, _), v in by.items() if tt == t)} ",
              dict((r, v) for (tt, r), v in by.items() if tt == t))
    ek = [d for d in deaths if d["team"] == them and d["actor_team"] == us]
    print(f"\nENEMY deaths during OUR dragon's move (we caused them): {len(ek)}")
    print("  by our logged intent:", collections.Counter((d["reason"], next((l for l in d["actor_logs"] if l in ("ASSIST_KILL", "ATTACK_SPRINT", "DENIAL_SPRINT")), "plain_move")) for d in ek))
    self_enemy = [d for d in deaths if d["team"] == them and d["actor"] == d["id"]]
    print(f"ENEMY deaths on their own move: {len(self_enemy)}", collections.Counter(d["reason"] for d in self_enemy))
    ok = [d for d in deaths if d["team"] == us and d["actor_team"] == them]
    print(f"OUR deaths during an enemy move: {len(ok)}")
    for phase, lo, hi in (("early <250", 0, 250), ("mid 250-399", 250, 400), ("late >=400", 400, 501)):
        e = sum(1 for d in deaths if d["team"] == them and lo <= d["round"] < hi)
        o = sum(1 for d in deaths if d["team"] == us and lo <= d["round"] < hi)
        print(f"  {phase}: enemy deaths {e}, our deaths {o}")
    print("\nBehaviour logs (count per team):")
    for k in ("DENIAL_SPRINT", "ASSIST_KILL", "ATTACK_SPRINT", "TURN_RULE", "FEED", "GROWER_REVERSE", "WORKER_REVERSE", "TRAPPED_SELF", "REMNANT_CLEAR"):
        print(f"  {k:15s} us {logs[(us, k)]:5d}   them {logs[(them, k)]:5d}")
    # Turn rule: consecutive single-step moves of one dragon.
    for t in (us, them):
        pairs = rev = close_same = turns = 0
        for i, mv in moves.items():
            if team.get(i) != t:
                continue
            prev_dir = None
            last_side, last_round = 0, -1000
            prev_round = None
            for r, steps in mv:
                if steps is None or r >= a.explore_end:
                    prev_dir = None
                    continue
                if prev_round is not None and r != prev_round + 1:
                    prev_dir = None
                for s in steps:
                    if prev_dir is not None:
                        pairs += 1
                        if s == OPP[prev_dir]:
                            rev += 1
                        side = -1 if s == LEFT[prev_dir] else 1 if s == RIGHT[prev_dir] else 0
                        if side:
                            turns += 1
                            if side == last_side and r - last_round < a.gap:
                                close_same += 1
                            last_side, last_round = side, r
                    prev_dir = s
                prev_round = r
        print(f"turn stats before round {a.explore_end}, team {t}: steps {pairs}, turns {turns}, "
              f"reversals {rev}, same-side turns <{a.gap} rounds apart {close_same} "
              f"({100 * close_same / max(1, turns):.1f}% of turns)")
    alive = collections.Counter()
    for rr in (50, 100, 150, 200, 250, 300, 400, 499):
        for t in (us, them):
            n = sum(1 for i, b in born.items() if team[i] == t and b <= rr) - sum(1 for d in deaths if d["team"] == t and d["round"] <= rr)
            alive[(rr, t)] = n
    print("\nalive (approx: dragons that had a turn by then minus deaths):")
    print("  " + "  ".join(f"r{rr}: {alive[(rr, us)]}/{alive[(rr, them)]}" for rr in (50, 100, 150, 200, 250, 300, 400, 499)), f"(us/them)")


if __name__ == "__main__":
    main()
