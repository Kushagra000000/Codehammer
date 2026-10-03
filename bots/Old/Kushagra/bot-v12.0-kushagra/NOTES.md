# bot-v12.0-kushagra

Derived from `bots/current/bot-v10.3.3` (main.cpp d82a7ea2…, endgame.hpp 6735cab1…,
coordination.hpp 3e4c52c4…, helper.hpp 6bd3b258… — the fingerprints in
`battlecode-automation/experiments/v1033-audit-20260930/fingerprints.json`). Architecture,
Hamiltonian patrol generator, escape splits, remnant cleanup and early game are unchanged.
`helper.hpp` is byte-identical. Not submitted anywhere.

Every change is independently reversible through a `Config` switch in `main.cpp`
(false/0 = v10.3.3 behaviour), except the three pure correctness fixes (1a, 1b, 2b).

| # | Change | Where | Switch |
|---|---|---|---|
| 1a | Terrain sharing certifies only the announced cycle's edges (was: every internal rectangle edge). Unknown edges only; direct observations never overwritten. | coordination.hpp `share_terrain(Claim, variant)`; callers in main.cpp `receive_status` and endgame.hpp `maybe_adopt_territory` | — |
| 1b | Territory validator now checks the last sweep's `u=1 → u=0` link before the return lane. v10.3.3 skipped it and could claim a cycle across kelp (11/400 randomised fixtures). | endgame.hpp `map_territory::valid` | — |
| 2a | Exact-length timestamps (`TeamSignal::length_round`, `KingInfo::len_obs`) separate from position freshness. A complete sighting is exact (even if shorter); a truncated one is a lower bound and does not renew an old exact length past its TTL. | coordination.hpp `update_coordination`; endgame.hpp `offer_king`, `update_king_info` | `V12_FRESHNESS` |
| 2b | Escape identity: `abdicate_after_escape()` clears the parent's own king/territory/claim state before `do_split`, so the 2-long remnant never beacons or claims its old length on the split turn. Heir ignores parent reports stamped `<=` its birth round (was `<`). | endgame.hpp | — |
| 3 | Donations: recipient must be strictly longer than the donor; near-king mode uses the king's BFS route length (not Manhattan) plus `HARVEST_MARGIN` against rounds left. | endgame.hpp `good_sacrifice_spot` (v10.3.3 version kept as `good_sacrifice_spot_v1033`) | `V12_DONATION_RULES`, `V12_DONATION_STRICT_LENGTH` |
| 3x | REJECTED: require >= half of projected drop cells harvestable by the king first. Mean longest fell 18.1 → 15.6 on dev seeds. Code kept, off. | same | `V12_DONATION_DROP_CHECK=false` |
| 4 | Late worker survival: after round 400 a non-retaining dragon with no safe exit may split-escape unless `useful_sacrifice_here()` holds. `certain_death` renamed `no_safe_exit` (it is a forecast). Logged as `WORKER_REVERSE`. Unit cap, tail threat, child space checks unchanged. | endgame.hpp `play_split_escape` | `V12_LATE_WORKER_ESCAPE` |
| 5x | Experimental, OFF: extra penalty for cells 2 steps from a truncated (e.g. portal-split) enemy head. No mechanistic reduction in long-dragon head deaths on dev seeds. | main.cpp `near_soft_threat_` | `NEAR_SOFT_PENALTY=0` |
| — | Experimental, OFF: non-kings leave pearls within R of the king's head after round 400. Inconclusive on dev seeds. | coordination.hpp `reserved_food` | `KING_FOOD_RADIUS=0` |

Not changed (no fixture demonstrated a need): escape-sprint BFS head-cell dedup
(main.cpp `add_escape_sprints`), patrol shortcut ordering (`patrol_score`), sonar
direction allocation, grower counts, early splitting.

Tests: `battlecode-automation/scripts/test_v12.cpp` (28 checks; v10.3.3 fails 15).
Evaluation and results: `battlecode-automation/experiments/v12/REPORT.md`.
