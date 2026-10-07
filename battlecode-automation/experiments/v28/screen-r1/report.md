# v28 paired comparison

unswbc 1.2.9 sandbox; 24/24 completed games; 0 evaluation errors; campaign attempts 24/80.

| Opponent | Games | W / D / L | Elimination W / L | Runtime errors ours / theirs | Max points ours / theirs |
|---|---:|---|---|---|---|
| all | 24 | 11 / 0 / 13 | 3 / 5 | 0 / 0 | 94,520,205 / 71,638,971 |
| Top_20_bot | 8 | 2 / 0 / 6 | 1 / 3 | 0 / 0 | 94,520,205 / 3,920,250 |
| bot-v27.3.0-leo | 8 | 4 / 0 / 4 | 1 / 1 | 0 / 0 | 94,424,800 / 70,024,976 |
| bot-v27.2.0-leo | 8 | 5 / 0 / 3 | 1 / 1 | 0 / 0 | 94,424,800 / 71,638,971 |

Snapshots in summary.json and matches.jsonl are after completed rounds 25/50/75/100/200/380/440; early means the first 75 completed rounds. Early endings retain terminal values and record the actual completed_rounds. Queen death rounds are one-based (null means alive at match end). Pearls are replay removals verified against actual actor movement. LOG counts record emitted intent, not proof an action succeeded; replay split/death/pearl events provide outcome evidence. The replay result is checked against engine queen/longest/total/population tiebreak fields. Submitted errors or interrupted attempts consume the shared budget and are never automatically retried.

| Map | Opponent | Side | Seed | Result | Rounds | Queens at end ours / theirs | Queen death rounds ours / theirs | Early pearls ours / theirs |
|---|---|:---:|---:|---|---:|---|---|---|
| default_small | Top_20_bot | B | 28001 | L | 177 | 0 / 0 | 52 / 75 | 20 / 60 |
| default_small | Top_20_bot | A | 28001 | L | 241 | 0 / 0 | 47 / 134 | 30 / 67 |
| default_small | bot-v27.3.0-leo | A | 28001 | W | 500 | 47 / 0 | None / 130 | 41 / 54 |
| default_small | bot-v27.3.0-leo | B | 28001 | W | 500 | 0 / 0 | 78 / 274 | 57 / 41 |
| default_small | bot-v27.2.0-leo | A | 28001 | W | 500 | 47 / 0 | None / 130 | 41 / 54 |
| default_small | bot-v27.2.0-leo | B | 28001 | W | 500 | 0 / 0 | 78 / 274 | 57 / 41 |
| maze | Top_20_bot | A | 28001 | W | 500 | 0 / 0 | 139 / 486 | 80 / 60 |
| maze | Top_20_bot | B | 28001 | L | 500 | 0 / 66 | 126 / None | 52 / 58 |
| maze | bot-v27.3.0-leo | A | 28001 | L | 500 | 0 / 0 | 101 / 391 | 80 / 49 |
| maze | bot-v27.3.0-leo | B | 28001 | L | 500 | 0 / 0 | 373 / 101 | 49 / 80 |
| portals | Top_20_bot | B | 28001 | L | 500 | 0 / 0 | 98 / 110 | 71 / 174 |
| portals | Top_20_bot | A | 28001 | L | 500 | 0 / 0 | 78 / 59 | 96 / 161 |
| maze | bot-v27.2.0-leo | B | 28001 | L | 500 | 0 / 0 | 373 / 101 | 49 / 80 |
| maze | bot-v27.2.0-leo | A | 28001 | W | 500 | 0 / 0 | 101 / 479 | 80 / 49 |
| portals | bot-v27.3.0-leo | A | 28001 | W | 500 | 0 / 0 | 184 / 99 | 97 / 53 |
| portals | bot-v27.3.0-leo | B | 28001 | L | 500 | 0 / 0 | 283 / 164 | 58 / 104 |
| stripes | Top_20_bot | B | 28001 | L | 68 | 0 / 3 | 68 / None | 2 / 41 |
| stripes | Top_20_bot | A | 28001 | W | 245 | 0 / 0 | 25 / 23 | 26 / 39 |
| stripes | bot-v27.3.0-leo | A | 28001 | L | 114 | 0 / 6 | 104 / None | 11 / 20 |
| stripes | bot-v27.3.0-leo | B | 28001 | W | 114 | 6 / 0 | None / 104 | 20 / 11 |
| stripes | bot-v27.2.0-leo | A | 28001 | L | 114 | 0 / 6 | 104 / None | 11 / 20 |
| stripes | bot-v27.2.0-leo | B | 28001 | W | 114 | 6 / 0 | None / 104 | 20 / 11 |
| portals | bot-v27.2.0-leo | A | 28001 | W | 500 | 0 / 0 | 184 / 99 | 97 / 53 |
| portals | bot-v27.2.0-leo | B | 28001 | L | 500 | 0 / 0 | 283 / 164 | 58 / 104 |
