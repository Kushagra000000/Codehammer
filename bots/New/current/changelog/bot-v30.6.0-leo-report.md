# bot-v30.6.0-leo — changes and evaluation

Based on **bot-v30.5.0-leo** (see `bot-v30.4.0-leo-report.md` and `bot-v30.5.0-leo-report.md`). It adds the
small-map improvements of Sabotage-d's newest version (v3.9f, ~2250 Elo), implemented in our own code. On maps of
1100 tiles or more nothing changes in practice (the opening and tunnel rules are small-map only; under the crown the
queen never fed anyway).

## What Sabotage-d changed between v3.9 and v3.9f

All general switches, keyed only to map size:

| Switch | What it does |
|---|---|
| `OE_W` 18 → 36, `OE_DECAY` | The opening pull to the centre is twice as strong at round 0 and fades to nothing by round 50. |
| `OE_NR` 50, `OE_NR_TWO`, `OE_NR_AREA` 400 | With exactly two starters a side (maps of 400+ tiles), the one that is not the queen (3+ long at round 0) feels no pull for 50 rounds: it explores while the queen goes for the middle. |
| `Q_TUNNEL` 1 (< 1100 tiles) | The queen pays for a step into a 1-wide corridor with food in it that runs out of her sight: bait, and a dragon coming the other way kills her. |
| `Q_NOFEED` | The queen never feeds the king. |
| `FOUNT_MEM` (< 1100 tiles) | Remembered fountains in navigation. |
| `Q_HUNT_CALL` (from r250) | Late hunt for the enemy queen, called by sonar. |
| `Q_CLEAR` 2 (4000+ tiles) | Our dragons keep two tiles clear round our queen's head. |

Local, all 25 official maps, 2 seeds × 2 sides: v3.9f vs v3.9 **54/100** (small maps 36/60, big maps 18/40).
v30.5 vs v3.9f: **53/100** (small 28/60, big 25/40 — the crown wins the big maps).

## What v30.6 takes (our implementation, switches in `tuning.hpp`)

| Switch | Value | Code |
|---|---|---|
| `OE_W` | 36 | `navigate.hpp` (opening pull) |
| `OE_DECAY` | true | `navigate.hpp` |
| `OE_NR`, `OE_NR_AREA` | 50, 400 | `decide.hpp` (who is the second starter, round 0), `navigate.hpp` |
| `Q_TUNNEL`, `Q_TUNNEL_MIN`, `Q_TUNNEL_AREA` | 1, 99, 1100 | `decide.hpp`, step scoring |
| `Q_NOFEED` | true | `tactics.hpp` `try_feed` |

Not taken: `FOUNT_MEM` (removing it from v3.9f changed nothing: 36/60), the late queen hunt and `Q_CLEAR` (big maps,
where v3.9f lost ground and our crown already wins). Everything from v30.5 is kept (crown, late keep, no pocket runs
for the queen, right of way and the queen's stall on small maps).

## Results (judge WASM sandbox)

| Test | v30.5 | v30.6 |
|---|---:|---:|
| vs Sabotage-d v3.9f, 15 small official maps, seeds 2701-2702 (60) | 28 | **33** (without right of way: 28) |
| vs Sabotage-d v3.9f, all 25 official maps, seeds 2701-2704 | 53/100 (seeds 2701-2702) | **72/119 so far** (small 46/79, big 26/40) |
| vs Sabotage-d v3.9f, 94 random small maps, seeds 1-2 (376) | — | 182 (48.4%) |

Per small map vs v3.9f (seeds 2701-2702, of 4), v30.5 → v30.6: Stripes 0 → 2, Trophy 0 → 2, Tower Defense 1 → 2,
Dilemma 2 → 4, Queen of Spades 2 → 3, Portals 1 → 2; Default 4 → 1.

## Limits

- The 200-game official run against v3.9f was still running when this was written (119 games in).
- On random small maps v30.6 is level with v3.9f (48%), not ahead.
- Still open: friendly head-ons through portals on portal-heavy maps; the queen walking into walled rooms whose far
  walls she has not seen (Tower Defense).

## What to check in ladder replays

- Small maps with two starters (Tower Defense, Queen of Spades, Trophy, Stripes, weakhold): the second starter heads
  off exploring for food instead of following the queen to the middle; both eat in the first 30 rounds.
- The queen does not walk into food corridors that run out of sight.
- Everything listed for v30.4 / v30.5.
