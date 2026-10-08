# v28 paired comparison

unswbc 1.2.9 sandbox; 54/54 completed games; 0 evaluation errors; campaign attempts 80/80.

| Opponent | Games | W / D / L | Elimination W / L | Runtime errors ours / theirs | Max points ours / theirs |
|---|---:|---|---|---|---|
| all | 54 | 22 / 0 / 32 | 8 / 16 | 0 / 0 | 95,204,947 / 94,635,915 |
| Top_20_bot | 18 | 4 / 0 / 14 | 0 / 8 | 0 / 0 | 94,889,612 / 4,572,435 |
| bot-v27.3.0-leo | 18 | 9 / 0 / 9 | 4 / 4 | 0 / 0 | 95,204,947 / 94,629,034 |
| bot-v27.2.0-leo | 18 | 9 / 0 / 9 | 4 / 4 | 0 / 0 | 95,204,947 / 94,635,915 |

Snapshots in summary.json and matches.jsonl are after completed rounds 25/50/75/100/200/380/440; early means the first 75 completed rounds. Early endings retain terminal values and record the actual completed_rounds. Queen death rounds are one-based (null means alive at match end). Pearls are replay removals verified against actual actor movement. LOG counts record emitted intent, not proof an action succeeded; replay split/death/pearl events provide outcome evidence. The replay result is checked against engine queen/longest/total/population tiebreak fields. Submitted errors or interrupted attempts consume the shared budget and are never automatically retried.

