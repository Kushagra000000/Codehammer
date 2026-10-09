# bot-v30.7.0-leo — changes and evaluation

Based on **bot-v30.6.0-leo**. One change: portals are taken more readily, as a hedge against unseen finals maps with
many portals. Everything else is v30.6.

## Change

| Switch | v30.6 | v30.7 | Effect |
|---|---|---|---|
| `PORTAL_STEP` | 2 | 0 | A portal on a planned route no longer counts as two extra steps (`navigate.hpp`). |
| `PORTAL_PEN_K` | 1 | 0.5 | The step cost of a portal crossing for dragons over 3 long is halved: 1 → 0.5 (4-6 long), 6 → 3 (7+ long) (`decide.hpp`). |

Tried and dropped: unexplored portals as cheap navigation targets early or all game (`PSCOUT_*`) and a bonus for
stepping into one (`PSCOUT_STEP`): 64-71 of 160 on portal-heavy maps vs v30.6's 78. `PORTAL_STEP` 1 alone: 67/160.

## Results (judge WASM sandbox, vs Sabotage-d v3.9f unless noted)

| Test | v30.6 | v30.7 |
|---|---:|---:|
| 40 random portal-heavy maps (islands joined by portals, small + medium), seeds 1-2 (160) | 78 | **81** |
| same maps, seeds 3-4 (160) | 87 | **90** |
| 25 official maps, seeds 2701-2702 (100) | 58 | **59** |
| 25 official maps, seeds 2703-2704 (100) | **65** | 56 |
| 25 official maps vs Cybers (20594-hh), seeds 2701-2702 (100) | 59 | **64** |
| **Total (620)** | 347 | 350 |

About +2% on portal-heavy maps (171 vs 165 of 320), about -4% on the official maps against v3.9f (115 vs 123 of 200).
A hedge, not a clear improvement: v30.6 is the better-tested choice for the current official maps.

## Limits

- Fewer games than v30.6's full test (1,800+ games).
- Friendly head-ons through portals (a dragon landing on a friend on the unseen side) are not addressed.
