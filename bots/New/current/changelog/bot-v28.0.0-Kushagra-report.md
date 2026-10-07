# bot-v28.0.0-Kushagra — changes and evaluation

Based on **bot-v27.3.0-leo**, with independent implementations informed by the reference team's behavior. The reference is currently stored as `../Top_20_bot` (the supplied Top_6 code, renamed). Existing bot versions and reference sources were left unchanged.

## Implemented changes

| Change | Behavior and limits | Implementation |
|---|---|---|
| Harvest known dead-end lanes | Before consolidation, ordinary workers on maps below 1,100 tiles defer routine splitting while a known inward branch has visible or near-due food. Once the entire forward branch is freshly observed empty, they can reverse with an `L−2` child, preserving most length at the old tail. Requires open outward space, clearance from visible enemy free-step reach and an available split slot. Unknown terrain, portals and sealed trees are not assumed to be exits. Queens, retainers and newborns are excluded. | [lanes.hpp](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v28.0.0-Kushagra/lanes.hpp) |
| Let newborns clear their parent | Small-map workers born after round zero skip routine splitting on their first turn. Queens and emergency escape splits retain their existing rules. This is one-turn protection, not a permanent change to reproduction thresholds. | [main.cpp](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v28.0.0-Kushagra/main.cpp) |
| Check whether feeding can help | A donor needs at least four friendly units and one new death pearl that the visible elected king can reach before the match ends. Checks actual even-index body drop locations, known body segments, visible nonportal routes and occupied cells; counts one movement step per remaining turn conservatively. Length-two donors remain eligible. Existing proximity, timing and enemy vetoes still apply. | [endgame.hpp](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v28.0.0-Kushagra/endgame.hpp) |
| Preserve the last survivor | The immediate enemy-queen head-trade shortcut cannot spend our last unit and thereby lose by elimination. | [queen.hpp](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v28.0.0-Kushagra/queen.hpp) |
| Correct teammate-exit scoring | Evaluate the resulting body, including tail cells released and sprint prefixes already vacated. Contested-pearl and escape sprints now receive the missing teammate/queen exit penalties. These remain score penalties, not absolute vetoes. | [scoring.hpp](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v28.0.0-Kushagra/scoring.hpp), [explore.hpp](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v28.0.0-Kushagra/explore.hpp), main.cpp |
| Correct continuation searches | Survival and king-plan searches cannot eat pearls already consumed by the proposed move a second time. Scoped reference counts restore correctly and compose with route search. | [survival.hpp](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v28.0.0-Kushagra/survival.hpp) |
| Preserve army escape behavior | Late army workers are no longer incorrectly classified as feeders when evaluating life-saving split escapes. | endgame.hpp |

The final lane safeguard closes an integration gap found after the tournament batch: the earlier threat fixtures populated hard/soft maps manually. New fixtures build the production worker maps from enemy heads and verify the veto for both teams. Ordinary emergency escapes remain available.

The lane and feeding ideas use our own map, body, threat, split-handover and king structures. They are not pasted reference functions. Team A/B fixtures exercise the same rules. Existing sonar packets, queen cycle/crown policy, v27.3 late-queen caution, optional wait limit and sealed-queen handling are preserved.

This version does **not** introduce the reference's adaptive economic clock, hunger messages, outward lane relay children, different queen opening split threshold, or sole-exit trapping attack. Those are separate strategic experiments with wider interactions.

## Match results (initial v28 revision)

| Opponent | Games | Wins / draws / losses | Win rate | Elimination wins / losses |
|---|---:|---:|---:|---:|
| Top_6 reference (folder `Top_20_bot`) | 26 | 6 / 0 / 20 | 23.1% | 1 / 11 |
| bot-v27.3.0-leo | 26 | 13 / 0 / 13 | 50.0% | 5 / 5 |
| bot-v27.2.0-leo | 26 | 14 / 0 / 12 | 53.8% | 5 / 5 |
| **All candidate matches** | **78** | **33 / 0 / 45** | **42.3%** | **11 / 21** |

**Zero runtime errors on either side in the tournament batch; zero evaluation errors.** The tournament revision’s peak use was **95,204,947 points**, below the 100-million-point turn limit. Screening: 11–13. Additional-map validation: 22–32.

**Treat v28 as a candidate, not a proven replacement for v27.3.** It contains reproduced correctness fixes and functioning new behaviors, but ties its parent in this sample and remains well behind the reference. Against both own baselines it won both sides on `default_small` and `big_empty`, but lost both sides on `stronghold`; against v27.3 it also lost both `maze` games. Those regressions are not explained away by the aggregate score.

