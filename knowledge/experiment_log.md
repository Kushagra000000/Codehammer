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
