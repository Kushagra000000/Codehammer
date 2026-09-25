# Research report — 2026-09-26

Current champion: **bot-v9**. Promotion status: accepted after 400-game research batch; exact EXP012 source copy. Best new screening candidate: **v9-exp012**, parent v9-exp001 / v8. 400 research matches recorded; final validation complete: True.

The change replaces toroidal Manhattan pearl-objective distance with BFS over remembered kelp and paired portals. Unknown ordinary edges remain optimistic. Existing memory, safety tiers, splitting, sonar, spacing and combat architecture are preserved. King, feeding and frontier additions are disabled in this candidate.

Parameters: split specialization 300, final growth cutoff 425, stable hash grower fraction 1/4, swarm target/max 64, pre-growth length 3, portal override probability 0%, portal rewards 250/35 (swarm/grower), unknown penalties 90/1200, exploration weights 6/2, visits 28/8, space penalties 1100/4800, enemy threats 260/1700, ally congestion 1600, direct trade max length 3, legacy sprint-trade ceiling 1000000, four sonar directions every turn. Reserved king/feeder controls are inactive, not implemented promises.

Candidate participated in 124 native games plus 4 sandbox checks; current-batch native Elo 1031.5 (separate from historical Elo). Against v5 in native games: {'games': 96, 'W': 90, 'D': 1, 'L': 5}; against v8: {'games': 14, 'W': 11, 'D': 0, 'L': 3}. Mean final longest: 9.44.

## Independent v5 validation

Both sides for seeds 303, 404, 505, 606. Threshold fixed before testing: at least eight games, Wilson lower bound >0.5, and a win on each side.

|Map|W/D/L|95% lower bound|Beaten|
|---|---|---:|---|
|Colosseum.map|8/0/0|0.676|True|
|arena.map|8/0/0|0.676|True|
|big_empty.map|8/0/0|0.676|True|
|default.map|8/0/0|0.676|True|
|default_small.map|8/0/0|0.676|True|
|devil.map|8/0/0|0.676|True|
|queen_of_spades.map|8/0/0|0.676|True|
|schooltime.map|7/0/1|0.529|True|
|stronghold.map|7/0/1|0.529|True|
|trauma.map|4/1/3|0.259|False|
|trophy.map|8/0/0|0.676|True|

Five-map requirement: **True**. Regression flags: none. Runtime/parse failures for candidate: 0.

## Resource validation

Sandbox games are diagnostic and excluded from strategy Elo/win-rate tables.

|Map|Candidate side|Maximum reported points|CPU limit events|Invalid actions|
|---|---|---:|---:|---:|
|big_empty.map|A|40400000.0|0|0|
|big_empty.map|B|40600000.0|0|0|
|default.map|A|14000000.0|0|0|
|default.map|B|14000000.0|0|0|

The frozen v5 opponent exceeded CPU limits on Big Empty; these resource wins are not independent evidence of strategic superiority. Earlier replay metric schemas omitted invalid-action death events from their runtime counter. The supplemental CLI diagnostics preserve and correct that limitation without rewriting old match rows.

## Experiments

|Experiment|Games|W/D/L|
|---|---:|---|
|EXP001-equivalence|4|4/0/0|
|EXP001-reference|4|4/0/0|
|EXP001-screen|32|22/0/10|
|EXP002-screen|32|21/0/11|
|EXP003-screen|32|24/0/8|
|EXP004-screen|32|25/1/6|
|EXP005-screen|16|12/0/4|
|EXP006-screen|16|12/0/4|
|EXP007-screen|16|13/0/3|
|EXP008-screen|16|12/0/4|
|EXP009-screen|16|11/0/5|
|EXP010-screen|16|12/0/4|
|EXP011-screen|16|12/0/4|
|EXP012-screen|16|15/0/1|
|EXP013-screen|16|13/0/3|
|EXP015-interaction|16|8/2/6|
|bot-v9-exp012-validation-v5|88|82/1/5|
|EXP012-validation-robustness|12|8/0/4|
|EXP012-resource|4|4/0/0|

EXP002 king alone regressed. EXP003 feeding and EXP004 frontier were promising but limited. Portal/adaptive/trade/echo sweeps did not surpass the navigation screen. EXP015 frontier+navigation lost 2–5–1 to navigation alone and was rejected. EXP014 king+feeding+frontier remains untested and preserved.

Strongest maps (all candidate games): Colosseum.map, default.map, devil.map. Weakest: trophy.map, stronghold.map, trauma.map. These rankings include unequal screening exposure; use the balanced validation table for v5 conclusions.

## Limits and next research

This is budget-limited research, not a plateau or optimality claim. External executable bots were unavailable; the replay endpoint required authentication and no API-key environment variable was present. Historical replays referenced in notes were absent. Root/global king consensus, alternative sacrifice methods/timings/sizes, sector assignment, risk-adjusted routing, structured sonar knowledge sharing and independent replication remain promising work.

All original bots and historical result files remain preserved. Exact candidates have source manifests and tar snapshots; working changes are not represented as a new Git commit. Replays and full raw output remain local. Nothing was submitted.

Candidate path: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/candidate/bot-v9-exp012`. Previous champion path: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/current/bot-v8`.

BEST_DISCOVERED_BOT: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/current/bot-v9`.
