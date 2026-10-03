# bot-v15.0 design: hybrid cascade bot

Goal: a new base bot, built from `bots/Old/current/bot-v10.3.3` (call it Bot 1), that
replaces Bot 1's single flat score with the layered design below (Bot 2). Bot 1 combines
distance, threats and exploration into one aggregate integer score per candidate move
(`c.score`). Bot 2 decides in layers. This file is the shared spec for the base builder
and for the lane agents. It describes intent, not tested facts: nothing here is proven to win.

## Layers (evaluated in this order)

### 1. Priority cascade (fast-exit overrides)
Before scoring ordinary moves, run hard-coded tactical checks. If one fires, take that
action and skip the rest.
- **Assassination:** compute exact multi-step sprint paths to ram a vulnerable enemy head,
  especially the enemy queen (team A queen is ID 0, team B queen is ID 1).
- **Sacrificial feeding:** decide whether this dragon should hit its own body so the dropped
  pearls feed a nearby queen or a longer ally.
- **Mitosis:** check swarm capacity and split when strategically worthwhile.

### 2. Inverse-square "gravity field" for food
For every visible pearl compute an arrival time (ETA) and an attraction that falls off with
the square of the ETA, so close food pulls sharply. Contest food: simulate rival head ETAs
(enemy and friendly) and strongly dampen a pearl's score if someone else gets there first.

### 3. Forward DFS lookahead with a CPU budget
Check each candidate tile with a depth-first search of future board states, capped by an
explicit operations budget. Model the tail clearing (empty cell versus body), and delay tail
clearing when a pearl is eaten along the simulated path. If the search cannot reach the
required depth, apply a large, fractional dead-end penalty, which separates fatal traps from
temporary jams.

### 4. Global BFS route planner
If the best local pearl score is below a minimum threshold, switch from local scoring to
breadth-first pathfinding through portals and unseen cells toward remembered pearls or
exploration frontiers. If a route exists, add a directional boost to the candidate that
follows it.

### 5. Post-evaluation fallbacks (panic escapes)
After combining food scores, DFS survival penalties and BFS weights to choose a move, check
the move. If it is fatal or leaves the dragon inside an enemy's sprint reach, fall back to
emergency multi-step sprints, split escapes, or reverse splits.

## Required code layout (so lanes can work in parallel without merge conflicts)
| File | Contents | Lane that mainly edits it |
| --- | --- | --- |
| `main.cpp` | init, update loop, calls each layer in order | lead / base only |
| `tactics.hpp` | layer 1 cascade and layer 5 panic escapes | combat |
| `food.hpp` | layer 2 gravity field and rival ETA contest | pearls |
| `route.hpp` | layer 4 BFS planner, pearl memory, frontiers | pearls |
| `lookahead.hpp` | layer 3 budgeted DFS | shared, change rarely |
| `params.hpp` | every tunable constant, with a one-line comment each | all lanes (append only) |
| `helper.hpp`, `coordination.hpp` | copied unchanged from v10.3.3 unless a fix is needed | nobody |
| `endgame.hpp` | v10.3.3 endgame logic, kept at first | endgame / lead |

## Constraints
- C++20, deterministic (no unseeded randomness), within 100M CPU points and 48 MB per dragon per turn.
- Our queen is the dragon with `ct.get_id() <= 1`. The tiebreak at round 500 compares queens first.
- Keep v10.3.3's survival mechanisms (grower reverse escape) unless replaced by layer 5.
