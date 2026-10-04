# v22.1 paired opening evaluation

Engine: unswbc 1.2.9. Mode: judge sandbox. 2 valid games; 0 evaluation errors.

Candidate: 1 wins, 0 draws, 1 losses. Elimination: 0 wins, 1 losses. Bot runtime errors (both teams): 0.

Population is measured after 25 / 50 / 75 completed rounds. Matches ending earlier retain their terminal population. Early means the first 75 rounds; splits are successful replay events and pearls are removed pearls matched to that actor's actual head movements.

Loop proxy: a turn starts on a head tile visited within the previous 12 rounds, with no pearl consumed or successful split since the earlier visit. It can include necessary survival maneuvers; it does not by itself prove an unwanted region loop.

Native runs use wall-clock search cutoffs and are screening evidence. Sandbox runs use the judge's deterministic point clock and enforce its 100,000,000-point turn budget. Small paired samples do not establish a statistically reliable win-rate improvement.

All row metrics below are candidate:baseline.

| Map | Seed | Candidate side | Result | Rounds | End reason | Population 25 / 50 / 75 | Early splits | Early pearls | Loop proxy |
|---|---:|:---:|---|---:|---|---|---|---|---|
| default_small | 22101 | A | loss | 172 | elimination | 5:4 / 11:9 / 7:8 | 18:18 | 50:46 | 11.8%:16.4% |
| default_small | 22101 | B | win | 500 | queen / longest / total | 4:3 / 12:6 / 10:8 | 15:15 | 44:39 | 13.7%:17.1% |