| Map | Opponent | Side | Seed | Result | Rounds | Queens at end ours / theirs | Queen death rounds ours / theirs | Early pearls ours / theirs |
|---|---|:---:|---:|---|---:|---|---|---|
| arena | Top_20_bot | B | 28001 | L | 18 | 0 / 4 | 18 / None | 6 / 32 |
| arena | Top_20_bot | A | 28001 | L | 19 | 0 / 3 | 18 / None | 7 / 30 |
| arena | bot-v27.3.0-leo | B | 28001 | W | 33 | 0 / 0 | 16 / 21 | 97 / 42 |
| arena | bot-v27.3.0-leo | A | 28001 | L | 45 | 0 / 0 | 21 / 16 | 63 / 131 |
| arena | bot-v27.2.0-leo | B | 28001 | W | 33 | 0 / 0 | 16 / 21 | 97 / 42 |
| trophy | Top_20_bot | A | 28001 | L | 45 | 0 / 0 | 45 / 23 | 15 / 30 |
| arena | bot-v27.2.0-leo | A | 28001 | L | 45 | 0 / 0 | 21 / 16 | 63 / 131 |
| trophy | bot-v27.3.0-leo | A | 28001 | W | 47 | 0 / 0 | 24 / 47 | 42 / 12 |
| trophy | Top_20_bot | B | 28001 | L | 65 | 0 / 0 | 65 / 24 | 30 / 73 |
| trophy | bot-v27.3.0-leo | B | 28001 | L | 47 | 0 / 0 | 47 / 24 | 12 / 42 |
| trophy | bot-v27.2.0-leo | A | 28001 | W | 47 | 0 / 0 | 24 / 47 | 42 / 12 |
| trophy | bot-v27.2.0-leo | B | 28001 | L | 47 | 0 / 0 | 47 / 24 | 12 / 42 |
| default | Top_20_bot | A | 28001 | L | 174 | 0 / 0 | 174 / 103 | 15 / 52 |
| default | Top_20_bot | B | 28001 | L | 165 | 0 / 0 | 66 / 17 | 16 / 59 |
| default | bot-v27.3.0-leo | A | 28001 | W | 171 | 6 / 0 | None / 141 | 29 / 22 |
| default | bot-v27.3.0-leo | B | 28001 | L | 173 | 0 / 0 | 125 / 161 | 22 / 29 |
| default | bot-v27.2.0-leo | A | 28001 | W | 171 | 6 / 0 | None / 141 | 29 / 22 |
| default | bot-v27.2.0-leo | B | 28001 | L | 173 | 0 / 0 | 125 / 161 | 22 / 29 |
| big_empty | Top_20_bot | B | 28001 | L | 500 | 0 / 0 | 216 / 263 | 284 / 221 |
| big_empty | Top_20_bot | A | 28001 | W | 500 | 0 / 0 | 178 / 189 | 235 / 251 |
| big_empty | bot-v27.3.0-leo | A | 28001 | W | 500 | 0 / 0 | 163 / 72 | 266 / 244 |
| big_empty | bot-v27.3.0-leo | B | 28001 | W | 500 | 0 / 0 | 72 / 163 | 244 / 266 |
| stronghold | Top_20_bot | A | 28001 | W | 500 | 82 / 49 | None / None | 52 / 29 |
| stronghold | Top_20_bot | B | 28001 | L | 500 | 0 / 53 | 363 / None | 58 / 32 |
| stronghold | bot-v27.3.0-leo | A | 28001 | L | 500 | 0 / 89 | 294 / None | 47 / 62 |
| stronghold | bot-v27.3.0-leo | B | 28001 | L | 500 | 0 / 69 | 331 / None | 62 / 47 |
| big_empty | bot-v27.2.0-leo | A | 28001 | W | 500 | 0 / 0 | 163 / 72 | 266 / 244 |
| dilemma | Top_20_bot | A | 28001 | L | 87 | 0 / 3 | 87 / None | 5 / 26 |
| dilemma | Top_20_bot | B | 28001 | L | 147 | 0 / 2 | 119 / None | 4 / 25 |
| big_empty | bot-v27.2.0-leo | B | 28001 | W | 500 | 0 / 0 | 72 / 163 | 244 / 266 |
| dilemma | bot-v27.3.0-leo | A | 28001 | W | 43 | 0 / 0 | 34 / 43 | 27 / 9 |
| dilemma | bot-v27.3.0-leo | B | 28001 | L | 43 | 0 / 0 | 43 / 34 | 9 / 27 |
| dilemma | bot-v27.2.0-leo | A | 28001 | W | 43 | 0 / 0 | 34 / 43 | 27 / 9 |
| dilemma | bot-v27.2.0-leo | B | 28001 | L | 43 | 0 / 0 | 43 / 34 | 9 / 27 |
| stronghold | bot-v27.2.0-leo | A | 28001 | L | 500 | 0 / 89 | 294 / None | 47 / 62 |
| stronghold | bot-v27.2.0-leo | B | 28001 | L | 500 | 0 / 70 | 331 / None | 62 / 47 |
| slithery_fight | Top_20_bot | A | 28001 | W | 500 | 69 / 7 | None / None | 295 / 319 |
| slithery_fight | Top_20_bot | B | 28001 | W | 500 | 74 / 8 | None / None | 294 / 326 |
| slithery_fight | bot-v27.3.0-leo | A | 28001 | W | 500 | 0 / 0 | 222 / 456 | 294 / 328 |
| slithery_fight | bot-v27.3.0-leo | B | 28001 | L | 500 | 0 / 64 | 460 / None | 309 / 301 |
| slithery_fight | bot-v27.2.0-leo | A | 28001 | W | 500 | 0 / 0 | 222 / 455 | 294 / 328 |
| slithery_fight | bot-v27.2.0-leo | B | 28001 | L | 500 | 0 / 58 | 443 / None | 309 / 301 |
| schooltime | Top_20_bot | A | 28001 | L | 500 | 3 / 3 | None / None | 67 / 68 |
| schooltime | Top_20_bot | B | 28001 | L | 500 | 3 / 3 | None / None | 108 / 73 |
| schooltime | bot-v27.3.0-leo | B | 28001 | W | 500 | 3 / 0 | None / 373 | 85 / 88 |
| schooltime | bot-v27.3.0-leo | A | 28001 | L | 500 | 3 / 3 | None / None | 88 / 85 |
| schooltime | bot-v27.2.0-leo | A | 28001 | L | 500 | 3 / 3 | None / None | 88 / 85 |
| schooltime | bot-v27.2.0-leo | B | 28001 | W | 500 | 3 / 0 | None / 373 | 85 / 88 |
| islands | Top_20_bot | A | 28001 | L | 500 | 0 / 0 | 78 / 282 | 124 / 134 |
| islands | Top_20_bot | B | 28001 | L | 500 | 0 / 0 | 417 / 144 | 143 / 110 |
| islands | bot-v27.3.0-leo | A | 28001 | W | 500 | 0 / 0 | 143 / 205 | 139 / 88 |
| islands | bot-v27.3.0-leo | B | 28001 | L | 500 | 0 / 0 | 138 / 224 | 88 / 139 |
| islands | bot-v27.2.0-leo | A | 28001 | W | 500 | 0 / 0 | 143 / 205 | 139 / 88 |
| islands | bot-v27.2.0-leo | B | 28001 | L | 500 | 0 / 0 | 138 / 224 | 88 / 139 |
