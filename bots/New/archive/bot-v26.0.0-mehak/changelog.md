# Changes from the new bot (nb = teammate's v25)

Every change is marked `// NB3:` in the source. Files changed: `main.cpp`, `scoring.hpp`.
Tested and dropped (not in the code): NB1 portal pockets for short workers (pockets with a loop),
NB1 2-pearl raids, NB2 queen keeps out of pockets. No gain in 30-36 native games each.

## Step 1: no centre pull on a portal crossing (`scoring.hpp`, goal_progress)
- The centre rush (rounds 0-74 on listed maps) charged a crossing for the whole jump: on Stripes
  about 11 path steps further from the centre = -3300 (3.3 pearls), against a blind-crossing risk
  of about -500. So nobody crossed early, and the refused crossing marked the far side a trap for
  15 rounds (refuse_route_portals).
- Now a portal crossing gets no centre pull either way.

## Step 2: pocket raids through a known portal (`main.cpp`, evaluate_single; switch `PORTAL_RAIDS`)
- Stripes' fast beds sit in dead-end pockets (3 tiles or 1 tile) reached only through a portal.
  A raid (go in with 3+ pearls there, eat, split so the rear walks back out) was allowed only for
  kelp dead ends (`!candidate.portal`). Now also through a known portal.
- Result (native, vs v22, Stripes/Portals/Devil x 5 seeds x 2 seats, 30 games):
  Stripes fast-bed pearls 12.5 -> 28.1 (v22 38.0), pearls by round 100 16 -> 30 (v22 38);
  Stripes wins 3 -> 2 of 10 (noise); Portals and Devil unchanged (9 and 10 of 10).
  Cost: raid remnants (2 long) die: our self-deaths on Stripes about 2 -> 9 a game.

## Step 3: queen splits like a worker early (`NB4`, main.cpp)
- `queen_keep()`: until round `QUEEN_EARLY_SPLIT_END` (50) she keeps `QUEEN_EARLY_KEEP` (2) and
  splits from length 4; afterwards 4 as before (seeding still ends at 75).
- Why: in 9 lost replays the opponent's queen split at length 4 keeping 2 (Trophy: 8 splits by
  round 63; ours 1 by round 30); they out-split us 3-5x by round 50.

## Step 4: defence (`NB4`, main.cpp + scoring.hpp)
- `count_enemies()` / `outnumbered()`: enemy dragons seen in the last 30 rounds vs our unit count.
- `defending()`: a non-valuable dragon of length <= 8, before round 50 or while outnumbered.
- A defending worker: reach threats are built for it; ending in an enemy head's reach costs 30% of
  its death, next to one 50%; the fight search prices a ram as our loss (before: an even ram
  counted as a gain on unit values, so nobody avoided it).
- Why: head-ons by round 100 in the 9 replays: they rammed us 55 times, we rammed them 15.

## Step 5: attack by numbers (`NB4`, main.cpp eligible_target)
- Before round 50 a worker may trade if we have at least 2 more dragons than enemies seen.
- While outnumbered, trades only into an enemy at least 2 longer (endgame hunt unchanged).

Test (native vs v22, Trophy/Devil/Autarky/Queen of Spades/Stripes x 2 seeds x 2 seats, 20 games):
win 45% -> 50%, our queen alive at end 4 -> 8 of 20, our dragons at r50 10.6 -> 11.1,
v22's dragons at r50 9.8 -> 8.2. Within noise at 20 games; v22 rams far less than the ladder
opponents, so the defence part needs a ladder or sandbox check against an aggressive bot.

## Step 6: short rams (`NB7`, main.cpp + scoring.hpp, switch `SHORT_RAMS`, default 1)
(NB5 and NB6 were tested and dropped; this is NB4 plus this step only.)
- `short_rammer()`: a worker (not the queen, not valuable, not retaining) of length <= 6, from
  round 50, with 6+ of us and not outnumbered.
- It may ram or sprint into shorter enemies too (`eligible_target`), and such a trade is worth at
  least +300 (`trade_value`), so the fight search takes it.
- Why: in the top team's 16 replays the winners rammed a shorter adjacent head 13% of the time,
  the losers 4%.
- Test (native, alone vs NB4, Devil/Autarky/Colosseum x 2 seeds x 2 seats): 6-6, no measurable
  difference at that sample size. Off: #define SHORT_RAMS 0 at the top of main.cpp.