# v22.1 paired opening evaluation

Engine: unswbc 1.2.9. Mode: judge sandbox. 14 valid games; 0 evaluation errors.

Candidate: 6 wins, 0 draws, 8 losses. Elimination: 2 wins, 3 losses. Bot runtime errors (both teams): 0.

Population is measured after 25 / 50 / 75 completed rounds. Matches ending earlier retain their terminal population. Early means the first 75 rounds; splits are successful replay events and pearls are removed pearls matched to that actor's actual head movements.

Loop proxy: a turn starts on a head tile visited within the previous 12 rounds, with no pearl consumed or successful split since the earlier visit. It can include necessary survival maneuvers; it does not by itself prove an unwanted region loop.

Native runs use wall-clock search cutoffs and are screening evidence. Sandbox runs use the judge's deterministic point clock and enforce its 100,000,000-point turn budget. Small paired samples do not establish a statistically reliable win-rate improvement.

All row metrics below are candidate:baseline.

| Map | Seed | Candidate side | Result | Rounds | End reason | Population 25 / 50 / 75 | Early splits | Early pearls | Loop proxy |
|---|---:|:---:|---|---:|---|---|---|---|---|
| default_small | 22101 | B | loss | 314 | elimination | 4:3 / 9:8 / 8:9 | 13:16 | 33:45 | 18.5%:13.7% |
| arena | 22101 | A | win | 56 | elimination | 16:10 / 22:4 / 29:0 | 55:21 | 149:53 | 13.3%:14.4% |
| arena | 22101 | B | loss | 60 | elimination | 9:17 / 6:19 / 0:25 | 25:77 | 70:201 | 9.3%:10.7% |
| default_small | 22101 | A | win | 500 | queen / longest / total | 5:4 / 10:10 / 8:10 | 16:20 | 43:48 | 13.6%:13.1% |
| trophy | 22101 | A | win | 58 | elimination | 9:4 / 23:5 / 27:0 | 31:4 | 78:13 | 7.1%:11.1% |
| trophy | 22101 | B | loss | 280 | elimination | 8:5 / 12:13 / 13:16 | 26:28 | 63:71 | 6.0%:10.2% |
| portals | 22101 | A | loss | 500 | queen / longest / total | 7:5 / 12:4 / 14:9 | 39:26 | 95:56 | 18.4%:17.4% |
| portals | 22101 | B | win | 500 | queen / longest / total | 6:7 / 10:10 / 14:10 | 39:28 | 89:71 | 19.2%:18.0% |
| default | 22101 | A | loss | 500 | queen / longest / total | 9:9 / 10:15 / 5:20 | 9:24 | 16:52 | 13.2%:15.0% |
| default | 22101 | B | win | 500 | queen / longest / total | 9:9 / 14:11 / 10:12 | 14:16 | 27:28 | 13.4%:11.2% |
| stronghold | 22101 | A | loss | 500 | queen / longest / total | 6:9 / 16:13 / 22:16 | 37:16 | 83:40 | 6.2%:8.3% |
| stronghold | 22101 | B | loss | 500 | queen / longest / total | 9:9 / 13:17 / 17:20 | 20:32 | 49:70 | 9.3%:7.2% |
| big_empty | 22101 | A | win | 500 | queen / longest / total | 20:17 / 54:53 / 53:40 | 109:96 | 265:237 | 2.7%:1.5% |
| big_empty | 22101 | B | loss | 500 | queen / longest / total | 18:16 / 52:49 / 49:59 | 98:108 | 242:255 | 2.1%:2.2% |
