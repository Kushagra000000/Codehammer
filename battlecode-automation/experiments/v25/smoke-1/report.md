# v22.1 paired opening evaluation

Engine: unswbc 1.2.9. Mode: judge sandbox. 6 valid games; 0 evaluation errors.

Candidate: 3 wins, 0 draws, 3 losses. Elimination: 2 wins, 3 losses. Bot runtime errors (both teams): 0.

Population is measured after 25 / 50 / 75 completed rounds. Matches ending earlier retain their terminal population. Early means the first 75 rounds; splits are successful replay events and pearls are removed pearls matched to that actor's actual head movements.

Loop proxy: a turn starts on a head tile visited within the previous 12 rounds, with no pearl consumed or successful split since the earlier visit. It can include necessary survival maneuvers; it does not by itself prove an unwanted region loop.

Native runs use wall-clock search cutoffs and are screening evidence. Sandbox runs use the judge's deterministic point clock and enforce its 100,000,000-point turn budget. Small paired samples do not establish a statistically reliable win-rate improvement.

All row metrics below are candidate:baseline.

| Map | Seed | Candidate side | Result | Rounds | End reason | Population 25 / 50 / 75 | Early splits | Early pearls | Loop proxy |
|---|---:|:---:|---|---:|---|---|---|---|---|
| default_small | 25001 | B | loss | 116 | elimination | 3:3 / 6:9 / 4:17 | 8:24 | 18:56 | 15.6%:18.7% |
| default_small | 25001 | A | loss | 146 | elimination | 4:2 / 7:8 / 4:12 | 11:22 | 33:55 | 15.2%:12.7% |
| trophy | 25001 | A | win | 52 | elimination | 6:3 / 18:1 / 18:0 | 22:1 | 50:7 | 2.4%:8.3% |
| trophy | 25001 | B | win | 70 | elimination | 8:4 / 9:5 / 25:0 | 29:4 | 74:13 | 6.8%:9.9% |
| default | 25001 | A | loss | 152 | elimination | 6:8 / 8:10 / 5:15 | 11:21 | 24:42 | 7.4%:5.3% |
| default | 25001 | B | win | 500 | queen / longest / total | 8:8 / 9:9 / 12:12 | 12:15 | 25:28 | 3.2%:4.8% |
