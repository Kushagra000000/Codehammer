# bot-v24.0.0: Timeouts and execution order

Source: `bots/New/current/bot-v24.0.0` (main.cpp 3,922 lines; route.hpp, scoring.hpp, survival.hpp, endgame.hpp, helper.hpp, queen.hpp, explore.hpp, coordination.hpp). Written 2026-10-06 from a full read of the code.

## Execution order

This page gives the bot's turn in the order it runs. Most surprising replays come down to three facts. First, the bot builds its whole picture of the world before it looks at a single move. Second, three overrides (queen kill, remnant clear, feeding) can end the turn before any move is scored, and those turns send no sonar. Third, every search stops at a points deadline, so a heavy turn quietly falls back to cheaper estimates instead of timing out.

All files except helper.hpp are `#include`d inside one class, `ReplayDerivedBot`. They are one object, not separate modules.

## The process

1. **Startup.** `main()` reads the clock, then `unswbc::init()` reads `ID`, `TEAM`, `MAP w h` and `UNIT_LIMIT`. It sets a 64 KB full buffer on stdout so a whole turn's output costs one write. It then builds the bot with an empty per-tile memory of the map.
2. **The loop.** `update()` reads one turn. If it reads `ENDGAME`, the process exits. Otherwise the turn clock starts, `play_turn()` runs, and `end_turn()` prints `PROTOCOL 3` and `ENDTURN` and flushes. The clock is read again, and that reading is the start of the next turn.
3. **The clock.** In the judge (wasm) the clock counts points, 1 ns = 1 point, so time spent blocked waiting for input does not count. In local native builds, nanoseconds are multiplied by 24 to imitate points, and the turn clock restarts once the round has been read.
4. **The first turn.** `first_round_` is set from the round number. `founder_` is true when that round is 0 or 1 and the length is 3. `heir_` is true when born at length 10 or more, which only a split escape produces. The birth length is also recorded.

A restarted process (after a timeout) starts with a fresh init block and **no memory**: no map, no body trail, no king, no portals. If it is long it comes back as an heir, which keeps the grower role.

## One turn (`play_turn`)

### Phase A: building the picture (always runs)

1. **`receive_status`.** Decodes this turn's sonar inbox. Messages with the wrong team bit or a failed checksum are dropped. The magic number picks the type:
   - `0xD` **status**: an ally's position, length, id and grower flag, plus an enemy report and symmetry bits. Rejected if more than 1 round old.
   - `0xE` **king beacon**: the king's id, length, position, round seen and queen flag.
   - `0xB` **portal pair**: both ends of a portal. Accepted only if it agrees with every edge we have seen ourselves.
   - `0x9` **coverage map**: the 49 explored 7×7 blocks.
   - `0xA` **body handover**: read only on our first turn, from the parent that split-escaped into us.
2. **`remember_vision`.** Copies all 49 visible tiles into memory: pearl, countdown, edges, and whether another dragon is on the tile. It samples countdowns to learn each tile's respawn gap (the pearl-bed rate). It registers portal ends, and it updates two running averages: the *fill rate* (free tiles taken per round) and *occupancy* (share of visible tiles holding another dragon).
3. **`refresh_neighbours`.** Rebuilds the cached step table `nbr_` (tile, direction → tile, `WALL` or `BLIND`) only around tiles whose edges changed. The whole table is rebuilt when a new portal end is learnt. Every search reads this table.
4. **`prepare_centre`.** On the 7 map sizes in `CENTRE_MAPS`, before round 75, runs a breadth-first search from the map centre so moves can be pulled towards it.
5. **`update_body_model`.** Applies the body handover, if any. Adds last turn's path to the trail, then checks it against what we see. Vision wins: on a mismatch the trail is reset. The trail is then extended out of sight from earlier sightings, and cut to our length. This builds `own_idx_` (our segment index per tile, −2 for "ours, order unknown") and `other_occ_` (another dragon is here).
6. **`observe_heads`.** For every other head in view, traces its body through facings to get a guaranteed length and whether the body runs out of sight. `queen` is set when the raw id is below 2. A running average of enemy length and of enemy and ally head density per tile is updated.
7. **`remember_bodies`.** Estimates how long each visible body segment will stay. Segment *i* of a dragon of length *len* stays for (len − i) ÷ pace rounds, + 1, where an ally's pace is half its free steps and an enemy's is 1. The estimate is capped at 30 rounds. `predict_hidden_segments` then marks the tile where a body visibly continues out of sight as taken.
8. **`compute_roles`.** Sets `retaining_now_` to one of three cases:
   - The queen retains except while seeding, which runs until round 75.
   - From round 400, only the elected king retains.
   - Before 300, nobody retains. From round 300, hash-picked growers (1 in 10) and anyone who is the king or vice retain.

   `valuable_now_` = retaining, or length ≥ 16, or the queen (from round 0).
