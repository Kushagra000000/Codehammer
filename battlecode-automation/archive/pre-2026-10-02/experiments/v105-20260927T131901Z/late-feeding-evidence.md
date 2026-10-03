# Late alpha feeding: observed replay evidence

All sixteen screening pairs compare identical map, opponent, seed and side. No validation results were read.

| Timing | W/L | Mean longest | Feed deaths, rounds 450–474 | Donors length ≥10 | Recent grower flags, length >4 | Top-two donors | Recipient-family pearls collected |
|---|---:|---:|---:|---:|---:|---:|---:|
| late-alpha475 | 9/7 | 29.50 | 59 | 15 | 3 | 1 | 137 |
| late-alpha450 | 11/5 | 29.75 | 79 | 26 | 33 | 5 | 166 |

All action/state event prefixes before 450 identical: true.

| Map | Opponent | Seed/side | Outcome 475→450 | Longest 475→450 | Feeds before 475: cutoff 475→450 |
|---|---|---|---|---:|---:|
| big_empty.map | bot-v10.3.1 | 105201/A | W→W | 52→46 | 2→2 |
| big_empty.map | bot-v10.3.1 | 105202/B | L→L | 39→46 | 8→10 |
| schooltime.map | bot-v10.3.1 | 105201/B | W→W | 50→50 | 7→14 |
| schooltime.map | bot-v10.3.1 | 105202/A | L→L | 24→25 | 3→3 |
| stronghold.map | bot-v10.3.1 | 105201/A | L→W | 22→26 | 0→5 |
| stronghold.map | bot-v10.3.1 | 105202/B | W→L | 35→24 | 6→9 |
| trauma.map | bot-v10.3.1 | 105201/B | W→W | 13→14 | 1→0 |
| trauma.map | bot-v10.3.1 | 105202/A | W→W | 9→9 | 0→0 |
| big_empty.map | bot-v11.1 | 105201/B | W→W | 62→57 | 12→12 |
| big_empty.map | bot-v11.1 | 105202/A | L→W | 43→48 | 6→6 |
| schooltime.map | bot-v11.1 | 105201/A | W→W | 27→23 | 0→1 |
| schooltime.map | bot-v11.1 | 105202/B | L→W | 25→33 | 2→3 |
| stronghold.map | bot-v11.1 | 105201/B | L→L | 17→23 | 4→3 |
| stronghold.map | bot-v11.1 | 105202/A | W→W | 34→34 | 8→11 |
| trauma.map | bot-v11.1 | 105201/A | W→W | 11→9 | 0→0 |
| trauma.map | bot-v11.1 | 105202/B | L→L | 9→9 | 0→0 |

Interpretation limits:

- No validation results read.
- Recent grower status is from the preceding sonar, not a reconstruction of role recomputation on the donation turn.
- Recipient collection includes split descendants.
- A donated backup followed by losing is an association; the replay cannot establish what retaining it would have done.

Individual donations, collector events and recipient survival are retained in late-feeding-evidence.json.

## Concrete events

- **Activation:** Schooltime versus v11.1, seed 105202, side B: dragon 239 (length 13, recent grower status) self-feeds dragon 465 (length 18) at round 450.
- **Successful concentration:** In that game, second-longest dragon 679 (length 18) feeds dragon 427 (length 22) at round 474. Recipient 427 collects 5 of 9 dropped pearls and finishes at length 33, beating 28. The control with cutoff 475 finishes at length 25 and loses.
- **Negative backup example:** Stronghold versus v10.3.1, seed 105202, side B: second-longest dragon 135 (length 13) feeds dragon 362 (length 14) at round 453. The recipient collects 2 of 7 dropped pearls and finishes at 24, losing to 29. In the control with cutoff 475, dragon 135 remains alive and finishes at 35, winning against 29. Later interactions also change; this does not establish an isolated counterfactual.

The aggregate increase in longest dragon is only 0.25 (29.50→29.75), despite 9→11 wins. Earlier feeding activates and can help, but is not uniformly beneficial.
