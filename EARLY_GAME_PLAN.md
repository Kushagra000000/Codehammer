# Early-game dispersion plan

Planning only, 2026-10-06. Baseline: `bots/New/current/bot-v24.0.0` (root README). No bot changes or new matches performed for this review. Findings below are from source inspection, not measured explanations of match losses.

## Verified rule reference

- [Structure](https://game.battlecode.au/docs/structure): turns execute in ascending dragon ID; games end on elimination or after 500 rounds. Final ranking prioritises the original queen (ID 0 or 1; dead means length zero), then longest living dragon, then total length. A surviving child does not replace the original queen for scoring.
- [Splitting](https://game.battlecode.au/docs/splitting): at most 64 living dragons per team; both split halves must have length at least 2. The old tail becomes the child's head, with reversed body order. The child runs a fresh process and acts later that round. Illegal splits kill the parent. IDs use the next unused dragon ID, not a team-local birth counter.
- [Movement](https://game.battlecode.au/docs/movement): the first ceil(starting length / 4) steps are free; subsequent steps cost one segment each. Every step checks collisions before moving the tail. Own-tail entry is fatal. Moving into another head kills both dragons, including allies.
- [Map](https://game.battlecode.au/docs/map-info), [vision](https://game.battlecode.au/docs/vision), [portals](https://game.battlecode.au/docs/kelp-and-portals): the map wraps; vision is a 7-by-7 square and does not look through portals. Kelp and portals live on edges. Geometric centre has no inherent resource advantage in the rules.
- [Pearls](https://game.battlecode.au/docs/pearls): tile countdowns are visible; their reset distributions are hidden. Spawns are attempted before dragons move and fail on occupied tiles. A visited region can remain economically valuable; coverage is not depletion.
- [Sonar](https://game.battlecode.au/docs/sonar): four directional 64-bit messages per turn, sent after the action; blocked by kelp or the first dragon hit. Delivery is not global or guaranteed. No authenticated sender/team metadata is supplied. Higher-ID recipients can receive in the same round; lower-ID recipients receive next round.
- [Death](https://game.battlecode.au/docs/death): death produces ceil(length / 2) pearls, starting at the head and alternating segments.
- [Execution](https://game.battlecode.au/docs/execution-order), [timeouts](https://game.battlecode.au/docs/timeouts): one final movement or split action per turn. Judge budget is 100 million points per dragon per turn, including output. Native local matches do not validate that budget; use sandbox matches for performance checks.

## Findings in v24

1. `main.cpp:194` enables dispersion only at area >=1600 and round >=20. Every dimension pair in `CENTRE_MAPS` has area <1600. Thus designated centre maps never use destination dispersion.
2. `scoring.hpp:183` applies centre progress to non-queens before round 75. Centre pull is 300 per step versus 180 for pearl-route progress. `scoring.hpp:211` applies dispersion only in EXPLORE; a pearl route switches the goal to EAT and removes that influence.
3. `main.cpp:3160` repels nearby allies, mainly at distance 1–2. This avoids close crowding but does not allocate separate harvesting regions. Spacing is skipped for direct pearl collection.
4. `explore.hpp` shares a permanent 49-block explored mask, not active destinations or local resource capacity. Its ID-based targets are geographically diverse but do not estimate food, travel cost, or how many workers an area supports.
5. `main.cpp:2827` excludes a pearl when another ally could arrive first, without knowing whether that ally intends to collect it. This can unnecessarily exclude productive alternatives; measure before changing.
6. `main.cpp:887` applies the safe-candidate filter only to retainers or length >=4. Length-2/3 workers can select an unsafe candidate despite a safe alternative. That may undermine early population growth; measure death causes.
7. Normal splits stop at cap minus two reserved escape slots (`should_split`). Ordinary growth therefore targets 62, not 64. Keep this deliberate survival reserve in the first experiment.
8. Queen cycling disables pearl-route planning from round 75 to 400. Cycle placement favours area, fewer bodies and known terrain rather than pearl yield. Possible scoring bottleneck, separate from dispersion.

## Proposed strategy

Optimise safe pearl intake and sustainable independent reproduction, rather than maximum geometric separation.

1. Allocate at most one or two centre-scout roles in the opening. A practical design is for the queen to issue a limited number of roles to direct split children through checked child-reaching sonar rays. Other newborns default to exploration, and scouts' offspring do not inherit centre status. A missing assignment means exploration. Do not assume ID parity or modulo gives a team-local first/second child. Any replacement mechanism must avoid duplicate roles under lost messages.
2. Give other workers persistent, reachable resource-region or frontier targets immediately after birth. Reuse block coverage and existing path fields; rank a small target shortlist by known pearls, useful countdowns, new vision, travel time, congestion, enemy exposure and escape options. Distinguish previously explored from currently claimed and from economically depleted.
3. Use soft regional preferences inside pearl-route selection as well as exploration. Take cheap safe pearls en route; avoid switching regions for a marginally better individual pearl. Reassign when blocked, unproductive, unsafe, or clearly overstaffed. Start with a short commitment interval and progress-based expiry; tune experimentally.
4. At splits, parent keeps its useful area and child prefers a different reachable frontier or underused bed. Extend existing handover where possible. Fall back to ID-seeded target ranking plus local observations if sonar misses. Avoid a communication-dependent global assignment algorithm.
5. Share short-lived destination claims later if simpler local assignment still duplicates work. Use arrival cost and ID to resolve conflicts, expire stale claims, and permit multiple workers on rich beds. Messages are advisory, not authenticated facts.
6. Keep safety and queen protection ahead of regional preferences. Allow a bottleneck to be shared temporarily; route separation may only become possible beyond it. Use wrapped, obstacle-aware distances and known portal connections.

## Experiments

Compare baseline, limited-centre plus early assignments, then regional pearl-route preferences. Keep combat and split policy fixed initially. Use identical maps/seeds/opponents with both team colours.

Measure at rounds 10/25/50/75/100: living workers, collected pearls, unique team-visible tiles, overlapping vision, occupied productive regions, queen status, and deaths by cause. Record time to 8/16/32/62 units, whether thresholds are ever reached, and child survival after 5/10 turns. Judge elimination rate and queen outcome alongside early growth; greater coverage alone is not success. Verify worst-case turn points in sandbox matches.

Separate follow-ups: safe-candidate preference for small breeders; replace hypothetical ally ownership with intent-aware discounting; compare immediate length-4 splitting against conditional waiting until length 5 on rich safe routes (length 5 gains two free steps, but delaying reproduction can lose growth).