9. **`update_king_info`.** Expires stale king knowledge after 15 rounds (40 in small-team mode), refreshes it from heads we see, and offers ourselves as king if we qualify. From round 380 the queen wins every election.
10. **`build_threat_maps`.** Builds the threat maps:
   - Enemy threat: every tile within the enemy's free steps.
   - Ally threat: every tile within one step of an ally head.
   - For valuable dragons only, *hard* threat: tiles inside an enemy's full sprint reach (free plus paid steps) at its guaranteed length, and within its 7×7 view.
   - *Soft* threat: the same reach if its hidden body is typical length (capped at 10).
11. **`update_coordination`.** Updates the enemy report and picks `enemy_target_`: from round 50, the nearest visible enemy worth hunting, or a long reported one within 12 tiles. From round 380, workers also get `feeder_target_`, the king's last position, with a path field to it. An attack within 3 tiles cancels feeding.

### Phase B: overrides (each one ends the turn)

These run in this order. Each prints its own move, and **none sends sonar**.

1. **`play_queen_kill`.** If any dragon other than our queen sees the enemy queen and can put its head on hers this turn (breadth-first through visible empty tiles, at most `max_turn_steps`, then replayed exactly with `simulate_path`), it moves there. Both die. Logged `QUEEN_KILL`.
2. **`play_clear_remnant`.** On the turn after a split escape, a remnant of length 3 or less (never the queen) with no free side dies on its own neck. Logged `REMNANT_CLEAR`.
3. **`play_feeder_strategy`.** From round 400, a feeder dies on its own neck so its pearls land for the king. All of these must hold:
   - No enemy head is within 3 tiles.
   - The king's head is visible.
   - The king is 2 to 4 tiles away (6 in small-team mode).
   - Our head is not in a corridor.
   - The king can actually reach us.

   Logged `FEED`.

### Phase C: per-turn maps (deadlines start here)

1. The current tile's visit count goes up by one.
2. **`update_explore_target`.** On maps of 1,600+ tiles, for workers, from round 20 to 380: keeps a destination in an unexplored block, picked from an R2 sequence keyed by the dragon's id. It is replaced when reached, unreachable, or after max(30, w + h) rounds.
3. **`prepare_survival`.** Works out, per tile, the move from which another dragon's body will have gone. It keeps four versions:
   - `other_free_at_` includes tiles another head can reach before our next move.
   - `wall_free_at_` is bodies only.
   - `plan_free_at_` covers the king's later turns.
   - `body_free_at_` is used for routing.
4. **`prepare_etas`.** Runs a time-aware breadth-first search from our head (out to 30 steps or the route radius) and one from each visible head, giving per tile the fewest turns any enemy needs and each ally's steps.
5. **`build_objective_map`.** One multi-source search outwards from everything worth going to:
   - Visible pearls cost 0.
   - Remembered pearls cost age ÷ 10.
   - Tiles about to spawn cost (rounds left + 1); good beds count 8 rounds ahead, others 4, keepers 10.
   - Expired timers nobody watched cost 5 + age ÷ 8; good beds cost 2.
   - Never-seen tiles cost 8 for workers and 12 for keepers, +8 if the team has already explored that block.
   - Before round 100, for workers, the tiles in front of a half-known portal cost 5.

   Pearls that are reserved for a grower, that an ally reaches first, or that sit in a portal pocket we would die in are skipped. Trap-marked tiles block the search. Feeders don't explore.
