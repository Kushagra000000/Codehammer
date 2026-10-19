# Changelog: v24.0.0 → v25.0.0

Implementation date: 6 October 2026.

Baseline: `../bot-v24.0.0/`. This version now lives in
`bot-v25.0.0-Kushagra/`; earlier experiment records call it `bot-v25.0.0/`.
The C++ source and bot configuration in this folder match the recorded final
v25 test hashes.

## Purpose and outcome

v25 attempted to build a larger, more dispersed early swarm without abandoning
valuable central resources. The intended opening was two centre scouts, with
other workers harvesting different productive areas and children branching away
from their parents. Empty regions should be abandoned, and small swarms should
prefer survival over risky movement.

**This was not a demonstrated improvement over v24.** The final local sandbox
comparison finished with 6 wins and 10 losses. Average population improved at
round 25, but fell behind by rounds 50 and 75; total early pearl collection also
declined. Passing the behaviour tests established that the implemented mechanisms
worked in those fixtures, not that their combined strategy was effective.

## File-by-file changes

| File | Actual change | Intended behaviour |
|---|---|---|
| `opening.hpp` — new | Added opening state, centre allocation and release, resource-region scoring, target lifecycle, split handovers, route weighting, and population-based candidate filtering. | Coordinate early expansion while avoiding prolonged crowding and empty-area occupation. |
| `main.cpp` | Added configuration constants, included `opening.hpp`, called `initialize_opening()` after sonar reception, moved target updates after ETA preparation, replaced the move-selection filter with `candidate_allowed()`, and added reception of packet type `0x8`. | Connect the new opening policy to the existing turn pipeline. |
| `explore.hpp` | Renamed the original `update_explore_target()` to `update_legacy_explore_target()`. Normal split coverage handover first attempts `send_opening_handover()`, then falls back to the original coverage sender. | Retain the old exploration policy outside the opening and pass newborns opening metadata. |
| `route.hpp` | Multiplied each shortlisted pearl's route weight by `opening_route_weight()`. | Make resource routes favour the worker's assigned region. |
| `scoring.hpp` | Restricted `centre_pull()` to active centre scouts. Added `disperse_score()` to known-path `EAT` progress during the opening. | Stop every worker being drawn to the centre, and keep regional direction relevant when food is found. |

`bot.toml`, `helper.hpp`, `coordination.hpp`, `endgame.hpp`, `queen.hpp`, and
`survival.hpp` were unchanged. Changes in movement selection can still change
the situations those unchanged components encounter.

## 1. Limited centre roles

### v24

On the configured centre-map dimensions, non-queen workers received a centre
attraction before round 75. It was not restricted to two workers. Its strength
was 300 points per path step toward the centre, outside the centre radius.

### v25 implementation

`initialize_opening()` assigns starting IDs 2/3 to seat 1 and IDs 4/5 to seat 2
on centre maps. It only attempts this when the process's first round is at most
1 and no opening handover has already assigned that newborn. The rule uses the
alternating initial team order; it is not a general child-ID numbering rule.

The queen initialises her grant counter from the existing friendly unit count,
reserving seats for starting workers. If a seat is missing, a later safe normal
split can grant it to one of her direct children. Other workers always give
their children seat zero.

Only `centre_scout_active()` workers receive the original centre pull. The
existing map list, 300-point pull, radius calculation, and round-75 cutoff remain.
During region ranking, scouts additionally:

- Lose 240 points per step beyond the centre radius.
- Gain 160 points for their preferred half of the centre: seat 1 prefers the
  lower-x half; seat 2 prefers the higher-x half.

A scout permanently releases its role when, while inside the centre radius:

- At least 12 rounds have elapsed since its first entry; or
- Three qualifying centre observations find at least three other friendly heads
  within distance four; or
- Three qualifying centre observations find neither food value nor unknown
  terrain in the considered central regions.

The checks occur while the scout is inside the centre radius; the elapsed dwell
timer is not a count of uninterrupted residence. Expiry at round 75 also disables
the role. Release removes the special centre attraction, but ordinary food routing
may still keep a worker in a productive central patch.

**Limits:** this allocates opening seats, not a continuously maintained force of
two living scouts. Deaths and released roles are not replaced through a global
election. Missing workers depend on the queen reproducing and a handover reaching
the child. Starting-worker detection uses first-round/ID checks, not an explicit
engine-provided founder flag.

