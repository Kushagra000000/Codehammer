# bot-v10.4.1 — UNTESTED

Parent: the current local bot-v10.4, including its existing SPLIT_CUTOFF=300.
No builds, tests, matches, or submissions were run. No champion promotion.

## Requested changes

1. From round 475, retaining snakes of length 16+ can prefer short visible-pearl
   detours, including pearls dropped by donors. Search is bounded to four ordinary
   moves, so these routes do not pay sprint length. A new detour must collect more
   visible pearls, or the same number sooner, than normal patrol over the same
   number of turns. Each planned step is checked against known body occupancy,
   growth, obstacles and enemy threats. Rejoining must preserve circular body
   order and enough time for occupied cycle tiles to release before the head
   arrives. The route is revalidated each turn. If blocked while outside the
   cycle, a bounded alternate return is sought; existing survival/navigation
   takes priority when no safe return is available. Return cannot be guaranteed
   against other moving snakes or unseen changes.
2. Patrol rectangles are ranked by recurring spawn density, discounted for the
   area covered by the snake. Consecutively observed countdown resets estimate
   spawn intervals. Dropped pearls do not count as permanent spawners. Productive
   loops need at least max(80, length+20) tiles; unproductive/undersized rectangles
   keep the existing area-based fallback. A replacement for an established long
   snake's cycle needs consistent body order and 20% higher productivity when the
   old loop is productive. Search still uses known-clear rectangles containing
   the head, rather than rebuilding unrelated navigation.
3. Cornered workers and growers of length 4+ can escape by splitting a child of
   length L-2 from the old tail, leaving two segments in the parent. This emergency
   split bypasses normal role/round split cutoffs, but respects can_split(), the
   engine unit cap, complete body knowledge and a visible safe tail exit. Existing
   ordinary split behavior is preserved. Existing movement/sprint escapes are
   preferred when they provide a viable continuation. Long children (10+) use the
   inherited heir rule, including when their rescued parent was a worker.

## Editable configuration (main.cpp)

- PATROL_FORAGE_ROUND = 475
- PATROL_DETOUR_STEPS = 4
- PATROL_DETOUR_BONUS = 6500
- PATROL_GROWTH_ROOM = 20
- PATROL_RELOCATION_PERCENT = 120

Only main.cpp, coordination.hpp and new patrol_growth.hpp change implementation.
helper.hpp and bot.toml are unchanged. Other inherited settings, feeding, sonar,
combat, general movement and splitting schedules are preserved.

HYPOTHESIS: collecting additional late pearls and rescuing cornered bodies improve
final longest length. UNKNOWN: net win rate and runtime cost. A split preserves
team length immediately but reduces the longest rescued individual by two; it
is intended for situations with no viable continuation. Bounded detours remain
opportunistic, and spawn-rate estimates are based on partial observations.