6. **`update_portal_room`.** If the tiles reachable without a portal (at most 64, all seen) hold no objective and a portal leads out, `stuck_in_room_` is set and crossing out becomes cheap. Never for the queen.
7. **Deadline `ROUTE_STOP` (40M points). `plan_route`.** Skipped for feeders and for the cycling queen.
   - Takes the closest 14 winnable pearls within the route radius (14 to 20 steps, or more for dragons with many free steps, at most 100) and runs a breadth-first search from each pearl.
   - Runs a beam search (width 16, depth 8) over visiting orders. A pearl's value is discounted per *turn* of arrival (0.92 for keepers, 0.85 for workers), cut to 40% if an enemy arrives first, and reduced by crowds. Keeping last turn's target adds +0.15.
   - Replays the top 8 orders step by step with the body growing and trailing, rejecting any that hit our own body. Each surviving route's end gets a survival search.
   - The first order that passes becomes the route. If none passes, any single pearl that does is used.
8. **Deadline `MOVES_STOP` (66M). `choose_goal`.** The base goal is the first that applies: **FEED** (we have a feeder target), **HUNT** (enemy target within 15 tiles, not valuable), **EAT** (a route exists), or **EXPLORE**. `prepare_fight` collects up to 2 enemy heads within 4 steps; if there are any, the goal becomes **FIGHT** and the base goal still pulls at half strength. The queen never fights.

### Phase D: candidate moves

They are added in this order, all into one list:

1. **Single steps** (`evaluate_single`), in the order N, E, S, W, not back into the neck. If none survives, they are retried with the reverse included. A step onto a body is dropped unless it is an eligible enemy head (a head trade). For each step the bot computes:
   - exits and time-aware room, along with region stats (dead end or trap);
   - known room (valuable dragons only);
   - the **survival search** result;
   - the move value from scoring.hpp, and the flags `critical_ok`, `safe`, `walled` and `pocket_plan`.
2. **Tactical sprints.** Multi-step head trades into eligible enemy heads, at most 6 steps, from round 50 on, non-retaining dragons only.
3. **Pearl sprints.** Only with an enemy in view, to a visible pearl beyond our free steps that an enemy reaches no later than we do. The pearls must pay for the paid steps.
4. **Free moves.** Every path of 2 up to min(⌈L/4⌉, 6) steps through visible tiles with no portals, replayed exactly. Paths are grouped by end tile and arrival direction, and the best 6 by quick value get a full evaluation. Retaining dragons skip these from round 380 when the goal is EXPLORE.
5. **Route rush.** Follows the checked route for all our free steps, including over remembered-free tiles out of sight. Each blind tile carries a risk of age × fill rate (capped by occupancy and at 50%). A rush with more than 8% total risk (2% when valuable) is marked unsafe.
6. **Escape sprints.** Valuable dragons only, when no move is `critical_ok`: a breadth-first search over paths of 2 to 6 steps (paying segments), keeping the best 4.
7. **Deadline `PLAN_STOP` (91M). `plan_king_moves`.** Applies to the king and growers with at least 2 free steps, but not the cycling queen. It runs on the 8 best candidates. Each is extended for turns 2 and 3 with the 4 best options per turn, scored by food on the way and route progress. A plan counts only if a deep survival search from its end proves a way on for the whole length. The best plan's later food, discounted 0.85 per turn, is added to that candidate's score.
8. **Deadline `SEARCH_STOP` (92M). `apply_known_space_penalties`.** For valuable dragons, every tile of known room short of the best candidate's costs 0.2% death chance, capped at 8%.

