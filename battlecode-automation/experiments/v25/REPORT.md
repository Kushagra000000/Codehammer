# v25 implementation and validation

2026-10-06. Candidate: `bots/New/current/bot-v25.0.0`; baseline: v24.0.0.
No promotion or remote submission was performed.

Implemented resource-aware opening areas, regional pearl routing, parent/child
separation, two limited-duration centre seats, and a map-scaled safe-move preference
for an understrength swarm. Existing queen timing, 62-unit normal split cap, and
ally pearl-yielding remain unchanged. See the candidate README for configuration
and the additive normal-split sonar handover.

## Verification

- Native C++20 build succeeded. Warnings from `-Wall -Wextra` concerned inherited
  unused constants/fields.
- 52 focused behaviour checks passed; output in `behavior-checks.txt`.
- `preserved-behaviour.json` verifies byte-identical queen, survival, endgame,
  coordination, and helper files, plus unchanged status sender, ally pearl-yielding
  and normal split-policy functions.
- Final code/config hashes are in `source-sha256.json`.
- Final judge-sandbox comparison: eight maps, seed 25002, both colours, 16 games.
  All completed without bot runtime errors. Maximum v25 turn cost was 95,195,996
  points out of 100,000,000. The set includes the full-size 64x64 Big Empty map.

## Outcome

v25 won **6/16** against v24. Elimination outcomes were two wins and three losses;
the other games reached the round limit. This is an implemented, tested candidate,
not demonstrated improvement over v24.

| Metric | v25 | v24 |
|---|---:|---:|
| Mean living population at round 25 | 7.56 | 7.00 |
| Mean living population at round 50 | 14.50 | 15.06 |
| Mean living population at round 75 | 17.31 | 18.75 |
| Pearls collected in first 75 rounds, total | 1,345 | 1,552 |
| Successful early splits, total | 555 | 629 |
| Early deaths, total | 318 | 369 |

Earlier-ended matches retain their terminal population in these averages. Fewer
deaths alone is not evidence of better survival: the populations, reproduction,
and amount of play also differ.

Both Trophy and Maze pairings were wins. Big Empty had stronger early population
on both colours but one final win. Arena and Schooltime need further tuning:
both showed materially worse early pearl collection and population. Default Small
survived to the round limit in both final games but lost the scoring tiebreak.
These are single-seed diagnostics, not statistically reliable win-rate estimates.

`final-validation/report.md`, `matches.jsonl` and `summary.json` contain the full
results. Its generated heading says v22.1 because the existing evaluator was reused;
the manifest and each row identify v25 versus v24 and fingerprint the source copies.
Earlier `smoke-1` and `validation` directories are superseded iterations. The final
iteration corrected centre deployment to use starting workers before filling seats
through queen births; only final-validation describes the delivered version.

Suggested next experiment: vary regional commitment/food bias on Arena and
Schooltime independently, keeping queen timing and pearl-yielding fixed. The new
policy should not be promoted solely on the basis of fewer loops or early deaths.