## 2. Resource-aware area selection

### v24

Destination dispersion started at round 20 and only on maps with at least 1,600
tiles. Workers selected ID-based sequence points in unexplored blocks. The
centre-map size list was entirely below the dispersion area threshold.

### v25 implementation

The new `update_explore_target()` handles non-queen, non-retaining workers before
round 100, provided dispersion is enabled. It runs immediately on small maps too.
It reuses the existing **7-by-7 grid of 49 regions**; these are not necessarily
7-by-7-tile regions.

Each turn it builds a terrain-distance field from the head, scores reachable
tiles, chooses a region, and builds a field toward that region's useful anchor.
The anchor maximises tile value minus 20 times path distance.

Tile values include:

- Unseen tiles: 100, or 40 when the shared coverage mask already marks the region
  explored.
- Remembered pearls: considered for 12 rounds, starting at 1,000 and losing 50
  per round of age, capped at a 600-point reduction.
- Upcoming spawns: considered when due by estimated arrival plus the existing
  four-round spawn window. Value is `600 / (1 + extra wait after arrival)`.
- Recently expired unseen timers: a smaller, rapidly fading value for at most
  six rounds after the expected spawn.

Known occupied tiles do not supply food value. These estimates reuse the existing
map memory; they do not discover hidden resources or prove future food availability.

The base region score is:

```text
min(food value, 6000) / 4
+ min(unseen tiles, 16) × (12 if explored, otherwise 35)
− 45 × distance to anchor
− 400 × max(0, friendly heads − capacity + 1)
− 500 × enemy heads
```

Capacity is `clamp(food value / 2000, 1, 3)`. Friendly counts use visible heads
plus fresh existing status signals, without double-counting visible senders.
An ID/region hash adds 0–300 points to separate otherwise similar choices.

This is a **local preference**, not exclusive region ownership. No global target
registry, destination-claim protocol, or complete estimate of allied intentions
was added. Several workers can still select the same region.

## 3. Commitment and leaving unproductive regions

The current region receives a 500-point commitment bonus for its first six
rounds and a 250-point bonus afterwards. A sufficiently better alternative can
override either bonus; this is not a mandatory six-round stay.

A region is retired if it has no useful anchor, the current target becomes
unreachable, or eight rounds pass without progress. Progress means reducing
distance below the recorded best distance or increasing the snake's length.
Retirement gives that region a 12-round cooldown. Crowding can also cause another
region to outscore the current one.

A fully known barren map supplies no regional target. Previously explored regions
can still be selected when they have food value. At round 100, the opening target
is cleared and the original v24 dispersion policy resumes; on small maps that
legacy destination policy is disabled.

**Intention:** reduce oscillation without camping on exhausted areas. The cooldown,
commitment weights and growth-based progress test are heuristics, not guarantees
of good harvesting decisions.

## 4. Region preferences during pearl collection

In v24, dispersion affected `EXPLORE` progress, but finding a pearl route switched
the goal to `EAT`, removing that influence.

v25 makes two changes:

1. `plan_route()` multiplies pearl weights by 1.25 inside the assigned region and
   0.85 outside it. Pearls within two path steps retain a multiplier of 1.0.
2. `goal_progress_base()` adds the existing 120-point-per-step dispersion term to
   `EAT` movement when the simulated path is known and opening regions are active.

Direct pearl reward is unchanged. Outside-region food remains eligible, and the
existing ally-arrival exclusion still applies. The multiplier is applied after
the existing nearby-pearl shortlist is formed; it does not expand that shortlist.

**Intention:** workers should keep separate harvesting directions after discovering
food. **Tradeoff:** a worker can favour a weaker local route over a better shared
patch. The tests did not isolate how much this contributed to the observed losses.

## 5. Parent/child separation and sonar

The parent keeps its current target state when splitting. A child receiving an
opening handover learns the parent's target sector, or the parent's current sector
if it has no target. Sectors form a coarse **4-by-2 grid**, distinct from the 49
exploration regions.

During the child's first 12 rounds, candidates in that parent sector lose 280
points. An additional 90-point-per-distance-step heading preference encourages
movement along its birth orientation; ID-based preferences also work without a
handover. Neither mechanism forbids staying near the parent when food is better.