### Phase E: splits

1. **`play_split_escape`.** Split to survive.
   - For the queen it triggers only when every move is hopeless:
     - a trade;
     - a move with no exit and no pearl to eat first;
     - a move whose survival search was complete and found at most 1 more move.
   - For workers and feeders it triggers only when the head is certainly dead: every move is a trade, a hard threat, or a move with no exit.
   - Growers and the king can also split out of tight space if length ≥ 12.
   - It needs length ≥ 4, a free unit slot for our tier (see below), and a legal split.
   - `choose_escape` keeps the largest front part that can still live. That is either a part next to a tile the child's tail will free, or a part beside a child of 2–3 that dies at once. If neither exists, it keeps 2.
   - It sends a status packet, then a body handover on every ray that will hit the child. The handover replaces the status packet on that ray. Logged `GROWER_REVERSE`, `WORKER_REVERSE` or `FEEDER_REVERSE`.
2. **Voluntary split.** All of these must hold:
   - `should_split`: not the queen after round 75; before round 380; not retaining; length > 3; at least 2 unit slots spare.
   - `split_child_ok`: the child has an exit.
   - `split_parent_ok`: the parent keeps a free side.
   - Not in a 1-wide corridor when length ≤ 5.
   - Not holding as a newborn escape child (the first 3 turns).

   Child size = L − 2 (the queen keeps 4), capped at 9. It sends status plus the coverage map to the child.

### Phase F: choosing and output

