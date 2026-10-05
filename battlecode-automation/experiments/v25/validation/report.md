# v22.1 paired opening evaluation

Engine: unswbc 1.2.9. Mode: judge sandbox. 16 valid games; 0 evaluation errors.

Candidate: 7 wins, 0 draws, 9 losses. Elimination: 2 wins, 5 losses. Bot runtime errors (both teams): 0.

Population is measured after 25 / 50 / 75 completed rounds. Matches ending earlier retain their terminal population. Early means the first 75 rounds; splits are successful replay events and pearls are removed pearls matched to that actor's actual head movements.

Loop proxy: a turn starts on a head tile visited within the previous 12 rounds, with no pearl consumed or successful split since the earlier visit. It can include necessary survival maneuvers; it does not by itself prove an unwanted region loop.

Native runs use wall-clock search cutoffs and are screening evidence. Sandbox runs use the judge's deterministic point clock and enforce its 100,000,000-point turn budget. Small paired samples do not establish a statistically reliable win-rate improvement.

All row metrics below are candidate:baseline.

| Map | Seed | Candidate side | Result | Rounds | End reason | Population 25 / 50 / 75 | Early splits | Early pearls | Loop proxy |
|---|---:|:---:|---|---:|---|---|---|---|---|
| default_small | 25002 | A | win | 288 | elimination | 5:2 / 13:5 / 13:5 | 20:7 | 55:18 | 14.6%:21.8% |
| trophy | 25002 | A | loss | 119 | elimination | 5:3 / 11:14 / 4:19 | 12:30 | 30:74 | 3.8%:8.0% |
| default_small | 25002 | B | loss | 459 | elimination | 3:4 / 7:9 / 6:14 | 9:19 | 25:46 | 14.5%:19.5% |
| trophy | 25002 | B | win | 100 | elimination | 4:3 / 14:8 / 25:5 | 36:15 | 85:38 | 4.1%:4.7% |
| default | 25002 | A | loss | 196 | elimination | 8:10 / 9:13 / 4:12 | 10:27 | 21:61 | 5.1%:7.2% |
| arena | 25002 | A | loss | 72 | elimination | 9:13 / 8:13 / 0:19 | 32:89 | 96:237 | 10.9%:4.8% |
| arena | 25002 | B | loss | 67 | elimination | 13:8 / 10:18 / 0:15 | 46:67 | 133:183 | 5.2%:9.1% |
| default | 25002 | B | loss | 500 | queen / longest / total | 8:8 / 13:14 / 13:16 | 21:21 | 46:50 | 5.9%:8.0% |
| portals | 25002 | A | win | 500 | queen / longest / total | 6:5 / 6:7 / 15:6 | 32:21 | 74:56 | 10.2%:18.7% |
| portals | 25002 | B | win | 500 | queen / longest / total | 5:5 / 8:5 / 17:11 | 39:26 | 88:65 | 7.5%:8.6% |
| schooltime | 25002 | A | loss | 500 | queen / longest / total | 5:5 / 8:14 / 13:24 | 14:33 | 29:78 | 17.1%:13.0% |
| schooltime | 25002 | B | loss | 500 | queen / longest / total | 5:5 / 7:12 / 14:31 | 24:44 | 59:106 | 19.0%:13.1% |
| maze | 25002 | A | win | 500 | queen / longest / total | 7:6 / 12:12 / 19:17 | 25:19 | 55:45 | 6.1%:8.2% |
| maze | 25002 | B | win | 500 | queen / longest / total | 6:6 / 9:11 / 16:16 | 16:18 | 35:43 | 3.2%:8.3% |
| big_empty | 25002 | A | win | 500 | queen / longest / total | 17:12 / 43:41 / 55:47 | 103:95 | 245:226 | 2.1%:2.9% |
| big_empty | 25002 | B | loss | 500 | queen / longest / total | 18:15 / 50:48 / 61:53 | 109:101 | 243:237 | 3.3%:3.7% |
