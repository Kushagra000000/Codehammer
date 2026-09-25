#!/usr/bin/env python3
"""Write a spreadsheet-friendly CSV snapshot of the current ladder."""
from pathlib import Path
import csv
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import autorun_matches as ladder

bots = ladder.discover_bots()
rows = ladder.read_jsonl(ladder.MATCHES_PATH)
config = ladder.load_config()
ratings, _, stats = ladder.ratings_from_matches(rows, config)
out = ladder.DATA_ROOT / "ladder.csv"
fields = ["rank","label","uid","rating","games","wins","losses","draws","win_rate","path"]
ranked = sorted(bots, key=lambda b:(ratings.get(b.uid,config.get('initial_elo',1000)),stats.get(b.uid,{}).get('games',0)), reverse=True)
with out.open("w", newline="", encoding="utf-8") as f:
    w=csv.DictWriter(f, fieldnames=fields)
    w.writeheader()
    for i,b in enumerate(ranked,1):
        s=stats.get(b.uid,{})
        w.writerow({"rank":i,"label":b.label,"uid":b.uid,"rating":round(ratings.get(b.uid,1000),2),"games":s.get("games",0),"wins":s.get("wins",0),"losses":s.get("losses",0),"draws":s.get("draws",0),"win_rate":s.get("win_rate",0),"path":b.path})
print(out)
