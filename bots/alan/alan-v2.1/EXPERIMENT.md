# alan-v2.1 arrival-triggered sacrifice experiment

Parent: `bots/current/bot-v10.3.3`

Hypothesis: dragons that have already reached a fresh allied king should be
allowed to enter the sacrifice phase during the round-380-to-399 gather
window, while distant dragons retain the original round-400 hard deadline.

Implementation: `sacrifice_started()` becomes true at round 400, or from
round 380 when the known king is fresh, is not this dragon, and its estimated
head is within seven toroidal steps. No movement, combat, scoring, or earlier
phase logic changed.

Decision: accepted as an active challenger, not promoted over the parent yet.

- All 15 maps, seed 10335, both sides: 16-14 after a clean serial rerun of two
  evaluator-error Portals fixtures.
- Seven maps that reached the feeding phase, seeds 10336-10337, both sides:
  17-11 with no errors.
- Across the three normal-engine seeds on affected maps: 25-17 (59.5%).
- Judge-like sandbox check on Portals and Stronghold, seed 10338, both sides:
  4-0 with no errors.
- Normal plus sandbox affected-map evidence: 29-17 (63.0%).

The strongest repeatable gains were Slithery Fight (6-0 across the three
normal seeds) and Stronghold (5-1). Portals was seed-sensitive, so this
candidate still needs diverse-opponent ladder evaluation before promotion.

Reports:

- `battlecode-automation/experiments/v105/experiments/alan-v2-1-arrival-timing-20260929/report.md`
- `battlecode-automation/experiments/v105/experiments/alan-v2-1-portals-rerun-20260929/report.md`
- `battlecode-automation/experiments/v105/experiments/alan-v2-1-late-validation-20260929/report.md`
- `battlecode-automation/experiments/v105/experiments/alan-v2-1-sandbox-20260929/report.md`