Existing sonar formats and the normal sending schedule were preserved. A new
normal-split packet uses:

```text
63–60: magic 0x8       59: team
58–10: all 49 coverage bits
 9–8: centre seat      7–5: parent sector
 4–0: checksum
```

It replaces the coverage payload only on checked child-reaching rays during an
opening normal split. The receiver validates the packet, merges its coverage,
and accepts assignment metadata only once on its first turn. Roles are not relayed.
If the new sender cannot send, the original coverage handover is attempted.

`opening_ray_hits_child()` is new. It requires a complete body model, follows the
shortened parent's tail orientation when the backward sonar leaves through the
tail, and can recognise known child segments outside the vision window. The
existing escape-body ray checker was not modified.

Thus sonar was **extended**, not left completely untouched: new sender/receiver
logic was added, while status `0xD`, coverage `0x9`, portals `0xB`, beacons `0xE`,
and escape-body `0xA` retained their existing encodings.

## 6. Safe movement for an understrength swarm

v24 applied its safe-candidate preference to retaining snakes and snakes of length
at least four. Approved head trades could bypass that filter.

v25 replaces that selection condition with `candidate_allowed()`. Before round
380, a swarm below the following living-unit threshold also protects short workers:

```text
min(normal split population limit, clamp(ceil(map area / 64), 6, 32))
```

Examples are 6 units for 16×16, 16 for 32×32, and 32 for 64×64. If a safe candidate
exists, unsafe ordinary candidates—including ordinary head trades—are rejected
while below the threshold. With no safe candidate, fallback choices remain.
Above the threshold, the previous selection policy applies.

This changes the preference between existing candidates; it does not redefine
`safe` or replace the survival search. Earlier queen-kill, feeding and emergency
split overrides remain separate and can bypass ordinary candidate selection.

## Preserved decisions

- Normal multiplication still stops at 62 on a 64-unit game, reserving two escape
  slots.
- Queen seeding ends at round 75; her cycle period ends at round 400.
- Ally pearl-yielding was not relaxed or replaced with intention claims.
- No changes were made to the normal split eligibility function, helper protocol,
  queen policy, survival search implementation, or feeding implementation.

## Validation and observed results

The recorded final version passed 52 focused behaviour checks and completed 16
judge-sandbox games: eight maps, seed 25002, both team colours, against v24.
There were no recorded runtime errors. Maximum v25 turn cost was 95,195,996 of
100,000,000 points, including testing on the 64×64 Big Empty map.

| Metric | v24 | v25 |
|---|---:|---:|
| Wins in the paired comparison | 10 | 6 |
| Mean population at round 25 | 7.00 | 7.56 |
| Mean population at round 50 | 15.06 | 14.50 |
| Mean population at round 75 | 18.75 | 17.31 |
| Pearls collected in the first 75 rounds, total | 1,552 | 1,345 |
| Successful early splits, total | 629 | 555 |
| Early deaths, total | 369 | 318 |

Early-ended matches retain their terminal population in the averages. Fewer deaths
alone does not establish stronger survival when population, reproduction and
total turns also differ. v25 had two elimination wins and three elimination losses.

Both Trophy and Maze pairings were wins. Both Arena and Schooltime pairings were
losses with weaker early resource collection. Big Empty showed stronger early
population in both colours but only one final win. Default Small reached the
round limit twice and lost both scoring tiebreaks.

These results show that the combined changes failed to establish an improvement.
They do not identify a single cause: centre allocation, regional attraction,
route preferences, split coordination and safety filtering changed together.
Possible costs to investigate separately include overcommitting to regions,
discounting valuable out-of-region pearls, and deploying missing centre scouts too
late. Those are hypotheses, not conclusions from the test suite.

Related repository records:

- `battlecode-automation/scripts/test_v25.cpp` — behaviour fixtures; its include paths
  refer to the original `bot-v25.0.0` name and need adjustment to rerun after renaming.
- `battlecode-automation/experiments/v25/behavior-checks.txt` — recorded checks.
- `battlecode-automation/experiments/v25/final-validation/` — final match report,
  metrics, manifest and source fingerprints.
- `battlecode-automation/experiments/v25/source-sha256.json` — tested source hashes.
- `battlecode-automation/experiments/v25/REPORT.md` — implementation/test summary.

This changelog is documentation only. No bot behaviour was changed while writing it.
