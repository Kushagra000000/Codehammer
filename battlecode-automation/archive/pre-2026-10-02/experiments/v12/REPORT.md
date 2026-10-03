# bot-v12.0-kushagra: audit fixes and evaluation (30 September 2026)

**Bottom line.** v12 fixes several confirmed defects in v10.3.3, and 28 invariant fixtures show each fix
directly. The late-worker escape also has a measurable effect: fewer unintended deaths of dragons of
length 12 or more, and a higher total team length. **It does not win more games against v10.3.3.** On
the held-out seeds it scored 44W–0D–46L (0.489, 95% CI 0.388–0.590). Against the four other versions it
scored 0.679, compared with 0.671 for v10.3.3 on the same schedule. Both differences are within noise.
The preregistered improvement criterion was **not** met. Strictly, the non-regression criterion
(s ≥ 0.5) was also missed, by one game.

Nothing was submitted. v10.3.3 and every archived bot are untouched.

## Setup

- Engine: local `unswbc 1.1.0`, the version the repository pins. Its engine wasm is shipped inside the
  package. Native builds were used; a separate `--sandbox` (wasm, judge metering) subset measured
  resources.
- The recorded match (`replays/bot-v10.3.3-vs-bot-v10.5-on-schooltime-2026-09-30-135117.replay`) was
  not in `replays/` on disk. `death-audit.json` records `seed: null`. So that match **could not be
  reproduced**. Every match here is a new match with an explicit seed. Both bots are deterministic
  (their jitter is a hash, not an RNG), so a (map, seed, side) game repeats exactly without `--sandbox`.
- All source hashes matched `fingerprints.json` before any change. Candidate fingerprints are in
  `bots/current/bot-v12.0-kushagra/SOURCE_STATE.json`.
- Runner: `scripts/v12_eval.py`. It plays every map under `maps/`, from both sides, with explicit seeds.
  An error is recorded as ERROR and is never counted as W/D/L. Summaries come from `scripts/v12_report.py`
  and `v12_pairs.py`. Raw per-game ledgers are gzipped here.
- Plan and acceptance criteria were written before the evaluation: see `PLAN.md`.

## Tests (scripts/test_v12.cpp)

`clang++ -std=c++20 -O2 -DV12 battlecode-automation/scripts/test_v12.cpp -o /tmp/t && /tmp/t`

v12 passes 28 of 28. Built against v10.3.3 the same file fails 15, one per defect:

| Check | v10.3.3 | v12 |
|---|---|---|
| Every cycle variant is a Hamiltonian cycle (w,h 2..9) | pass | pass |
| Every edge of a claimed patrol cycle is known clear (400 random kelp layouts) | **FAIL: 11/400 claims cross kelp** | pass (0/400) |
| Terrain sharing, variants 0–3: only cycle edges become known; no invented interior chords | **FAIL ×4** | pass |
| An unused kelp chord is not certified open to a fresh child | **FAIL** | pass |
| Observed kelp is never overwritten by inference | pass | pass |
| King beacon and territory beacon round trips; a corrupted packet is rejected | pass | pass |
| Heir ignores the parent's king beacon stamped with the heir's birth round | **FAIL** | pass |
| Cornered long grower still split-escapes (behaviour preserved) | pass | pass |
| Parent does not beacon its pre-split king length on the escape turn | **FAIL** | pass |
| A truncated sighting refreshes position but not a 10-round-old exact length | **FAIL** | pass |
| A complete sighting records the exact, shrunken length | **FAIL** | pass |
| King record: an old exact length is not renewed as exact by a truncated sighting | **FAIL** | pass |
| Donation: a short donor near a much longer visible king is still approved | pass | pass |
| Donation: an equal-length recipient is rejected | **FAIL** | pass |
| Donation: route ETA beyond the deadline is rejected (Manhattan distance would pass) | **FAIL** | pass |
| Audit r445 reconstruction (A721 len 13 → 562 len 13) is rejected | **FAIL** | pass |
| Audit r402 reconstruction (A676 len 16 → 617 len 23, clear route) is still approved | pass | pass |
| A trapped late worker with no useful sacrifice escapes by split | **FAIL** | pass |
| A late worker at a useful sacrifice spot does not escape | pass | pass |

About r402: a longer king three cells away on a clear route is a legitimate donation under any rule I
tested. The poor 2/8 transfer happened afterwards, through the king's route and competition from allies.
The donation test cannot see that, so the fixture documents a limitation rather than a fix.

