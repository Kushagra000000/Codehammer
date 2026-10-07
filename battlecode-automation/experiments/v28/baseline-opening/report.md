# v28 paired comparison

unswbc 1.2.9 sandbox; 2/2 completed games; 0 evaluation errors; campaign attempts 26/80.

| Opponent | Games | W / D / L | Elimination W / L | Runtime errors ours / theirs | Max points ours / theirs |
|---|---:|---|---|---|---|
| all | 2 | 0 / 0 / 2 | 0 / 2 | 0 / 0 | 9,644,483 / 3,722,483 |
| Top_20_bot | 2 | 0 / 0 / 2 | 0 / 2 | 0 / 0 | 9,644,483 / 3,722,483 |

Snapshots in summary.json and matches.jsonl are after completed rounds 25/50/75/100/200/380/440; early means the first 75 completed rounds. Early endings retain terminal values and record the actual completed_rounds. Queen death rounds are one-based (null means alive at match end). Pearls are replay removals verified against actual actor movement. LOG counts record emitted intent, not proof an action succeeded; replay split/death/pearl events provide outcome evidence. The replay result is checked against engine queen/longest/total/population tiebreak fields. Submitted errors or interrupted attempts consume the shared budget and are never automatically retried.

| Map | Opponent | Side | Seed | Result | Rounds | Queens at end ours / theirs | Queen death rounds ours / theirs | Early pearls ours / theirs |
|---|---|:---:|---:|---|---:|---|---|---|
| default_small | Top_20_bot | B | 28001 | L | 177 | 0 / 0 | 52 / 75 | 20 / 60 |
| default_small | Top_20_bot | A | 28001 | L | 241 | 0 / 0 | 47 / 134 | 30 / 67 |
