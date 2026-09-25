# Experiment Log

## Baselines

### BASELINE-CURRENT — Uploaded Current Bot

Status: Baseline observation only. No new experimental result should be inferred from this section.

Current architecture:

* Maintains per-dragon map memory.
* Records visited tiles, pearl observations and edges.
* Learns portal endpoints from visible edges.
* Builds enemy and ally threat maps from visible heads.
* Uses flood-fill space estimation.
* Uses pearl-distance scoring.
* Uses teammate spacing.
* Uses deterministic movement jitter.
* Uses tactical head-trade sprints.
* Uses four-directional sonar status messages.

Current behavioural phases:

* Early game: splitting until approximately round 300.
* Middle game: approximately one quarter of IDs are selected as growers.
* Late game: all surviving dragons retain length after approximately round 425.

Important limitations to investigate:

1. No explicit king/central-growth policy (as observed in one of the top ranking teams)
2. No explicit late-game feeder/sacrifice strategy (observed in one of the top teams)
3. No true configurable portal probability (looks like a variable we can play around with)
4. Exploration is local rather than explicitly sector-based.
5. Split children start with fresh memory.
6. Pearl route distance does not model kelp/portal topology.
7. Some combat decisions treat large and small dragons similarly.
8. Sonar communication is not yet a full team-control protocol.
9. Current phase transitions are mostly fixed to round numbers.
10. Some tactical behaviours may overfit the replay that originally inspired the bot.

---

# Experiment Format

Each experiment must contain:

## Experiment ID

Unique identifier such as `EXP-001`.

## Parent

Bot version used as the starting point.

## Candidate

Temporary candidate or resulting bot version.

## Hypothesis

What specific idea is being tested?

## Variables

Every changed parameter and its exact value.

## Unchanged Variables

Important controls that remain fixed.

## Maps

Maps tested (try testing on all the maps using the inbuild unswbc run or script I created (recheck script before running))

## Opponents

Versions / bots used as opponents (try using top elo teams as opponents)

## Games

Number of games and side assignments.

## Results

Wins, losses and draws.

## Per-map Results

A candidate should never be judged from aggregate results alone.

## Strategic Metrics

Record where possible:

* longest surviving dragon,
* total team length,
* final unit count,
* elimination round,
* exploration coverage,
* portal entries,
* portal deaths,
* sacrificial deaths,
* pearls collected by king,
* king survival.

## Conclusion

One of:

* Reject
* Weak improvement
* Promising
* Candidate champion
* Requires more testing

Never describe an experiment as successful if the result is unparsed or incomplete.

---

# Initial Experiment Queue

## EXP-001 — Parameterise Existing Behaviour

Goal:

Move important strategy values to a configuration section at the top of `main.cpp`.

Parameters should include:

* split cutoff,
* final growth cutoff,
* grower fraction,
* portal probability,
* portal bonus,
* exploration weight,
* visit penalty,
* space penalty,
* threat penalty,
* king mode,
* sacrifice start round.

No strategic behaviour should change in this experiment.

Purpose:

Create a clean baseline for automated optimisation.

---

## EXP-002 — Portal Probability

Hypothesis:

Increasing intentional portal exploration improves map coverage and pearl access.

Test:

* 5%
* 10%
* 25%
* 50%
* 75%

Keep the rest of the strategy identical.

Primary metrics:

* win rate,
* portal entries,
* portal-related deaths,
* map coverage.

---

## EXP-003 — Exploration Strength

Hypothesis:

The current swarm converges too strongly toward the same pearl regions.

Test different exploration weights and visit penalties.

Compare:

* current behaviour,
* low exploration,
* medium exploration,
* high exploration,
* deterministic sector exploration.

Primary metric:

Map-by-map performance plus exploration coverage.

---

## EXP-004 — Sector-Based Distribution

Hypothesis:

Assigning dragons deterministic map sectors produces better resource coverage than purely local exploration.

Test:

* no sectors,
* 4 sectors,
* 8 sectors,
* 16 sectors,
* hash-based dynamic sectors.

Measure:

* tile coverage,
* team spread,
* pearl collection,
* survival,
* win rate.

---

## EXP-005 — King Identification

Hypothesis:

