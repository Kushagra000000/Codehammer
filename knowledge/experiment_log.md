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