## Evaluation (candidate perspective; score = (W + D/2)/N, Wilson 95% CI)

"Pairs" counts each (map, seed) played from both sides. Mirror v10.3.3 against itself always splits 1–1,
so a pair where the candidate won both games is a gain, and a pair where it lost both is a loss. p is a
two-sided sign test on gains vs losses.

| Ledger | Variant | N | W | D | L | Score | 95% CI | mean longest | opp longest | pair gains | pair losses | p |
|---|---|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---:|
| dev (all maps, seeds 1201-2) | v12 draft (drop check ON) | 60 | 30 | 0 | 30 | 0.500 | 0.377–0.623 | 15.6 | 17.4 | 4 | 4 | 1.00 |
| dev | corr: correctness fixes only | 60 | 33 | 0 | 27 | 0.550 | 0.425–0.669 | 18.1 | 17.2 | 6 | 3 | 0.51 |
| dev | noDon: + late escape | 60 | 33 | 0 | 27 | 0.550 | 0.425–0.669 | 17.9 | 16.1 | 6 | 3 | 0.51 |
| dev | noLate: + donation rules incl. drop check | 60 | 30 | 0 | 30 | 0.500 | 0.377–0.623 | 17.1 | 17.8 | 4 | 4 | 1.00 |
| dev | **donA = final v12** | 60 | 34 | 0 | 26 | 0.567 | 0.441–0.684 | 18.1 | 16.1 | 6 | 2 | 0.29 |
| dev | donB: final without strict length | 60 | 33 | 0 | 27 | 0.550 | 0.425–0.669 | 17.9 | 16.1 | 6 | 3 | 0.51 |
| dev | val12: VALUABLE_LENGTH 12 | 60 | 35 | 0 | 25 | 0.583 | 0.457–0.699 | 19.1 | 16.4 | 6 | 1 | 0.13 |
| dev | val10: VALUABLE_LENGTH 10 | 60 | 33 | 0 | 27 | 0.550 | 0.425–0.669 | 17.9 | 16.6 | 5 | 2 | 0.45 |
| dev2 (8 late-game maps, seeds 1203-6) | **donA = final v12** | 64 | 28 | 1 | 35 | 0.445 | 0.330–0.567 | 31.5 | 32.4 | 3 | 7 | 0.34 |
| dev2 | val12 | 64 | 25 | 1 | 38 | 0.398 | 0.288–0.521 | 31.1 | 31.9 | 2 | 9 | 0.07 |
| dev2 | noLate2: final without late escape | 64 | 27 | 1 | 36 | 0.430 | 0.316–0.552 | 32.0 | 33.7 | 3 | 7 | 0.34 |
| dev2 | corr2: correctness only | 64 | 29 | 1 | 34 | 0.461 | 0.345–0.582 | 31.6 | 32.9 | 3 | 5 | 0.73 |
| dev2 | ns: NEAR_SOFT_PENALTY 20000 | 64 | 32 | 1 | 31 | 0.508 | 0.388–0.626 | 31.8 | 30.6 | 5 | 5 | 1.00 |
| dev2 | resv: KING_FOOD_RADIUS 5 | 64 | 32 | 1 | 31 | 0.508 | 0.388–0.626 | 32.8 | 33.8 | 5 | 5 | 1.00 |
| dev2 | fresh0: final without freshness | 64 | 31 | 0 | 33 | 0.484 | 0.366–0.604 | 32.9 | 33.1 | 5 | 6 | 1.00 |
| **holdout (all maps, seeds 1301-3)** | **final v12** | 90 | 44 | 0 | 46 | **0.489** | 0.388–0.590 | 17.6 | 17.6 | 3 | 4 | 1.00 |

Longest lengths are approximate: they use the last LENGTH each survivor reported, so growth in its
final move is missed. Winners come from the engine and are exact.

Holdout per map: every map is 3–3 except arena (4–2), stronghold (4–2), slithery_fight (2–4) and
trauma (1–5).

### Diverse opponents (all 15 maps, seed 1401, both sides; identical schedule for both bots)

