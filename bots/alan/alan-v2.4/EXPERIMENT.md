# alan-v2.4 collectible-body sacrifice experiment

Parent: `bots/alan/alan-v2.1`

Hypothesis: early sacrifice should occur only when the king can collect most of
the pearls produced by the feeder's entire body, rather than judging only the
head's death tile.

Before round 400, the complete tracked body is sampled at the segments that
produce pearls. At least 60% must lie ahead on the king's known patrol cycle or
within collection range of a visible king. At round 400 the original hard
deadline and v2.1 behavior remain unchanged. No phase start, movement, combat,
or feeding-position logic changed.

Decision: rejected. The first two-seed screen against alan-v2.1 finished 17-11,
but the third fresh seed reversed to 5-9. Combined parent result: 22-20
(52.4%), too small and unstable for promotion. The validation seed was 7-7
against v10.3.3. All 56 matches completed without errors.

Reports:

- `battlecode-automation/experiments/v105/experiments/alan-v2-4-collectible-screen-20260929/report.md`
- `battlecode-automation/experiments/v105/experiments/alan-v2-4-validation-20260929/report.md`
