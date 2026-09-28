# bot-v10.4.1 rewrite — local validation, 2026-09-27

The three requested behaviors are implemented and tested. This is NOT a proven champion upgrade. Final implementation: bots/current/bot-v10.4.1, derived directly from the current v10.4 (grower cutoff300, ordinary split stop350 unchanged). Only main.cpp and coordination.hpp change implementation.

## Scope and bug fixes

- Rank known clear patrol rectangles by recurring pearl production; measured countdown resets favor faster spawn tiles. Keep an established productive loop. No global-map knowledge is assumed.
- Give the normal patrol bonus to a nearby visible pearl route within4 steps; retain existing collision/threat/space checks. No475 gate, four-turn planner, or blanket risk increase.
- Split a truly trapped worker or grower if legal; preserve L-2 in the tail child. Do not fragment a snake merely for low estimated future space. A doomed head no longer requires its distant tail to be visible; the child gets fresh local vision. Known sealed/threatened tails and engine split limits still prevent rescue.

## Actual verification

- C++20/O2 build and actual-code fixtures passed: spawn density/frequency selection, countdown reset detection, food overrides before475, threat priority, unchanged no-food patrol, worker/grower splits and child movement, unit cap/minimum size/sealed-tail guards, offscreen and incomplete-body tail rescues.
- Explicit isolated unswbc1.1.0 / Protocol3. Setup checker passed. CLI outcomes agree with decoded replay results in every match.
- 48 engine matches across development and validation; 16 used the final implementation. No unparsed games or reported runtime failures. Native and sandbox results are labeled separately.
- User replay M237041 matches local Stronghold in geometry, spawning, portals, kelp, starting dragons and limits; only map-name capitalization differs. Its151-length winner has NOT been reproduced by this rewrite.

## All results by revision

|Batch|Games|W|D|L|Errors|
|---|---:|---:|---:|---:|---:|
|v1041-parent-smoke|1|1|0|0|0|
|v1041-r1-smoke|1|0|0|1|0|
|v1041-r1-screen|11|8|0|3|0|
|v1041-r1-stronghold-validation|3|1|0|2|0|
|v1041-r1-sandbox|1|1|0|0|0|
|v1041-r2-diagnostic|4|1|0|3|0|
|v1041-r3-screen|11|6|0|5|0|
|v1041-r4-diagnostic|3|0|0|3|0|
|v1041-final-screen|11|6|0|5|0|
|v1041-final-sandbox|2|2|0|0|0|

The parent smoke is v10.4 vs v10.3.1; all candidate games use v10.4 as opponent. R1/R2/R3 are preserved development variants, not final results. FinalR4 totals:8W/0D/8L (native6W/8L; sandbox2W/0L). Fresh all-map screen6W/5L. Earlier diagnostic losses are retained, not discarded.

## Final implementation game details