| Opponent | v12 W-D-L | v12 score | v10.3.3 W-D-L | v10.3.3 score |
|---|---|---:|---|---:|
| v10.5 | 19-0-11 | 0.633 | 19-0-11 | 0.633 |
| v10.4.1 | 17-0-13 | 0.567 | 16-0-14 | 0.533 |
| v9 | 21-1-8 | 0.717 | 20-1-9 | 0.683 |
| v11.1 | 23-2-5 | 0.800 | 24-2-4 | 0.833 |
| **All (120)** | 80-3-37 | **0.679** | 79-3-38 | **0.671** |

Paired over the same 120 games, v12 did better in 6 and worse in 5 (p = 1.0). Mean total team length
was 66.0 for v12 against 51.4 for v10.3.3, the most consistent effect measured.

## Acceptance criteria (from PLAN.md)

1. **Correctness: met.** 28/28 fixtures pass. 0 ERROR games across 1,258 native games and 4 sandbox
   games (the first sandbox attempt failed because of a runner bug; those 4 ERROR rows are kept raw in
   `sandbox/`). 0 "no valid action" deaths for either team. Maximum sandbox points per turn: 18.5M for v12 against
   17.1M for v10.3.3 (+8%, under the 20% bound; the judge limit is 100M).
2. **Holdout: improvement not established; non-regression narrowly missed.** Score 0.489 < 0.5 (CI
   0.388–0.590).
3. **Diverse opponents: met.** 0.679 against 0.671.
4. **Escape children: met.** On holdout, 711/720 GROWER_REVERSE children (98.8%) and 2,936/2,992
   WORKER_REVERSE children outlived their birth round; v10.3.3 managed 3,054/3,090.

## Mechanism metrics (holdout, 45 pairs, both teams)

- Unintended deaths at length ≥ 12, excluding FEED and REMNANT_CLEAR: **67 for v12 against 105 for
  v10.3.3**. Most of the removed deaths are late TRAPPED_SELF by long workers (dev ledgers: 33 → 5).
- FEED: 997 vs 944 events. The recipient 8-round growth proxy was 0.87 vs 0.72 per dropped pearl. This
  proxy is confounded by other pearls and is an upper bound, not a pearl-attribution count; exact
  designated-recipient and lineage attribution needs the replay decoder, which was not available here.
- Portal traversals inside the window, with unintended O/H/W deaths on that turn: 774/9,994 (7.7%) for
  v12 against 777/10,125 (7.7%). Intentional TRAPPED_SELF reversals through a portal are excluded.
- Head-to-head deaths of dragons of length ≥ 12 from enemy sprints before round 470 are unchanged
  (≈50 per 64 games in dev2). The killers were short enemies (length 3–9) sprinting 2–5 steps.

## Why the win rate barely moves

About half of all games end by early elimination (rounds 56–300) on arena, Colosseum, default_small,
devil, dilemma, queen_of_spades and trophy. Side and turn order decide them before any v12 change can
act (dev ledgers: A won 41 of 60). On late-game maps the fixes change the outcome of only 10–25% of
games, in both directions, so 60–90 games cannot resolve an effect of a few percent. The
early-elimination dynamic is the largest lever left. Changing it means an early-game strategy change,
which the audit brief ruled out for this candidate.

## Rejected or inconclusive changes (code kept, switched off)

- `V12_DONATION_DROP_CHECK` (require at least half the projected drop cells to be harvestable):
  **rejected**. Mean longest fell from 18.1 to 15.6 on dev seeds.
- `VALUABLE_LENGTH` 12 or 10: inconclusive or negative (dev2: 0.398).
- `NEAR_SOFT_PENALTY` (portal-truncated sprint reach, audit item 5): no reduction in long-dragon head
  deaths, so there was no mechanism to keep.
- `KING_FOOD_RADIUS` (allies leave the king's pearls, aimed at r402-style competition): inconclusive.
- Audit item 6 (escape BFS dedup by head cell, cycle body-order invariant): not changed. No fixture
  showed a failure, and the brief allowed changes only when one did.
- Audit item 5 (A533, A478): not reproducible without the original replay or seed, and neither case
  has a proven safer legal alternative. The only code is the NEAR_SOFT knob, which is off.

## Remaining limitations

- The original schooltime match could not be replayed (replay file missing, seed null).
- The longest-length metric is approximate, and pearl-transfer efficiency is a proxy.
- Dev tuning looked only at dev seeds. Holdout and diverse seeds were run once, on the final candidate.
- 90 holdout games give roughly ±10 percentage points of resolution.
