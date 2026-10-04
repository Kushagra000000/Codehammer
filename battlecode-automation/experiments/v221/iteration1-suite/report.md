# v22.1 paired opening evaluation

Engine: unswbc 1.2.9. Mode: judge sandbox. 12 valid games; 0 evaluation errors.

Candidate: 5 wins, 1 draws, 6 losses. Elimination: 1 wins, 3 losses. Bot runtime errors (both teams): 0.

Population is measured after 25 / 50 / 75 completed rounds. Matches ending earlier retain their terminal population. Early means the first 75 rounds; splits are successful replay events and pearls are removed pearls matched to that actor's actual head movements.

Loop proxy: a turn starts on a head tile visited within the previous 12 rounds, with no pearl consumed or successful split since the earlier visit. It can include necessary survival maneuvers; it does not by itself prove an unwanted region loop.

Native runs use wall-clock search cutoffs and are screening evidence. Sandbox runs use the judge's deterministic point clock and enforce its 100,000,000-point turn budget. Small paired samples do not establish a statistically reliable win-rate improvement.

All row metrics below are candidate:baseline.

| Map | Seed | Candidate side | Result | Rounds | End reason | Population 25 / 50 / 75 | Early splits | Early pearls | Loop proxy |
|---|---:|:---:|---|---:|---|---|---|---|---|
| arena | 22101 | A | win | 63 | elimination | 17:11 / 13:11 / 18:0 | 63:33 | 163:96 | 7.6%:12.9% |
| arena | 22101 | B | loss | 60 | elimination | 9:17 / 6:19 / 0:25 | 25:77 | 70:201 | 9.3%:10.7% |
| trophy | 22101 | A | loss | 132 | elimination | 6:5 / 10:14 / 12:21 | 20:30 | 44:74 | 4.4%:12.7% |
| trophy | 22101 | B | loss | 171 | elimination | 8:5 / 14:14 / 12:18 | 24:30 | 60:79 | 6.7%:8.7% |
| portals | 22101 | A | draw | 500 | queen / longest / total | 6:5 / 7:5 / 12:9 | 23:24 | 53:58 | 20.6%:10.8% |
| portals | 22101 | B | win | 500 | queen / longest / total | 7:7 / 12:9 / 18:8 | 32:27 | 76:66 | 16.0%:17.7% |
| default | 22101 | A | win | 500 | queen / longest / total | 9:9 / 11:13 / 13:14 | 14:16 | 26:33 | 13.5%:16.8% |
| default | 22101 | B | loss | 500 | queen / longest / total | 9:8 / 10:11 / 10:10 | 13:14 | 25:28 | 8.2%:9.6% |
| stronghold | 22101 | A | loss | 500 | queen / longest / total | 6:9 / 13:13 / 20:18 | 21:18 | 49:47 | 5.0%:9.0% |
| stronghold | 22101 | B | win | 500 | queen / longest / total | 8:9 / 14:16 / 20:25 | 18:34 | 40:75 | 4.7%:6.3% |
| big_empty | 22101 | B | loss | 500 | queen / longest / total | 19:16 / 54:50 / 54:56 | 102:104 | 235:255 | 1.4%:1.9% |
| big_empty | 22101 | A | win | 500 | queen / longest / total | 18:16 / 57:54 / 51:62 | 99:110 | 230:264 | 1.6%:2.7% |
