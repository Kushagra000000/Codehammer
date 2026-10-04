#!/usr/bin/env python3
"""Mirror-pair analysis vs an opponent that is a byte-identical-behaviour baseline in mirror.
For each (map, seed) the candidate plays both sides. A deterministic mirror match of the
baseline against itself always splits 1-1, so a (W,W) pair is a gain, (L,L) a loss.
python3 v12_pairs.py LEDGER CAND"""
import json, sys, collections
from math import comb
rows = [json.loads(l) for l in open(sys.argv[1])]
cand = sys.argv[2]
pairs = collections.defaultdict(dict)
for r in rows:
    if r["status"] != "ok": continue
    side = "A" if cand in r["a"] else "B"
    s = 1.0 if r["winner"] == side else (0.5 if r["winner"] is None else 0.0)
    pairs[(r["map"], r["seed"])][side] = s
gain = sum(1 for v in pairs.values() if len(v) == 2 and v["A"] + v["B"] > 1)
loss = sum(1 for v in pairs.values() if len(v) == 2 and v["A"] + v["B"] < 1)
even = sum(1 for v in pairs.values() if len(v) == 2 and v["A"] + v["B"] == 1)
n = gain + loss; k = min(gain, loss)
p = min(1.0, 2 * sum(comb(n, i) for i in range(k + 1)) / 2 ** n) if n else 1.0
print(f"{cand}: pairs={len(pairs)} gain(both sides won)={gain} loss(both lost)={loss} split={even}  sign-test p={p:.3f}")
