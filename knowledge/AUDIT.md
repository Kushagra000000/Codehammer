# v10.3.3: death and decision audit — 30 September 2026

## Scope and limits
Reviewed the supplied 276,900-line verbose run through event extraction, its saved replay, and the decision paths in main.cpp, coordination.hpp and endgame.hpp. This is a prioritized, line-referenced review, not a claim that every source line has been formally verified. No bot was modified and no new match was run. `Knowledge/game_rules.md`, `Knowledge/Strategy.md` and `Knowledge/experiment_log.md` were absent during this audit; use installed engine behavior to verify rules. Current source fingerprints are saved separately; the exact source/WASM identity at the original run is not established.

Input: `analysis.txt`; replay: `replays/bot-v10.3.3-vs-bot-v10.5-on-schooltime-2026-09-30-135117.replay`. A is v10.3.3, B is v10.5 according to the supplied command. `extract.cjs` reconstructs events and checks bodies against every round-start replay snapshot. `death-audit.json` contains ALL 712 deaths, each victim’s last four turns, nearby heads and their actions from death−1 through death+2, and traced death-pearl collections. The verbose log lacks full candidate scores, so a fatal chosen move does not prove a safe alternative existed. Replayed omniscient positions are analysis evidence, not information the bot may use.

## What happened
OBSERVED: v10.3.3 won at round 500: longest 33 versus 25; total length 337 versus 123; survivors 20 versus 10. This match is not evidence that v10.3.3 is generally stronger.

A had 399 splits and 382 deaths: 163 head collisions, 212 self-collisions, seven other-body collisions. Every self-collision has an explicit logged intent: 92 TRAPPED_SELF, 86 REMNANT_CLEAR, 34 FEED. An intentional action can still be strategically poor, but these are not unexplained accidental self-crashes. B had 337 splits and 330 deaths (155 head, 166 self, nine body). No A kelp/invalid-action fatalities occurred.

A logged 89 GROWER_REVERSE escapes; all children survived their birth round. Dragon 617 reached length 37 at the start of round 464. At round 477 it split 28 of its 30 segments into 738; at 497, 738 split 33 of 35 into 740. The two-segment parent cleared itself at 498. Preserve this successful survival mechanism; it protected the eventual winning length rather than merely wasting snakes.

## Decisive timelines and reactions

| Incident | Before / death / after | Interpretation |
|---|---|---|
| A533, length 14, r350 | r347–349 moved east. r350 moved (56,10)→(57,10). Later B590, length 3, sprinted N,W from (58,11); both died. See log 209693–209840. Nearby A478 continued east; A377 moved S,S,E over r350–352. | A533 advertised grower status. Enemy head was visible, but its other two segments were across a portal at (0,21),(0,20). The bot cannot infer exact length from that head. main.cpp:1280 uses lower bound 2, so its guaranteed reach is only one step; the actual two-step attack falls into uncertain/soft reach if modeled. Investigate portal-truncated body classification and candidate scores before asserting the chosen move was avoidable. All seven drop pearls were later collected by allies, not concentrated into nearby A478. |
| A478, length 18, r366 | r363–365 moved south. r366 moved (4,18)→(5,18). B558, length 7, then sprinted S,W,W,W from (8,17) and killed it. See log 221550–221668. | Enemy head was four columns away before the move, outside vision, and three afterwards. This is an observable vision-fringe ambush. Eight of its nine death pearls eventually went to enemies. A651 continued W,S,E in r366–368; no explicit death response is logged. |
| A676, length 16, r402 | Approached N,N,W, then reversed E into itself at (31,33): FEED 617 len 16, log 245882. King 617 moved north and grew 22→23→24 over r402–404. | Designated recipient 617 collected only 2/8 donor pearls; 643 got five, 738 got one. Since 738 is 617’s eventual escape child, the lineage collected 3/8. Feeding helped the team but poorly concentrated growth. |
| A721, length 13, r445 | Moved W,W,W then reversed E: FEED 562 len 13, log 261924. Recipient 562 was also length 13 and moved W,W,N over r445–447 without growing. | Designated id 562 collected 0/7; ids493,341,729 collected 3,3,1. Election tie-breaking can approve an equal-sized recipient. Suicide near the claimed head is insufficient to ensure the intended snake harvests the body’s pearl trail. |

Nearby movement is observable; whether a snake mentally “reacted” cannot be recovered from these logs. The code reacts to visible pearls, changed occupancy and received messages, rather than a reliable global death event. Local role/king records can remain stale after death or escape.

All seven A other-body deaths crossed portal edges: (round,id)=(164,129),(212,208),(285,0),(357,614),(358,212),(391,651),(422,494). This identifies remote landing occupancy as a useful investigation target. Without the total portal-use denominator, it does NOT justify claiming portals usually fail or banning them.