A dedicated king produces a longer final dragon.

Test:

* no king,
* ID-based king,
* root dragon king,
* largest-dragon king,
* root-then-largest backup.
* Snake Id 1 as king (given priority since beginning of game and has low risk moves)

Do not assume `ID 1` is universally correct.

---

## EXP-006 — Late-Game Feeding

Hypothesis:

Small dragons should deliberately die near the king in the final phase.

Test:

* no sacrifice,
* sacrifice after round 400
* sacrifice after round 430,
* sacrifice after round 450,
* sacrifice after round 465.

Measure:

* final king length,
* team win rate,
* final total length,
* king survival rate.

---

## EXP-007 — Sacrifice Method

Compare:

1. self-collision,
2. collision with king body,
3. sprint death,
4. hybrid method.

Do not intentionally collide with the king's head.

---

## EXP-008 — Feeder Size

Test:

* maximum feeder length 2,
* maximum feeder length 3,
* maximum feeder length 4,
* no restriction.

Measure final king length versus total sacrificed pearl yield.

---

## EXP-009 — Adaptive Phase Timing

Replace fixed phase boundaries with state-dependent decisions.

Candidate inputs:

* round,
* rounds remaining,
* unit count,
* king length,
* enemy longest known length,
* number of small dragons,
* map size.

---

## EXP-010 — Portal-Aware Pathfinding

Replace the Manhattan/toroidal objective distance with:

* local BFS,
* known-map BFS,
* portal-aware BFS,
* risk-adjusted BFS.

Compare performance and compute cost.

---

## EXP-011 — Sonar Echoes

Use Protocol 3 sonar echoes as additional information.

Test whether they can improve:

* danger detection,
* corridor detection,
* enemy proximity,
* portal exploration,
* king protection.

---

## EXP-012 — Sonar Team Command System

Build structured messages for:

* king,
* feeder,
* explorer,
* scout,
* defender,
* backup king.

Test whether coordination beats independent local decisions.

---

## EXP-013 — Large-Dragon Combat Restriction

Hypothesis:

Large dragons should not be allowed to make ordinary head-trade attacks.

Test:

* current behaviour,
* only length <=3 may head-trade,
* only length <=4,
* adaptive trade based on role.

---

## EXP-014 — Memory Sharing

Problem:

Split children start with fresh program state.

Test sonar-based sharing of:

* portal endpoints,
* high-value pearl locations,
* exploration targets,
* king location,
* danger zones.

Measure whether redundant exploration falls.

---

## EXP-015 — Full Late-Game Economy

Combine:

* king,
* feeders,
* portal routing,
* king protection,
* adaptive phase,
* pearl prioritisation.

Only run this after individual components have been measured and only their individual best
values are taken into account.

---

## EXP-016 — When to start increasing length

Having the snakes switch to increasing length afte continusouly splitting (current model). But change 300 to try other values.

* Stop splitting after round 300
* Stop splitting after round 100
* Stop splitting after round 150
* Stop splitting after round 200
* Stop splitting after round 350
* Stop splitting after round 400

Only run this after individual components have been measured and only their individual best
values are taken into account.

---

# Search Method

Do NOT immediately brute-force every parameter combination.

Use staged search:

1. Establish a frozen baseline.
2. Test one variable at a time.
3. Keep the strongest few variants.
4. Test interactions between promising variables.
5. Combine only proven components.
6. Re-test the combined strategy on holdout maps.
7. Compare against the frozen v5 baseline.
8. Promote only robust candidates.

This avoids wasting thousands of matches on obviously bad combinations.

---

# Baseline Acceptance Rule

The team's stated minimum requirement is:

> A final candidate should beat v5 on at least 6 different maps.

For automated testing, define "beat a map" precisely, for example:

`candidate wins more games than v5 over repeated games on different maps`

using both side assignments.

A single lucky game is not sufficient.

---

# New Map Rule

Any new `.map` file added to the repository must automatically enter evaluation.

The automation system should:

1. detect the new map,
2. run the frozen v5 baseline,
3. run the current champion,
4. run the newest candidate,
5. record results,
6. add the map to the dashboard,
7. never silently exclude it because it was not known when the strategy was written.
