# Experiment log (new rules, from 2026-10-03)

Append one entry per tested bot version, newest first. Earlier experiments used the old
tiebreak and live in `battlecode-automation/archive/pre-2026-10-02/experiments/`.
Failures are as valuable as wins: log them.

## Entry template
```
### E-NNN | bot-v13.x-hNNN | hypothesis H-NNN | agent: codex-a/codex-b/claude | date
Change: one or two sentences, with the files touched.
Engine: unswbc version. Maps: how many. Seeds: list. Games: N (errors: N, reported separately).
Result vs champion: W-D-L, score rate. Result vs each panel opponent: W-D-L.
Round-500 games: how many, and which tiebreak decided (queen / longest / total / draw).
Weakest maps: names and score rates.
Verdict: keep / reject / inconclusive, and why. Confidence: low/medium/high.
Next step suggested:
```

## Entries
### E-000 | setup check | no bot change | agent: claude | 2026-10-03
Engine: unswbc 1.2.9 (Python 3.13). Ran bot-v10.3.3 (A) vs alan-v2.1 (B), arena.map and default.map, seed 1.
Both bots build and run unchanged on 1.2.9. Arena game: team A won by elimination after 82 rounds
(about 12 s including build). One game only: this is a setup check, not evidence of strength.
Also observed: queen IDs (A=0, B=1), see `knowledge/game_rules.md`.

(Next entry should be E-001, the re-baseline from H-001.)

### B-001 bot-v15.0 build | hypothesis H-008 | agent: claude | 2026-10-03
Change: new base bot `bots/New/bot-v15.0`, copied from bot-v10.3.3 and split into main.cpp, params.hpp,
tactics.hpp (cascade + panic), food.hpp (gravity + rival ETA), lookahead.hpp (budgeted DFS), route.hpp
(BFS planner + portal/vision memory); helper/coordination unchanged; endgame.hpp kept, king election now
always picks our queen (ID <= 1). Five commits "v15.0 stage 1..5" on base/v15.0.
Engine: unswbc 1.2.9 (Linux venv). Maps: arena, default_small, devil, trophy, both sides. Seed: 1. Games: 8 (errors: 0).
Result vs bot-v10.3.3: 2-0-6 (wins: default_small as A, trophy as A). All 8 ended by elimination; no round-500 games.
Sandbox (devil, v15 as B): p50 5.5M, p99 6.4M, max 6.7M CPU points per turn (limit 100M).
Observed: on devil v15 played 1258 dragon-turns vs 9156 for v10.3.3, so it grows/splits far less.
Verdict: build accepted as base (H-008 done-keep); strength not measured (8 games, untuned). Confidence: low.
Next step suggested: H-001-style re-baseline of v15.0 vs v10.3.3 on all maps; first suspect the gravity weights (H-010).

### B-002 bot-v15.0 rev2 | hypothesis H-008 | agent: claude | 2026-10-03
Change: three planned fixes plus optional fix 4, each tested once on the standard 8-game check vs bot-v10.3.3
(arena, default_small, devil, trophy; both sides; seed 1; unswbc 1.2.9). Pre-rev2 state tagged `v15.0-rev1`.
Rule check (game_rules.md, Collisions/Death): a head-to-head kills BOTH dragons regardless of length or team.
Phase A diagnosis (4 games, -v): v15 dies mostly head-to-head (always mutual, so equal counts both sides) and
rarely by hitting itself; v10.3.3 dies to itself far more but splits 4-10x as often (devil: 38 vs 278, 11 vs 115).
The split gap is already there by round 50 (6-10 vs 23-25).

| step | W-D-L | devil turns v15 (A+B) | devil splits v15 A/B | v15 deaths devil A / B | verdict |
|---|---|---|---|---|---|
| rev1 baseline | 2-0-6 | 633+1258=1891 | 11 / 38 | h2h16 self2 / h2h33 self11 | - |
| fix 1 far-field linear pull | 0-0-8 | 913+903=1816 | 18 / 22 | h2h17 self7 / h2h23 self5 | reverted (wins fell) |
| fix 2 DFS bodies clear tail-first | 2-0-6 | 633+1258=1891 | 11 / 38 | unchanged | kept |
| fix 3 panic head-on rule | - | - | - | - | no change: head-on always kills both, so adjacency is always lose-or-tie |
| fix 4 v10 growth policy | 2-0-6 | 980+787=1767 | 19 / 28 | h2h23 self2 / h2h20 self15 | reverted (devil turns fell) |
v10.3.3 devil turns in the same games: rev1 3518+9156, fix 1 4797+4952, fix 4 5291+4834.
Sandbox (devil, v15 as B, final code): p50 5.6M, max 6.7M points/turn (limit 100M). Errors: 0 in 33 games.
Verdict: target not met (needs >= 3 wins and devil turns >= half of v10.3.3). Fix 4 shows the growth gap is
NOT the food score: with v10.3.3's exact pull v15 still splits ~5x less. Remaining suspects: layer 4 route
boost overriding v10 exploration, layer 3 penalties, layer 5 panic, GRAVITY_CAP flattening dense areas
(only matters if gravity scores). Confidence: low (single seed, 8 games per step).
Next step suggested: an ablation run (layers 3, 4, 5 each switched off) measuring splits by round 50 on devil.
