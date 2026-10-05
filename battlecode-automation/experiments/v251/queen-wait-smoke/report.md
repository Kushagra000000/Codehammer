# v25.1.0 paired sandbox smoke evaluation

Engine: unswbc 1.2.9. Mode: judge sandbox. 6 valid games; 0 evaluation errors.

Candidate: 3 wins, 0 draws, 3 losses. Elimination: 0 wins, 0 losses. Bot runtime errors (both teams): 0.

Population is measured after 25 / 50 / 75 completed rounds. Matches ending earlier retain their terminal population. Early means the first 75 rounds; splits are successful replay events and pearls are removed pearls matched to that actor's actual head movements.

Loop proxy: a turn starts on a head tile visited within the previous 12 rounds, with no pearl consumed or successful split since the earlier visit. It can include necessary survival maneuvers; it does not by itself prove an unwanted region loop.

Native runs use wall-clock search cutoffs and are screening evidence. Sandbox runs use the judge's deterministic point clock and enforce its 100,000,000-point turn budget. Small paired samples do not establish a statistically reliable win-rate improvement.

All row metrics below are candidate:baseline.

| Map | Seed | Candidate side | Result | Rounds | End reason | Population 25 / 50 / 75 | Early splits | Early pearls | Loop proxy |
|---|---:|:---:|---|---:|---|---|---|---|---|
| default_small | 25101 | A | win | 500 | queen / longest / total | 3:1 / 12:5 / 15:8 | 26:10 | 68:25 | 13.2%:7.3% |
| default_small | 25101 | B | loss | 500 | queen / longest / total | 1:3 / 5:12 / 8:15 | 10:26 | 25:68 | 7.3%:13.2% |
| schooltime | 25101 | B | loss | 500 | queen / longest / total | 5:5 / 10:12 / 21:22 | 24:32 | 58:80 | 18.1%:20.2% |
| schooltime | 25101 | A | win | 500 | queen / longest / total | 5:5 / 12:10 / 22:21 | 32:24 | 80:58 | 20.2%:18.1% |
| slithery_fight | 25101 | A | loss | 500 | queen / longest / total | 35:37 / 50:53 / 59:59 | 127:138 | 294:316 | 9.1%:9.0% |
| slithery_fight | 25101 | B | win | 500 | queen / longest / total | 35:33 / 53:50 / 60:54 | 131:131 | 307:299 | 10.2%:9.1% |
