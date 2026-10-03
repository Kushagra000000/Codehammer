#!/usr/bin/env python3
"""Summarise v12_eval.py ledgers from the candidate's perspective.

python3 battlecode-automation/scripts/v12_report.py --cand bot-v12.0-kushagra LEDGER.jsonl [...]
Errors are listed separately and never counted as W/D/L.
"""
from __future__ import annotations

import argparse
import collections
import json
import math
import pathlib


def wilson(p, n, z=1.96):
    if n == 0:
        return (float("nan"), float("nan"))
    d = 1 + z * z / n
    c = p + z * z / (2 * n)
    r = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    return ((c - r) / d, (c + r) / d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cand", required=True, help="candidate project directory name")
    ap.add_argument("ledgers", nargs="+")
    ap.add_argument("--by-map", action="store_true")
    args = ap.parse_args()
    rows = []
    for l in args.ledgers:
        rows += [json.loads(x) for x in pathlib.Path(l).read_text().splitlines() if x.strip()]
    errors = [r for r in rows if r["status"] != "ok"]
    ok = [r for r in rows if r["status"] == "ok" and args.cand in (pathlib.Path(r["a"]).name, pathlib.Path(r["b"]).name)]
    per = collections.defaultdict(lambda: collections.Counter())
    per_map = collections.defaultdict(lambda: collections.Counter())
    agg = collections.defaultdict(lambda: collections.defaultdict(list))
    deaths = collections.defaultdict(collections.Counter)
    death_len = collections.defaultdict(list)
    esc = collections.defaultdict(lambda: [0, 0])
    feeds = collections.defaultdict(lambda: [0, 0, 0, 0])  # n, drops, gain, gain_known_n
    portal = collections.defaultdict(lambda: [0, 0])
    pts = collections.defaultdict(list)
    for r in rows:
        if r["status"] != "ok":
            continue
        for side in "AB":
            me = pathlib.Path(r[side.lower()]).name
            opp = pathlib.Path(r["b" if side == "A" else "a"]).name
            key = f"{me} vs {opp}"
            res = "D" if r["winner"] is None else ("W" if r["winner"] == side else "L")
            per[key][res] += 1
            per_map[(key, pathlib.Path(r["map"]).stem)][res] += 1
            o = "b" if side == "A" else "a"
            s = side.lower()
            agg[key]["longest"].append(r[f"{s}_longest"])
            agg[key]["opp_longest"].append(r[f"{o}_longest"])
            agg[key]["total"].append(r[f"{s}_total"])
            agg[key]["eliminated"].append(1 if r[f"{s}_dragons"] == 0 else 0)
            agg[key]["eliminated_opp"].append(1 if r[f"{o}_dragons"] == 0 else 0)
            for d in r["deaths"]:
                if d["team"] == side:
                    deaths[me][(d["reason"], d["intent"] or "-")] += 1
                    if (d["length"] or 0) >= 12 and d["intent"] not in ("REMNANT_CLEAR", "FEED"):
                        death_len[me].append((pathlib.Path(r["map"]).stem, r["seed"], d["round"], d["length"], d["reason"], d["intent"]))
                    # Portal landing deaths: unintended (no logged intent) collisions on
                    # a turn whose move crossed a portal inside the 7x7 window.
                    if d["portal"] and d["intent"] is None and d["reason"] in ("O", "H", "W"):
                        portal[me][1] += 1
            portal[me][0] += r["portal_moves"][side]
            for c in r["children"]:
                if c["team"] == side and c["kind"] in ("GROWER_REVERSE", "WORKER_REVERSE"):
                    esc[(me, c["kind"])][0] += 1
                    if c["died"] is None or c["died"] > c["birth"]:
                        esc[(me, c["kind"])][1] += 1
            for f in r["feeds"]:
                if f["team"] == side:
                    x = feeds[me]
                    x[0] += 1
                    x[1] += f["drops"]
                    if f["recipient_gain8"] is not None:
                        x[2] += f["recipient_gain8"]
                        x[3] += 1
            if r["points_max"][side]:
                pts[me].append(r["points_max"][side])
    print("## Head-to-head (rows from each bot's perspective)\n")
    print("| Pairing | N | W | D | L | Score | 95% Wilson (score) | mean longest | opp longest | mean total | eliminated | eliminated opp |")
    print("|---|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---:|")
    for key in sorted(per):
        if not key.startswith(args.cand):
            continue
        c = per[key]
        n = c["W"] + c["D"] + c["L"]
        sc = (c["W"] + c["D"] / 2) / n
        lo, hi = wilson(sc, n)
        a = agg[key]
        m = lambda v: sum(v) / len(v)
        print(f"| {key} | {n} | {c['W']} | {c['D']} | {c['L']} | {sc:.3f} | {lo:.3f}–{hi:.3f} | {m(a['longest']):.1f} | "
              f"{m(a['opp_longest']):.1f} | {m(a['total']):.1f} | {sum(a['eliminated'])} | {sum(a['eliminated_opp'])} |")
    if args.by_map:
        print("\n## Per map (candidate perspective)\n\n| Pairing | Map | W | D | L |\n|---|---|---:|---:|---:|")
        for (key, mp), c in sorted(per_map.items()):
            if key.startswith(args.cand):
                print(f"| {key} | {mp} | {c['W']} | {c['D']} | {c['L']} |")
    print("\n## Deaths by (reason, logged intent), all games in these ledgers\n")
    for me, c in sorted(deaths.items()):
        print(f"- {me}: " + ", ".join(f"{k[0]}/{k[1]}={v}" for k, v in c.most_common()))
    print("\n## Unintended deaths at length >= 12 (excluding FEED / REMNANT_CLEAR)\n")
    for me, v in sorted(death_len.items()):
        print(f"- {me}: {len(v)}  e.g. {v[:6]}")
    print("\n## Escape children outliving birth round\n")
    for (me, kind), (n, s) in sorted(esc.items()):
        print(f"- {me} {kind}: {s}/{n}")
    print("\n## FEED events: count, drops, recipient 8-round growth proxy (confounded upper bound)\n")
    for me, (n, drops, gain, kn) in sorted(feeds.items()):
        print(f"- {me}: feeds={n} drops={drops} recipient_gain8_sum={gain} over {kn} feeds "
              f"({(gain / drops if drops else float('nan')):.2f} per dropped pearl)")
    print("\n## Portal traversals seen in-window and UNINTENDED deaths (O/H/W, no logged intent) on those turns\n")
    for me, (m, d) in sorted(portal.items()):
        print(f"- {me}: traversals={m} unintended_deaths_on_traversal_turn={d} ({(d / m if m else 0):.4f})")
    if pts:
        print("\n## Sandbox points per turn (max over games)\n")
        for me, v in sorted(pts.items()):
            print(f"- {me}: max={max(v)} median-of-game-max={sorted(v)[len(v) // 2]}")
    print(f"\nErrors: {len(errors)}")
    for e in errors:
        print("-", e["map"], e["a"], e["b"], e["seed"], e.get("error"))


if __name__ == "__main__":
    main()
