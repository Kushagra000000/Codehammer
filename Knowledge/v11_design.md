# v11 navigation experiment — 2026-09-26

Parent: frozen bot-v10.2; comparisons also include v10.1, v10, v9 and v8.
Only new v11/v11.1 working directories are edited. Every tested source is
archived before matches. No automatic official submission or promotion.

## Architecture and observed issues

OBSERVED (source inspection): v10.2 retains v9 remembered portal routing,
per-instance vision/pearl memory, flood space, threat scoring, swarm splitting,
and v10 grower/claim/feeding coordination. Body memory records one head per
turn; sprint intermediates are missing and fresh children have no body history.
Pearl BFS routes through bodies. Rectangular patrols can change with a body
already inside, and are merely a scoring bonus. Center attraction continues
throughout the game; the 32x16 special case lasts 200 rounds. A 48x24 dimension
check causes unconditional non-top-two suicide at 450. Echoes are aggregate,
so zero enemies cannot certify a particular portal safe.

## Algorithms researched

John Tapsell's actual Snake implementation describes ordering every body segment
along a cyclic tour and allowing shortcuts only without overtaking the tail:
https://johnflux.com/2015/05/02/nokia-6110-part-3-algorithms/
https://github.com/johnflux/snake_game
A second real implementation uses head-to-tail path search / Warnsdorff ordering:
https://github.com/chynl/snake

HYPOTHESIS: ordered cycles plus bounded virtual-body search transfer usefully.
Battlecode differs: occupied tail is lethal BEFORE movement, partial vision,
kelp edges, portals, moving rivals, growth and fresh split processes. A cycle
cannot guarantee safety against adversaries. General Hamiltonian search on
irregular maps is not a practical per-turn guarantee under the instruction cap.

## Implementation

- Preserve growth from 250, early splitting and guarded local feeding.
- Center attraction ends at 25, known barren center or two visible allies there.
  Resource-aware hashed sector gradients spread workers without overriding food.
- Remove dimension-only suicide/opening overrides and excessive feeder gravity.
- Body-aware objective BFS, reconstruct visible children using segment facing,
  and retain all sprint intermediate positions until actual next-turn length.
- Depth-four virtual body continuation; no moving into the current tail.
- Emergency two-to-five step dodges, costing one-to-four segments, only when
  no ordinary safe candidate exists. Require observed destinations; do not
  rely on extra sprint tail shrinkage to make an occupied route safe.
- Lock territory when every known body segment is correctly cycle-ordered.
  v11 follows the cycle; v11.1 permits forward shortcuts without overtaking
  the tail. Static flood space cannot veto an otherwise verified ordered cycle.
- One rotating Protocol 3 ray sends an observed tile: four edges, one portal
  identity, pearl/countdown and original round. Dynamic reports expire; direct
  knowledge has priority. Other rays retain grower and territory coordination.
- Aggregated echoes never certify portal occupancy. Enemy reports are only
  emitted when locally confirmed, preventing indefinitely renewed relay ghosts.

## Predeclared evaluation

Baseline smoke: Stronghold v10.2/v10.1 both sides, seed 8100, strict replay/CLI
agreement. Candidate smoke and native safety fixtures before screening.
Screen v11/v11.1 against v10.1/v10.2 on Stronghold, Big Empty, Default and Trophy,
both sides seed 8101. Then fresh-seed all-map comparison against all five older
versions for the strongest surviving candidate; mutual candidate comparison.
Both sides always count separately. No dropped crashes or unparsed outcomes.
A recommendation needs positive aggregate score against each named baseline,
no resource failure, and fresh validation. Report sample uncertainty; if a
baseline is not beaten, say so rather than fabricate success. Formal historical
v5 promotion remains a separate untested gate in this requested opponent pool.

## R2 and R3 adjustments

TESTED: R1 screen was 6W/2D/8L for v11.1. R2 caches repeated same-turn BFS
queries and own-body occupancy. Territory expansion now tries the larger cycle
and keeps it only if the complete body still has a valid cyclic order (either
orientation). A verified cycle gets an absolute priority score, not a bonus
that an inaccurate static flood deficit can overwhelm. Patrol entry begins at
length eight. v11 R2 screen: 10W/2D/4L; v11.1 R2 with depth-eight remembered
terrain search and shortcuts: 9W/2D/5L. The deeper variant was not superior.

R3 v11.1 returns to the stronger v11 R2 navigation and requires at least four
known pearl-spawning tiles inside a proposed patrol territory. A summed-area
table evaluates resource counts without rescanning every rectangle. HYPOTHESIS:
this prevents safe but unproductive loops on sparse maps. Target screen uses
Devil, Schooltime, Stronghold and Trauma, both sides seed8103, against v11 and
v10.2. v11 R2's all-map fresh validation is independently seed8102.

## Validation and compatibility follow-up

The initial v11 R2's complete 110-game validation failed the opponent-pool
criterion despite winning its screen. A higher opening center weight (1200
instead of45, still ending by25/barren/occupied) screened 7–9 and was rejected.
v11 returns to weight45 with the new5/6/7 sonar tags (R4); a fresh all-map
v10.1 comparison uses8107. This is a new source identity, not silently merged
with the earlier110 games. The latest v11.1 R6 uses v10.1 food/exploration
weights and the repaired tags; all-map validation seed8105 is separate.

Performance: v11 R2's final Big Empty sandbox checks max52.1M/55.0M instructions,
versus the original R1's99.2M/99.8M. No CPU-limit or invalid-action events.

A final independent option is prepared directly from v10.1: retain its small
snake decision rules and transplant body memory, ordered patrols, long-snake
body-aware routes, bounded dodges and terrain sharing. Its status/claim/enemy
messages use disjoint full-byte A1/A2/A3 tags. It will only replace the v11.1
working slot after the running R6 validation completes, and the R6 archive is
kept for exact restoration if the rebase is worse.