1. A newborn escape child, on its first turn, gets −5 segments on any move that keeps its tail in place.
2. If there are no candidates at all, the bot reverses into its own neck (`TRAPPED_SELF`), which is suicide.
3. **Selection.** Retaining dragons and anyone of length 4 or more are *conservative*: if any candidate is `safe`, unsafe ones are skipped unless they are head trades. The highest score wins. If everything was filtered out, the highest score overall wins.
4. **Queen cycle override.** From round 75 to 400 the queen walks a Hamiltonian cycle on a block (up to 16×16 and 144 tiles, more if she is long). She takes as many free steps along it as are *queen-safe*: `safe` and `critical_ok`, zero threats of any kind, and a way on proven for her whole length (or 2 exits if the search was cut short). Otherwise she takes the queen-safe move nearest the cycle. If no move is queen-safe, the normal choice stands. The block is rebuilt when kelp breaks it or she outgrows it.
5. **`refuse_route_portals`.** If the goal's way on was a portal crossing and we chose otherwise, the far tile is trap-marked for 15 rounds so the goal stops pulling us back to it.
6. **Output.** `MOVE d` for one step or `MOVE dddd` for several, with a log line naming the multi-step kind (`ATTACK_SPRINT`, `PEARL_SPRINT`, `RUSH`, `FREE_MOVE`, `ESCAPE_SPRINT`). It records portals taken (for loop detection) and the path (for next turn's trail).
7. **`send_status`.** One `SONAR` per direction:
   - On the beacon slot ((round + id) mod 4): a king beacon, from round 300. From round 380, 3 of the 4 slots carry beacons.
   - Opposite it: a portal pair every 3 rounds, before round 380.
   - Next to it: the coverage map every other round, before round 300.
   - Everything else: the status packet, holding the *predicted* position and length after the move.

### What a turn prints

```
LOG ...            (only some branches; debug builds print many more)
MOVE NNE           (or SPLIT k)
SONAR N <u64>
SONAR E <u64>
SONAR S <u64>
SONAR W <u64>
PROTOCOL 3
ENDTURN
```

It is one buffered write. When an escape split is followed by the body handover, two `SONAR` lines can name the same direction. The engine keeps the last one, which is the handover, and the code relies on that.

## How the bot models the engine (`simulate_path`)

Every multi-step candidate is replayed the way the engine applies it.

1. A dragon of length L (its length at the start of the turn) takes ⌈L/4⌉ steps free. Each later step needs current length > 2 and costs one tail segment. Lengths 1–4 get 1 free step, 5–8 get 2, 9–12 get 3, and 80 gets 20.
2. Each step is checked against our body as the previous step left it, tail included, because the check runs before the tail moves. A tile fatal as a first step can be safe later.
3. A pearl is counted once per tile on the path and keeps the tail for that step.
4. Steps must land on visible tiles, except a rush, which may cross remembered-free tiles out of sight but never through a portal. A head trade must end exactly on an enemy head.

The engine page for bot-v2.0.0 says *every* step after the first is paid. v24 assumes the newer ⌈L/4⌉-free rule (the comment in helper.hpp says the same). If the engine disagrees, every free move and rush will cost segments the bot didn't plan for. Check one replay.

## The survival search (`survival`)

From where a move ends, the bot runs a depth-first search over our own next single steps. It plays them as the engine would: our tail moves, pearls in view are eaten, known portals are followed and unknown ones are not, and other bodies are walls until their timing says they have moved on.

- **Target depth.** L + 2 moves, at most 20. The king and growers use L + 2 up to 300.
- **Order.** The most open tile is tried first, so open water costs very little.
- **Budgets.** 4,000 nodes per candidate and 40,000 per turn. Deep searches (king and growers) get 20,000 and 250,000. Route checks get 24,000 (80,000 for keepers), and plans get 150,000.
- **Points.** One search may spend at most a quarter of what its phase has left, and at least 2M. The clock is checked every 128 nodes.
- **Walled vs contested.** If the search fails with other heads' reach counted, it is searched again with real walls only. Still failing is **walled**: certain death, priced at the full trap cost. Passing is **contested**, and the second result is used.

## Scoring (`move_value`, in thousandths of a segment)

- **Currency.** A pearl is +1000. Our death costs our length (×1.5 if retaining; the queen at least 30) plus 2 for the unit. A kill gains the enemy's length plus 3, and +3 more from round 470 against dragons of 10 or more. A head trade is worth kill − death.
- **Goal progress.** Only the goal's own pull scores:
  - EAT: 180 per step closer along the route, +80 for following the checked path.
  - EXPLORE: 180 per step closer to the nearest objective, plus dispersal (120 per step) and the late rendezvous.
  - HUNT: 180, or 340 from round 380.
  - FEED: 260, or 400 in small-team mode.

  On top of any goal:
  - Before round 75, on centre maps, the centre pull adds 300 per step. It does not apply to FIGHT or FEED, or to the queen.
  - The enemy-queen hunt adds 1000 per step.
  - Taking our boxed queen's last free side costs 4000; taking any free side of her head costs 600.
- **Position.** This term prices what happens after we arrive:
  - The trap terms work in order. A survival search that reaches its target leaves only a small one-exit risk. A walled move pays the full trap cost. A complete search that falls short pays 90% × shortfall × trap cost. Otherwise the cheaper room estimates are used: no exit 80%, one exit 1%, short room 30% scaled, dead end 80%.
  - On top of that: exits and room add a little, an adjacent enemy costs 2% (outside fights), and each ally head next to the tile costs 120.
  - Valuable dragons also pay 50% per hard threat, 3% per soft threat, and a crowd term. Long or valuable dragons also pay for unseen tiles within 3 steps.
- **Portals.** A blind crossing costs 8% death up to round 40, rising linearly to 20% at round 150. It is halved if we saw the far tile free within 4 rounds, and 80% if a body was seen there. Before round 100 the far side's richness counts, including unexplored tiles. After that only known pearls count (300 per expected pearl, cap 2500), and only if the far side has a way on. A crossing out of a portal room is worth +1500 at 10% risk. Going back through the pair just used, with nothing to eat, costs 240.
- **Fights.** For each nearby enemy, each of its replies is played against our best answer. The result is 0.7 × worst + 0.3 × mean. The replies include a ram (a trade), a sprint into our head up to max(3, its free steps), leaving it less room than its length (60% kill), and leaving us no exit.
- **Habits.** Jitter of 0–20 breaks ties. Each earlier visit to a tile costs 11 (2 for keepers), except on good beds. There are spacing penalties near allies, −140 for sitting on a tile about to spawn, and 12 per step away from enemies (22 for keepers, 150 for the queen).

## Timeouts

The judge meters each turn at **100M points**. Over that, nothing is delivered, the dragon dies (no valid action) and the process restarts with no memory. v24 never relies on finishing a search. Each phase has a deadline measured from the turn's start:

| Phase | Stops at | What runs out first |
|---|---|---|
| Pearl route (`plan_route`) | 40M | fewer pearl maps, fewer replays |
| Move evaluation (survival searches) | 66M | later candidates read "not searched" and use room estimates |
| King's plans | 91M | remaining candidates keep their scores |
| Anything after (split escape, etc.) | 92M | as above |

The comments say a turn peaks near 95M: the plan overshoots its stop by under 1M, and choosing plus the output write costs under 4M. Writing costs about 2.5M per write plus 4,000 per byte. A long `LOG` (debug builds) eats into the 5M margin. **Never ship a `BOT_DEBUG` or `BOT_PROFILE` build.**

## Roles and the calendar

| Round | What changes |
|---|---|
| 0 | The queen (raw id 0 or 1) is valuable from the start: no hunts, no trades, no blind portals. |
| 0–75 | The queen seeds (worker splits, keeps 4). On the 7 centre maps everyone else rushes the centre. |
| 20 | Dispersal starts on big maps (1,600+ tiles). |
| 40–150 | The blind-portal risk rises from 8% to 20%. |
| 50 | Trades, sprints and hunting are allowed. |
| 75–400 | The queen walks her Hamiltonian cycle. |
| 100 | Workers stop treating unknown portals as exploration. |
| 300 | Growers (1 in 10, by hashed id) and the longest dragon retain. Beacons start. Coverage sharing stops. |
| 380 | No more worker splits. The gathering starts. The queen is crowned king. Free moves for keepers are only for eating, fighting or escaping. |
| 400 | One retainer (the king). Everyone else feeds by dying next to it. Small-team mode starts at 6 units or fewer. |
| 470 | Any non-retainer may trade into an enemy of 10 or more. |
| 500 | The game ends. The tiebreak is queen length, then the longest dragon, then total length. |

**Unit-slot tiers for splits.** Normal splits stop at cap − 2. A split escape needs a free slot for its tier: ordinary workers below cap − 2; valuable dragons and anyone of length 8 or more below cap − 1; the king up to the last slot. The cap is min(64, UNIT_LIMIT).

## Things worth checking (found while reading)

1. **Override turns send no sonar.** `play_queen_kill`, `play_clear_remnant` and `play_feeder_strategy` return before `send_status`. A dragon killing the queen sends nothing, which may not matter. But a crowd of feeders dying near the king also stops relaying beacons for that turn.
2. **12-bit id wrap and the queen.** `remember_ally_length` ignores ids below 2 before round 380 using the *wrapped* id. A late split child with id 4096 or 4097 is treated as the queen there. Rare, but possible in long, split-heavy games.
3. **Free-step rule.** As above, confirm the engine gives ⌈L/4⌉ free steps. The bot-v2.0.0 page describes paying for every step after the first.
4. **The queen's cycle ignores the route.** From round 75 to 400 she doesn't plan pearl routes (`plan_route` returns early) and she skips the king's plans. Her growth depends on pearls landing on her block.
5. **Unused symmetry deduction.** `sym_h_`, `sym_v_` and `sym_r_` exist but are never updated ("remains disabled"). Only bits received by sonar can set the symmetry, and nothing in the move logic reads it.
6. **Reversal as a last resort.** With no candidates at all, the bot moves into its own neck. This always kills it, and its pearls drop where it stands.
