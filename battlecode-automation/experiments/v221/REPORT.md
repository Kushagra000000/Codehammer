# v22.1.0 verification report

Final candidate: `bots/New/current/bot-v22.1.0`, copied from v22.0.0. Two implementation iterations were used, within the requested maximum of four. The v22.0.0 source and the change-log were not edited.

The requested opening behavior is implemented and the final sample shows faster early population growth. It does **not** establish an overall competitive improvement: the final candidate won 6 and lost 8 games against v22.0.0, with 2 elimination wins and 3 elimination losses. Keep these results separate from the behavioral checks.

## Implemented behavior

- Opening scouting starts immediately on every map. Each dragon has a persistent destination; two of every three destination proposals are concentrated in the central half of each map axis, with the remainder spread across the whole map. Consecutive children use distinct sequence indices; both founders receive the same central proposal.
- Scouting ends for a destination as soon as its target becomes visible, its block is sufficiently explored, or the dragon enters its vision radius. The block is retired for that dragon's opening. A productive region continues to attract the existing pearl router; there is no geometric pull to remain after its pearls run out.
- Known food routes take priority over regional travel. Tests cover pearls two and three open steps away in the opposite direction, as well as eating to split length, executing `SPLIT 2`, and the parent resuming its unfinished destination. Newborns select their own destinations using their IDs and inherited sonar coverage.
- Unreachable destinations are replaced. Eight scouting moves without improvement retire a stalled destination. Food and fight detours reset that counter; a split does not consume a travel attempt.
- The opening policy ends at round 75; arrival never waits for that cutoff. The existing large-map exploration policy then applies. The queen's later cycle, combat, survival, portal handling, split safeguards, and endgame code were preserved.

Only `main.cpp`, `explore.hpp`, and `scoring.hpp` differ from v22.0.0. All seven other bot files are byte-identical.

## Iterations and checks

Iteration 1 removed the centre restoring pull and added temporary destinations with travel weight 240. The seven-map paired sample showed fewer repeated empty head positions, but weaker collection and splitting. A focused fixture then confirmed that 240 could override a real imminent-spawn objective worth 180 per step.

Iteration 2 reduced only the opening travel weight to 120. The new resource-priority fixture fails at 240 and passes at 120. No combat, queen, safety, or split thresholds were tuned. Source diffs are saved in [iteration1.patch](iteration1.patch) and [iteration2.patch](iteration2.patch).

- **54 behavior checks pass**, also under UndefinedBehaviorSanitizer with no diagnostics: [verification output](verification.txt).
- The new imminent-spawn regression fails against the first iteration: [regression output](iteration1-objective-regression.txt).
- The nonadjacent-food fixture also reproduced v22.0.0's original behavior: east toward centre instead of north toward the reachable pearl.
- Final source fingerprints match the sandbox run manifest. C++20 compilation, Python harness syntax, and changed-source whitespace checks passed.
- AddressSanitizer could not be used in this environment: even an empty-main probe stalled and timed out. The stalled attempt was stopped; it is not counted as a pass. UBSan and actual judge-sandbox tests completed.

## Final paired results

unswbc 1.2.9; judge sandbox; seed 22101; both sides on Default Small, Arena, Trophy, Portals, Default, Stronghold, and Big Empty. Fourteen final games, plus fourteen on iteration 1. The final engine results agree with decoded replay results, including queen, longest-dragon, total-length, and elimination results.

| Measure | v22.1.0 | v22.0.0 |
|---|---:|---:|
| Wins in final batch | 6 | 8 |
| Wins by elimination | 2 | 3 |
| Average population after 25 rounds | 9.64 | 8.86 |
| Average population after 50 rounds | 18.79 | 16.50 |
| Average population after 75 rounds | 19.21 | 17.57 |
| Successful splits in first 75 rounds | 531 | 512 |
| Pearls eaten in first 75 rounds | 1,302 | 1,240 |
| Early repeated-head-position proxy | 8.36% | 8.02% |
| Maximum CPU points per turn | 95,284,096 | 95,119,559 |
| Runtime / invalid-action errors | 0 | 0 |

The final candidate averaged about 13.9% more dragons at round 50 and ate 5.0% more early pearls in this sample. Its overall win rate and elimination results did not beat the baseline. Repeated positions also did not fall overall in the final batch; the proxy includes legitimate resource waiting and survival movement, and is not a direct measurement of regional attraction. The lifecycle tests establish that a completed regional target cannot pull the dragon back.

Population snapshots are measured after completed rounds; games ending earlier retain terminal population. Early events include the first 75 rounds, so short games have shorter observation windows. Both bots' trajectories change in a head-to-head match. One seed per map and adaptation between iterations are not sufficient to infer a reliable ladder win-rate or elimination-rate improvement.

Detailed final per-map results: [iteration 2 report](iteration2/report.md), [summary](iteration2/summary.json), [match ledger](iteration2/matches.jsonl), [source/map manifest](iteration2/manifest.json). The ledgers link local replay files (ignored by Git). The evaluator stages complete source fingerprints to avoid the toolkit's header-insensitive wasm build cache; results therefore use the intended source revision.

## Reproduce

From the repository root:

```sh
clang++ -std=c++20 -O2 -fsanitize=undefined battlecode-automation/scripts/test_v221.cpp -o /tmp/test_v221
/tmp/test_v221
python3 battlecode-automation/scripts/v221_eval.py --a bots/New/current/bot-v22.1.0 --b bots/New/current/bot-v22.0.0 --maps maps/default_small.map maps/arena.map maps/trophy.map maps/portals.map maps/default.map maps/stronghold.map maps/big_empty.map --seeds 22101 --out battlecode-automation/experiments/v221/recheck --sandbox --workers 2
```

The final candidate's full-source SHA-256 is `a0c6a11689283cc9b8e1e350352feba300f95c3a92a985a1be72c257c92bdcd8`.

Rules reviewed: [overview](https://game.battlecode.au/docs/overview), [scoring and structure](https://game.battlecode.au/docs/structure), [map topology](https://game.battlecode.au/docs/map-info), [pearls](https://game.battlecode.au/docs/pearls), [movement](https://game.battlecode.au/docs/movement), [splitting](https://game.battlecode.au/docs/splitting), [vision](https://game.battlecode.au/docs/vision), [kelp and portals](https://game.battlecode.au/docs/kelp-and-portals), [sonar](https://game.battlecode.au/docs/sonar), [execution order](https://game.battlecode.au/docs/execution-order), [judge limits](https://game.battlecode.au/docs/timeouts), and [wire protocol](https://game.battlecode.au/docs/protocol). The installed toolkit/engine and its replay decoder were also inspected.

Reminder: ask for a change-log update using only the verified changes and measured results above.
