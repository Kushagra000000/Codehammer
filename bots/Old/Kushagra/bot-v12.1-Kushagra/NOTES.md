# bot-v12.1-Kushagra

Based on bots/Kushagra/bot-v12.0-kushagra (cycle version). Only main.cpp changed; switches in `Config`.

1. Denial sprint (`DENIAL_SPRINT`, `add_denial_sprints`): before round 250, a non-grower worker (length >= 3)
   sprints 2 steps onto a visible pearl exactly 2 away when an enemy head is adjacent to that pearl
   (it would take it before our next turn). Net length change 0 (-1 sprint segment, +1 pearl). Logged `DENIAL_SPRINT`.
2. Assisted kill (`ASSIST_KILL`, `assist_target`): a dragon of length <= 4 that sees an enemy head with an
   allied head within 4 of it trades into that head by a 1-step move or a 2-step sprint (+2500 score), from
   round 0 (normal aggression still starts at round 50). The ally collects the drop.
3. Fix after run 1: hash-selected/founder/heir growers never denial-sprint or assist-trade
   (run 1 lost big_empty 0-4 with shorter kings).

Results vs bot-v10.3.3 (all 15 maps, seeds 1201-1202, both sides, 60 games):
- run 1 (initial): 37W-23L, 0.617
- run 2 (after fix): 38W-22L, 0.633 (95% CI 0.507-0.744); both-sides pairs won 10, lost 2 (sign test p=0.039)
- v12.0 on the same games: 34W-26L, 0.567
These seeds were also used for the one fix, so run 2 is slightly optimistic; confirm on fresh seeds.