## Line-referenced changes to investigate
Line numbers refer to the fingerprinted current v10.3.3 source. CONFIRMED means a static code/property mismatch, not proven cause of a particular match death.

| Source | Finding and narrowly scoped change |
|---|---|
| coordination.hpp:177–188; endgame.hpp:766 onward | CONFIRMED topology overclaim: share_terrain marks every internal rectangle edge EMPTY. Territory validation certifies the cycle edges, allowing unused interior kelp. Share only the edges of the announced cycle variant, or explicit verified edge records; never fabricate chords. Preserve unknown edges and provenance. Add a rectangle-with-interior-kelp fixture. |
| coordination.hpp:64–76; endgame.hpp:184 | CONFIRMED freshness conflation: head sightings refresh records while retaining a previous larger length through max(old,visible). Split/sprint shrinkage can look like a fresh exact length. Separate position, exact-length and lower-bound timestamps. An incomplete sighting must not renew old exact length or role certainty. |
| main.cpp:2162 onward; endgame.hpp:705–708 | REVIEW stale identity after escape: status advertises predicted length 2 but king beacon may still describe the pre-split large parent. Invalidate/downgrade parent king state on escape; design child handoff within actual protocol constraints. Never assume the parent already knows the new child ID. |
| endgame.hpp:527–569 | CONFIRMED deadline mismatch: fallback checks Manhattan distance against rounds_left, but BFS accepts any reached target without checking actual path length n against that deadline. Require route ETA plus harvest margin before approving sacrifice. |
| endgame.hpp:537–548,588 onward | OBSERVED donation inefficiency: acceptance tests donor head position, not where the complete corpse drops pearls or who reaches them first. Require a fresh recipient trajectory, projected drop cells, collection deadline and competition estimate. Keep the donor alive when transfer utility is poor. Treat the 2/8 and 0/7 examples as fixtures, not proof every donation fails. |
| endgame.hpp:682–717 | CONFIRMED exception to “all cornered snakes escape”: complete-body workers cannot escape after SACRIFICE_ROUND, even far from the king. Allow feasible survival if death is not a verified useful donation. Also rename certain_death: hard threat or zero exits is a risk heuristic, not mathematical certainty. Preserve the successful child-survival behavior. |
| main.cpp:859–900,1261–1382 | REVIEW portal-discontinuous enemy traces and hidden sprint budgets. A533’s attacker had only its head locally visible. Keep exact/lower-bound/unknown length distinct; ensure unknown portal continuations cannot certify a complete body. Evaluate conservative uncertain-reach risk for valuable growers with opportunity-cost controls. |
| main.cpp:1320–1382 | OBSERVED fringe blind spot: reach threats start from visible heads; A478’s killer was outside the pre-move window. Add age-limited known enemy reach and bounded uncertainty at newly exposed destinations; do not hard-block every unseen tile or supply replay-only information. |
| main.cpp:1938–2035, especially1963–1980 | CONFIRMED search abstraction limitation: escape BFS keeps one route per head cell although routes differ in body positions, eaten pearls and remaining length. Retain a bounded number of distinct body/heading states or justify safe dominance pruning. Use one consistent safety predicate for step and sprint candidates. Benchmark sandbox costs. |
| endgame.hpp:354 onward | REVIEW shortcut invariant: cycle gap arithmetic is safe only when the body remains ordered on that cycle. Validate actual body indices after detours/adoption/rebuild; otherwise use safe rejoining rather than assuming order. This match does not prove long-snake self-collision caused by this code. |
| endgame.hpp:766 onward | FEATURE, not bug: choosing maximal cycle area does not optimize sustained pearl yield. Test known spawn/collection rate and safe capacity together; preserve working cycles unless estimated benefit pays relocation risk. |
| main.cpp:696–719;2162 onward | MAINTENANCE: post-400 early election return bypasses vice logic; current sonar direction condition sends beacons on three directions despite a two-direction comment. Document actual policy and measure message allocation before changing it. |

Do not rewrite the Hamiltonian subsystem, weaken exact body simulation, reduce early splits, or change grower counts on the strength of this one match. Highest-confidence first fix: truthful terrain sharing. Most directly observed strategic opportunity: increase useful pearl transfer to a surviving longest snake while protecting it from uncertain sprint attacks.

## Reproduce extraction
From repository root:
```
node battlecode-automation/experiments/v1033-audit-20260930/extract.cjs battlecode-automation/experiments/v105-20260927T131901Z/experiments/validation-parent/decoder.cjs replays/bot-v10.3.3-vs-bot-v10.5-on-schooltime-2026-09-30-135117.replay battlecode-automation/experiments/v1033-audit-20260930/death-audit.json
```
Decoder is a pre-existing extracted toolkit decoder. Pearl attribution tracks explicit death tile changes until collection; it is not a claim about unobserved strategy intent. Do not commit replays.