All matches use the installed **unswbc 1.2.9 judge WASM sandbox**, seed **28001**, both starting sides, and a maximum of four parallel games per main batch. **The 24-game screen and 54-game validation use the same frozen initial v28 candidate.** Final review then added one narrow safeguard: lane splitting now also checks the normal worker enemy-reach map, because ordinary workers leave the hard/soft maps empty. The delivered folder includes this fix, tested with real-head threat-map fixtures and a fresh WASM regression run; it was not re-run in full tournament matches. Consequently, the table reports the initial revision, not a measured win rate for the final safeguard. Validation covers nine additional maps; combined coverage is **13 of the repository's 25 maps**:

`default_small`, `maze`, `portals`, `stripes`, `arena`, `trophy`, `default`, `big_empty`, `stronghold`, `dilemma`, `slithery_fight`, `schooltime`, `islands`.

Two additional v27.3-versus-reference opening controls bring the campaign to **80 full matches total**. Tiny scripted regression fixtures are reported separately below. [Screen report](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/screen-r1/report.md) · [Validation report](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/final-r1/report.md) · [Opening controls](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/baseline-opening/report.md) · [Budget ledger](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/game-budget.jsonl)

A single seed and two closely related baseline opponents limit the evidence. These are head-to-head results for this combined change set, not per-feature ablations or proof of performance against the live field.

## What the replays establish

The installed viewer's packed-Cap'n-Proto decoder successfully parsed the replays. Analysis extracts events and snapshots; it does not print entire replay files. Reported winners and end-state queen/longest/total-length figures are checked against the engine.

- In the screen, **238 lane-depletion intentions corresponded to 238 actual splits**, primarily on `portals`; **580 feeding intentions corresponded to 580 donor deaths**. This proves execution, not that every resulting pearl was ultimately eaten by the intended recipient. [Action verification](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/screen-features.json)
- Against the reference on `default_small`, v28 and v27.3 produced **identical semantic event sequences** for both sides: 13,073 events on B and 21,422 on A. Only computation costs differed. Both lost by elimination at rounds 177/241, so those opening weaknesses were inherited.
- In those openings, our original worker first found food at round 26 on A and died without eating on B. Reference workers first ate at rounds 16/14. By round 25 our population was three versus eight/five. The reference's earlier reproduction and resource acquisition deserve a separate opening experiment; this version does not resolve that gap.
- Our queens' deaths at rounds 47/52 occurred in the baseline too. Replays prove the deaths, but do not by themselves prove a safe alternative existed.

## Regression verification

- **59 lane/newborn checks under UBSan:** both teams, food/depletion, unknown terrain, portals, threats built from real enemy heads, reserves, queen/large-map exemptions, first-turn hold and next-turn restoration. The disabled-lane build also passes.
- **27 native feeding checks:** mirrored teams, length-two donor, death-drop parity, existing pearls, incomplete body knowledge, wrapping, blocked/portal-only routes, deadlines, enemy veto and army escape. A four-step kelp detour is rejected when only two collection turns remain.
- **51 final sandbox/engine queen checks:** seven tiny scripted engine fixtures, repeated legal two-segment waiting, minimum length, released blockers, unit cap, crowned last-slot privilege and boxed-child behavior. These preserve the earlier requested queen mechanism.
- Safety regressions reproduce and fix tail-release false positives, missed sprint penalties and last-unit queen trading. Survival/plan fixtures verify the consumed-pearl loop and count cleanup. Independent UBSan checks with constructor elision disabled verify nested/moved scopes and a five-step paid sprint that releases an earlier path cell.
- Feeding's address/undefined-sanitizer run stalled in this environment; no sanitizer pass is claimed for that suite. Its native checks and tournament sandbox execution passed.

Tests and runners are under [battlecode-automation/scripts](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/scripts): `test_v28_{lanes,feeding,safety,survival,postmove}.cpp`, `test_v28_queen.py`, `v28_eval.py`, and `v28_replay_features.cjs`. The evaluator snapshots all source headers and maps by content hash, retains full local replays, and enforces the shared 80-attempt ledger including errors/interruption. [Saved verification output](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/verification)

Final-source audit: only `lanes.hpp` differs from the tournament snapshot, by the two enemy-reach vetoes described above. [Exact final safeguard diff](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/verification/final-safety.patch) · [Final source hashes](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/verification/final-source-check.json) · [Final queen test output](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v28/verification/queen-final.txt)