|Batch|Map|Seed|Side|Mode|Result|Final longest ours / parent|Replay|
|---|---|---:|---|---|---|---:|---|
|v1041-r4-diagnostic|stronghold|104101|A|native|L|23 / 50|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-45378e8d74bdd8d85abef92b11546d62e157db673e76ee860a158f2a4723c499.replay)|
|v1041-r4-diagnostic|stronghold|104102|A|native|L|22 / 31|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-86d372db120cc07768e5e1499f7b715cf8350eea381eae2ad71a4b4f76a7b262.replay)|
|v1041-r4-diagnostic|stronghold|104301|A|native|L|26 / 35|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-2eff96675e4568d4c7c826f16fecf780e8b56d68cc1311d6786e6fe6cbf59b2f.replay)|
|v1041-final-screen|Colosseum|104401|A|native|L|0 / 4|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-ac360cf14c356ade3aac0eadb8e6eac08633f78a3a1c83e4f5ae4920f9ccd1f1.replay)|
|v1041-final-screen|arena|104401|B|native|L|0 / 4|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-f7696f91d0eba0e0b7dec457ff778c87ee4ca761048ebca7a41f3d5de6b0b0bd.replay)|
|v1041-final-screen|big_empty|104401|A|native|W|52 / 44|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-ff69d164c7356a430c307cc8103ec14120d24bf81d90b83cbd46731df4f4aaa4.replay)|
|v1041-final-screen|default|104401|B|native|L|18 / 25|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-d2321c4e8d7e21880438e1410a940e145e4d7b2ae7c72669a6461c8fa3e6ad1d.replay)|
|v1041-final-screen|default_small|104401|A|native|W|3 / 0|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-9b42f9a90ce74310b565a70dbd6695b5dc77330741ed24d9b8a4faf7a87de285.replay)|
|v1041-final-screen|devil|104401|B|native|W|4 / 0|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-e6428a375eb65bc1aab83041337ef0012971ea7b01f23855a4b07066ebc98ee7.replay)|
|v1041-final-screen|queen_of_spades|104401|A|native|W|3 / 0|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-81e7bb4e37ad56b73ddd12c43b6b275cb37426c5045927c9a0ba22ca1d25f61d.replay)|
|v1041-final-screen|schooltime|104401|B|native|L|30 / 33|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-e768bc07996e29a13be9298393d19efad88b566766ed763617d61222b0a89663.replay)|
|v1041-final-screen|stronghold|104401|A|native|L|31 / 34|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-43e4df8e257c12f514ea4e10d45214cffde1f20c4e3578fc4f93e1516fd72022.replay)|
|v1041-final-screen|trauma|104401|B|native|W|16 / 8|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-c44124441ee03e9c1a32a28a78fd1ac2eadf13f334b29551c4977852f5c6751c.replay)|
|v1041-final-screen|trophy|104401|A|native|W|4 / 0|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-068141acc46a45b152c65b70327b5980dfa49322510403bd8c118081f0a8f05a.replay)|
|v1041-final-sandbox|stronghold|104402|A|sandbox|W|38 / 35|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-67b9f3da5868c7b608a615a1c01165a4190d1be60c6f0d916c0ed3aad63c4c8f.replay)|
|v1041-final-sandbox|big_empty|104402|A|sandbox|W|52 / 46|[replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-0b9baf304ba1b7faab7bdb2ae01bed3b9eca8bc93cc40dd4cd5ea5a9c2c3c02b.replay)|

## Replay evidence and limitations

The final eleven-map native screen recorded176 patrol-food decisions,125 actual departures from the nominal cycle,167 decisions followed by pearl collection within four own turns, and30 collection windows including sacrifice drops. Windows can overlap and do not prove causal attribution. None of these decisions died in the same turn. There were135 actual escape splits;134 children survived their birth round and112 survived five rounds. Full event body reconstruction was checked against toolkit snapshots.

A distant-tail rescue can still fail: on Big Empty a length21 dragon split19, but an enemy killed the child before its first turn. Splitting does not guarantee survival. Stronghold final-native rescue children with offscreen tails survived20/20 birth rounds in this screen, yet longest growth remained weak.

Stronghold final-code games: four native losses, one sandbox win (38vs35). Big Empty reached52vs44 native and52vs46 sandbox; Schooltime lost30vs33 in the final screen. Do not infer universal improvement or select only the best replay. Larger late kings on Stronghold remain an unresolved resource-concentration problem outside these three changes.

## Development findings

R1 showed a real tail-visibility bug: forced self-deaths at47(Big Empty) and28(Schooltime) despite legal splits and clear offscreen tail exits in the full replay. The parent could not see those exits. R3 added last-resort offscreen/incomplete-tail rescue. R4 further avoids speculative splits while a legal head action exists. R1 andR2 growth results varied; none were silently promoted.

Existing rejected v10.4.1 is saved in rejected-v10.4.1/. Parent, all intermediate sources, exact schedules, raw outputs, game rows and analysis JSON remain in this directory. Replays remain under repository replays/. final-source-state.json records exact tested code bytes. No official submission or automatic champion promotion.

## Final replay and sandbox addendum

The13 final screen/sandbox games (excluding the three earlier final-code diagnostics)
show416 patrol-food decisions,295 deviations from cycle,153 immediate pearl
collections and395 collection windows within four own turns. None died on that
same turn.161 escape splits produced160 birth-round survivors;51 escapes had
tails outside parent vision and50 survived their birth round. No accidental
self/body deaths of snakes length16+ occurred in these13 games; enemy head
collisions still occurred. These are observed outcomes, not survival guarantees.

Stronghold sandbox: at round497, length38 dragon158 was trapped, split child438
of length36 from its distant tail, and the child moved north immediately. It
survived and regrew to38, finishing as the winning longest dragon versus35.
Replay: replays/research-67b9f3da5868c7b608a615a1c01165a4190d1be60c6f0d916c0ed3aad63c4c8f.replay.
This demonstrates the requested behavior; it is not an ablation proving that
the change alone caused the win. Final sandbox CLI maxima:7.2M CPU points on
Stronghold and16.0M onBigEmpty, with no recorded runtime failure.
