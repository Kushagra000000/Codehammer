# Strategy knowledge

Rules of use: add an item only with its evidence and date. Label each item
OBSERVED (seen in a replay or log), HYPOTHESIS (idea, untested) or TESTED (gate result,
link the experiment log entry). Items from before 2026-10-02 were gathered under the
old tiebreak and are marked PRE-RULE-CHANGE; they may no longer be right.

## The 2026-10-02 rule change and what it probably means
- HYPOTHESIS: the queen (team A ID 0, team B ID 1) now matters more than any other dragon at
  round 500, because its length is compared first and a dead queen is length 0.
- OBSERVED (PRE-RULE-CHANGE audit, `knowledge/archive/AUDIT.md`): our champion's strategy
  grows a single "king" by splitting, and the longest dragon in the audited game was
  ID 617, not ID 0 or 1. Under the new rule that king is not the queen, so the policy
  that won that game by "longest dragon" may no longer win. This is the main risk to test (H-003).
- HYPOTHESIS: splitting shortens the parent, so a queen that keeps splitting loses length;
  the queen should split rarely or never late (H-004).
- HYPOTHESIS: killing the enemy queen wins the tiebreak if the game reaches round 500 (H-005).

## Carried over from earlier rounds (PRE-RULE-CHANGE)
- OBSERVED: most deaths in a long self-play game were head collisions and deliberate
  self-collisions (clearing a trapped or leftover body), not random crashes.
- OBSERVED: the "grower reverse" escape (a long dragon splits off almost its whole body
  to escape a trap) kept the winning length alive. Preserve it unless data says otherwise.
- OBSERVED: enemy heads at the edge of the 7x7 vision window can ambush a long dragon.
  A truncated sighting (body across a portal) does not reveal true length.
- HYPOTHESIS: early resource concentration, serpentine movement and deliberate feeding
  help on large maps (from a reference replay on Stronghold; source team unknown).
- OBSERVED: map set matters. See `knowledge/map_analysis.md`; only 11 of the maps are analysed
  and 7 newer maps from `unswbc` 1.2.x are not yet.

## Open questions
- How often does a game reach round 500 under each bot and map? (H-002)
- OBSERVED 2026-10-03: the queen is team A's dragon ID 0 and team B's dragon ID 1 (arena and
  default maps). Still to check on the other maps, and whether a queen can ever change ID.
- Maps like default.map start each team with 6 dragons, so only 2 of the 12 starting IDs are queens.
