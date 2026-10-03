#!/usr/bin/env python3
"""Paired comparison of two candidates run on the identical (map, seed, side) schedule.
python3 v12_paired.py LEDGER_X CAND_X LEDGER_Y CAND_Y"""
import json, sys, pathlib
def load(led, cand):
    out = {}
    for l in pathlib.Path(led).read_text().splitlines():
        r = json.loads(l)
        if r["status"] != "ok": continue
        side = "A" if cand in r["a"] else "B"
        s = 1.0 if r["winner"] == side else (0.5 if r["winner"] is None else 0.0)
        out[(r["map"], r["seed"], side)] = (s, r[f"{side.lower()}_longest"])
    return out
x, y = load(sys.argv[1], sys.argv[2]), load(sys.argv[3], sys.argv[4])
keys = sorted(set(x) & set(y))
better = sum(1 for k in keys if x[k][0] > y[k][0]); worse = sum(1 for k in keys if x[k][0] < y[k][0])
print(f"{sys.argv[2]} vs {sys.argv[4]} on {len(keys)} shared games: X better in {better}, worse in {worse}, same {len(keys)-better-worse}")
print(f"score X {sum(x[k][0] for k in keys)/len(keys):.3f}  Y {sum(y[k][0] for k in keys)/len(keys):.3f}; "
      f"mean longest X {sum(x[k][1] for k in keys)/len(keys):.1f}  Y {sum(y[k][1] for k in keys)/len(keys):.1f}")
# exact two-sided sign test on discordant pairs
from math import comb
n = better + worse; k = min(better, worse)
p = min(1.0, 2 * sum(comb(n, i) for i in range(k + 1)) / 2 ** n) if n else 1.0
print(f"sign test on discordant pairs: p = {p:.3f}")
