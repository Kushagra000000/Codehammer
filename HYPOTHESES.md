# Hypotheses queue

How this works (see `AGENTS.md` for the full process):
1. The lead adds ideas here. Each has an id, a lane, a priority and a test.
2. An agent claims one by setting `owner` and `status: claimed` in a commit on its
   own lane branch **before** starting work.
3. One hypothesis = one bot version = one logged result in `knowledge/experiment_log.md`.
4. When done, set `status: done-keep`, `done-reject` or `done-inconclusive` and link the log entry.

Lanes: `pearls` (growth and routing), `combat` (fights, splitting, escapes),
`endgame` (late game, queen, tiebreaks), `infra` (tooling, lead only).

Context: since 2026-10-02 the 500-round tiebreak checks **the queen's length first**
(queen = team A's dragon ID 0 or team B's dragon ID 1, observed 2026-10-03; our queen is whichever matches our side). A dead queen counts as
length 0. No existing bot was written with this rule. These ideas are guesses to test,
not facts.

---

## H-001 | lane: infra | priority: 1 | status: open | owner: -
Claim: all earlier ratings are invalid; we need a new baseline on the new engine.
Test: build the env from `requirements.txt` (unswbc 1.2.9), run `unswbc maps`, then
round-robin every bot in `battlecode-automation/panel.json` on every map in `maps/`,
both sides, several seeds. Record per game whether it ended by elimination or at
round 500, and for round-500 games which tiebreak decided it (the engine prints it
with `unswbc run -v`).
Output: a table in `knowledge/experiment_log.md`; proposed champion for the human.

## H-002 | lane: endgame | priority: 1 | status: open | owner: -
Claim: how often games reach round 500, and how often the queen decides them, tells us
how much the new rule matters. If it is rare, queen work is low value.
Test: from the H-001 data, count round-500 finishes and deciding tiebreak per bot and map.

## H-003 | lane: endgame | priority: 2 | status: open | owner: -
Claim: our bots' "king" (the dragon the team feeds) should be the queen, so the
queen is the longest dragon at round 500.
Test: new bot where pearl donation targets and growth priority favour dragon ID 0/1.
Keep if: gate passes vs champion and the panel.

## H-004 | lane: endgame | priority: 2 | status: open | owner: -
Claim: the queen should never split, never take a sacrificial/worker role, and never
risk a head-on collision, because splitting shortens the parent and a dead queen
loses the tiebreak outright.
Test: queen-protection rules added to the champion; measure queen deaths and length.

## H-005 | lane: combat | priority: 2 | status: open | owner: -
Claim: attacking the enemy queen is worth more than attacking other dragons, since
a dead queen loses the tiebreak. The enemy queen is the enemy dragon with ID 0 or 1 (ID 1 if we are team A, ID 0 if we are team B).
Test: bias fight/collision decisions toward the enemy queen when it is in vision.

## H-006 | lane: pearls | priority: 3 | status: open | owner: -
Claim: routing toward the soonest-spawning pearl beats nearest-pearl on large maps.
Test: new bot version; gate on the 7 maps added in unswbc 1.2.x (australia, islands,
maze, stripes, tower_defense, unsw, weakhold) as well as the old set.

## H-007 | lane: infra | priority: 3 | status: open | owner: -
Claim: constants in the champion can be tuned automatically for gains.
Test: move 8-15 magic numbers into one `params.hpp`; tune with SPSA on the Pi/Mac.

---
# bot-v15.0 hypotheses (added 2026-10-03)
`bots/New/bot-v15.0` is the new base (design: `knowledge/v15_design.md`), built from v10.3.3.
These are BLOCKED until v15.0 exists and has been merged into the lane branches.

## H-008 | lane: infra | priority: 1 | status: done-keep | owner: claude-base
Build bot-v15.0 per `knowledge/v15_design.md`. Keep: it builds, plays, no timeouts under `--sandbox`.
Not required: beating v10.3.3.
Result (B-001): builds, 9 full games without errors, sandbox max 6.7M points/turn. Strength unproven (2-0-6 in 8 games).
Note (B-002 rev2): fix 2 kept (DFS bodies clear tail-first); fixes 1 and 4 reverted; fix 3 not needed (head-on always mutual). Still 2-0-6, devil turns ~15% of v10.3.3; growth gap is not the food score. See B-002.

## H-010 | lane: pearls | priority: 1 | status: blocked | owner: -
Claim: the gravity field's falloff (power and ETA scaling) and the rival-ETA damping in `food.hpp` set food-gathering speed. Test two or three falloff variants.

## H-011 | lane: pearls | priority: 2 | status: blocked | owner: -
Claim: the BFS planner in `route.hpp` fires too late or too early. Test the "local score below threshold" trigger and the size of the directional boost.

## H-012 | lane: pearls | priority: 3 | status: blocked | owner: -
Claim: lookahead depth versus the CPU budget is mis-set. Measure CPU use under `--sandbox` and test one deeper or cheaper setting.

## H-020 | lane: combat | priority: 1 | status: blocked | owner: -
Claim: `tactics.hpp` assassination should prioritise the enemy queen and only fire when the sprint is provably safe. Test sprint length limit and queen priority.

## H-021 | lane: combat | priority: 2 | status: blocked | owner: -
Claim: panic-escape ordering (sprint, split escape, reverse split) matters. Test one reordered cascade.

## H-022 | lane: combat | priority: 2 | status: blocked | owner: -
Claim: sacrificial feeding and mitosis should favour our queen (`get_id() <= 1`). Test a rule that feeds the queen first and never splits the queen late.
