# bot-v30.5.0-leo — changes and evaluation

Based on **bot-v30.4.0-leo** (see `bot-v30.4.0-leo-report.md`). On maps of 1100 tiles or more it plays exactly as
v30.4 (checked turn for turn). On smaller maps it adds right of way among our own dragons and the queen's smart
split, as designed by Leo:

> right of way — priority of movement decided by: 1. you are the queen; 2. the other is the queen on a narrow path;
> 3. you realise the queen is stuck; 4. no queen: compare lengths by sonar, the shorter dies; 5. no time for sonar:
> the older (lower id) goes first; 6. assume you are the longest when longer than 8 (maps up to 16×16) / 15.
> Smart queen split: when stuck she splits 2 off her back (a split is her action, so she does not move) so the
> friend in the way realises it and kills itself.

All switches are in `tuning.hpp`; with them off the bot plays as v30.4.

## Changes (maps under 1100 tiles: `ROW = true`, `ROW_AREA_MAX = 1100`, `QNOLANE = true`)

| Change | Behaviour and limits | Code |
|---|---|---|
| Priority between two of our dragons | Our queen; then a dragon longer than 8 (maps of at most 256 tiles; 15 on larger ones) takes itself for the longest; then the longer, by its whole body in view or by the right-of-way sonar packet; then the lower id. | `tactics.hpp` `friend_outranks` |
| Right-of-way sonar packet | `6 << 60 \| team << 59 \| flags(queen, stuck) << 57 \| length << 49 \| id << 33 \| check << 17`, sent only straight at a friend's head next to mine in a 1-wide passage (the beacon still goes in the other directions), and by a boxed-in queen in all four directions. Sending it at every friend in contact displaced the king beacon and cost the crown on big maps. | `comms.hpp`; `decide.hpp` (`turn`, right-of-way block) |
| Prevention | A dragon does not step onto the 1-wide passage ahead of a friend with priority (followed up to `ROW_AHEAD` = 5 tiles, cost `ROW_W` = 5), nor next to that friend's head inside a passage (half the cost). | `decide.hpp`, step scoring |
| Smart queen split | Boxed in, the queen never makes the reverse split (it gives the free tail end away and leaves her, the head end, stuck). She stalls with `SPLIT 2` while 4+ long and calls "stuck". | `decide.hpp`, no-candidate branch |
| Freeing the queen | A friend that sees her boxed in (from its view, or her call) and has a segment next to her head that will not free up before she runs out of stalls dies against kelp or its own body (never into a head), so she moves first next round. | `decide.hpp` (`row-free-queen`), `tactics.hpp` `row_die` |
| Boxed-in workers | Next to a friend with priority: die at once (`row-yield`). Next to one without: stall (`row-hold`), it will yield. | `decide.hpp`, no-candidate branch |
| Queen out of lanes | The queen never walks deeper into a lane (a dead-end farming corridor) and never makes the lane-out split, which leaves her inside. | `QNOLANE`; `decide.hpp` |

## What it fixes (replays with reason labels)

- Map *small*, every seed: a friend entered the queen's 1-wide lane head first and she died boxed at round 3. Now the
  friend dies at round 2 (`row-free-queen`) and she walks on.
- 24 games on corridor maps: our queen boxed in by our own dragons 4 → 0; deaths of ours into kelp or bodies with a
  friend's head next to them 15.9% → 11.5% of our deaths (Portals excluded). Devil 49 → 0, Stripes 19 → 3,
  default_small 40 → 15.
- In base games half of our dragons' deaths are "boxed in, no move" (499 of 980 in 16 games).

## Results (vs Sabotage-d, judge WASM sandbox, seeds 2701–2708 both sides; v30.0 = 50%)

| Maps | v30.4 | v30.5 | Notes |
|---|---:|---:|---|
| 15 official maps < 1100 tiles (240 games) | 121 | **130** | right of way on |
| 10 official maps ≥ 1100 tiles (160) | 97 | 97 | identical play |
| **All 25 official (400)** | **218** | **227** | |
| 94 random small maps (376, seeds 1, 2) | 195 | 183 | −12: within noise, not a gain |
| 57 random big maps (228) | 118 | 118 | identical play |

Small official maps, per map (of 16), v30.4 → v30.5: Dilemma 8 → 16, Small 8 → 11, Autarky 9 → 10, Portals 9 → 10;
Arena, Devil, Stripes, Queen of Spades 8 → 7; the rest unchanged. Queen alive at the end on the official maps: 82 in the
run with right of way on every map (v30.0: 69).

Right of way on big maps too (`ROW_AREA_MAX = 0`) scored 93/160 there (64×64 maps 30/64 vs v30.4's 37/64), so it
stays off on big maps: dragons are long there, jams rarer, and the crown's coordination matters more.

## Limits

- Locally the gain is not proven: +9 on official small maps, −12 on random small ones. The situations it fixes are
  real but rare in the mirror test; ladder replays are the test.
- Not fixed: on Portals our dragons step through portals onto a friend's head on the unseen far side (~60 a game;
  one-way portal costs, `OWPORT_W`, cut it by only ~20%); on Tower Defense the queen walks into walled home rooms whose
  far walls she has not seen (unseen walls count as open).

## What to check in ladder replays

- Small maps: the queen is not boxed in by our own dragons; when she is, she stalls (splits 2) and a friend next to her
  dies to free her; two of ours meeting head first in a corridor: the one with priority survives.
- Big maps: as v30.4 (queen grows from round 150; long dragons stay whole after round 420).
