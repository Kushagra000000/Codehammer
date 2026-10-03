# opening-region300-r80

Candidate: **bot-v10.5** (`6ea660ec45d8501678718419a95a0160efa18c8a93f7a7fb511a1f2eafe0d8ff`).

Hypothesis: Extend best central-region300 opening to80rounds, preserving food/crowd/barren gates

Completed 21/21; toolkit 1.1.0; one alternating side per opponent/map/seed.

Errors remain errors. Elimination comes from the replay end reason, not an inferred round cutoff. Score rate counts a draw as half; these reports do not promote a champion.

| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |
|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|
| TOTAL | 12 | 2 | 7 | 0 | 57.1% | 61.9% | 12/2/7 | 0/0/0 | 2.3 | 7 |

## Opponents

| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |
|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|
| bot-v10.1 | 3 | 1 | 3 | 0 | 42.9% | 50.0% | 3/1/3 | 0/0/0 | 1.6 | 4 |
| bot-v10.3.1 | 5 | 0 | 2 | 0 | 71.4% | 71.4% | 5/0/2 | 0/0/0 | 2.9 | 4 |
| bot-v11.1 | 4 | 1 | 2 | 0 | 57.1% | 64.3% | 4/1/2 | 0/0/0 | 2.6 | 7 |

## Maps

| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |
|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|
| Colosseum.map | 3 | 0 | 0 | 0 | 100.0% | 100.0% | 3/0/0 | 0/0/0 | 4.0 | 4 |
| arena.map | 1 | 2 | 0 | 0 | 33.3% | 66.7% | 1/2/0 | 0/0/0 | 1.3 | 4 |
| default.map | 2 | 0 | 1 | 0 | 66.7% | 66.7% | 2/0/1 | 0/0/0 | 3.3 | 7 |
| default_small.map | 2 | 0 | 1 | 0 | 66.7% | 66.7% | 2/0/1 | 0/0/0 | 2.7 | 4 |
| devil.map | 2 | 0 | 1 | 0 | 66.7% | 66.7% | 2/0/1 | 0/0/0 | 2.7 | 4 |
| queen_of_spades.map | 0 | 0 | 3 | 0 | 0.0% | 0.0% | 0/0/3 | 0/0/0 | 0.0 | 0 |
| trophy.map | 2 | 0 | 1 | 0 | 66.7% | 66.7% | 2/0/1 | 0/0/0 | 2.3 | 4 |

## Matches

| Map | Opponent | Seed | Side | Result | End reason | Round | Longest | Opponent longest | Seconds | Replay / error |
|---|---|---:|---|---|---|---:|---:|---:|---:|---|
| Colosseum.map | bot-v10.1 | 105101 | A | W | teamEliminated | 54 | 4 | 0 | 4.31 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-53d383e84304bd4db9976f7e730fbad3f2fc2befbab6cef415a8af0ce0a6ed17.replay) |
| arena.map | bot-v10.1 | 105101 | B | D | teamEliminated | 8 | 0 | 0 | 4.27 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-4e7fa0272436c94276d17b2d3f46dc0dcf8913796bb702fd6f49211add0de541.replay) |
| default.map | bot-v10.1 | 105101 | A | W | teamEliminated | 288 | 3 | 0 | 6.12 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-b0f470b122648deb990c24c7a2b142e87ae1554edf050788dcf27392ebbbc627.replay) |
| default_small.map | bot-v10.1 | 105101 | B | L | teamEliminated | 146 | 0 | 4 | 4.51 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-d6e4164ae5fa3b50c08e6ace41baf4a480f32ea9ac39520954f74d4ad27748fe.replay) |
| devil.map | bot-v10.1 | 105101 | A | W | teamEliminated | 92 | 4 | 0 | 4.55 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-4a7df8de07ddf45616c338e3af8594ff5fec66b3fd0c5ce93546540bfc4a7425.replay) |
| queen_of_spades.map | bot-v10.1 | 105101 | B | L | teamEliminated | 435 | 0 | 17 | 6.25 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-5bc778002f1ba0d96f1354b467bf6b18d2dcef9a3f6352dd68b557f4412a5a56.replay) |
| trophy.map | bot-v10.1 | 105101 | A | L | teamEliminated | 301 | 0 | 4 | 6.67 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-179f2e6612826396731a2d4023dd3c21ddca35f21994bd349e375ca2febeb569.replay) |
| Colosseum.map | bot-v10.3.1 | 105101 | B | W | teamEliminated | 90 | 4 | 0 | 5.67 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-fe1a1f808e377974dadc9814a940717cb6a724b7c7b8f3e7da4e7a33df6d9961.replay) |
| arena.map | bot-v10.3.1 | 105101 | A | W | teamEliminated | 77 | 4 | 0 | 6.16 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-0c274edf366c4b4b6ce1c51a470ad9082dedbdb06aea988bba2ed48d8d7c1244.replay) |
| default.map | bot-v10.3.1 | 105101 | B | L | teamEliminated | 278 | 0 | 5 | 8.36 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-93a19aec7e46da13f671d26240a51d144103f9959dd998b33ea31bfa2f656653.replay) |
| default_small.map | bot-v10.3.1 | 105101 | A | W | teamEliminated | 146 | 4 | 0 | 6.3 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-43b572a0f1c2a8c9c7a314b3f304371c970553c4f77d93f33f323e687e2bc167.replay) |
| devil.map | bot-v10.3.1 | 105101 | B | W | teamEliminated | 153 | 4 | 0 | 7.61 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-f393f50750927c9df8881ddde817bd45a34381d0e4a264261b0b4a17ecba350e.replay) |
| queen_of_spades.map | bot-v10.3.1 | 105101 | A | L | teamEliminated | 188 | 0 | 4 | 6.57 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-6f1394bd95d03cf5813aabbc76d2f987e8e2bbbe09545c6e5394d317f18e98fa.replay) |
| trophy.map | bot-v10.3.1 | 105101 | B | W | teamEliminated | 226 | 4 | 0 | 8.22 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-16b002a704e453ed57f3c3fe3a626818bc0c833518d0655ddc207b31225fbff9.replay) |
| Colosseum.map | bot-v11.1 | 105101 | A | W | teamEliminated | 92 | 4 | 0 | 5.72 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-40511c882c1f75af02f03f4b1f58453fde0bcc51c1d9822bad8ac550e4cbb0ad.replay) |
| arena.map | bot-v11.1 | 105101 | B | D | teamEliminated | 2 | 0 | 0 | 4.99 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-5411e76ded1bce1f6d89a44ccaa18efb95fefe0abf68e857e8a475a1e3901a22.replay) |
| default.map | bot-v11.1 | 105101 | A | W | teamEliminated | 373 | 7 | 0 | 8.9 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-43c91e5446f2507717447b463b1186917d5af9fe6e83b98724a71de62f3ee1f7.replay) |
| default_small.map | bot-v11.1 | 105101 | B | W | teamEliminated | 160 | 4 | 0 | 5.85 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-a1cec68f324397ffa7dc6d54e4dc9ec0fa66ac1af9b2bf32485ecf651a0b6a91.replay) |
| devil.map | bot-v11.1 | 105101 | A | L | teamEliminated | 154 | 0 | 4 | 7.47 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-a5dba1fd2b9c696946be774bcf40c7198432b5980fd550e1c9fa582b04a69384.replay) |
| queen_of_spades.map | bot-v11.1 | 105101 | B | L | teamEliminated | 159 | 0 | 3 | 6.07 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-a418892245c8057c4132a2f729cf2ea356170e9fabf4e8a6a6448fafa8514439.replay) |
| trophy.map | bot-v11.1 | 105101 | A | W | teamEliminated | 206 | 3 | 0 | 6.62 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-fb346a2d98536822d03c946a8c51989ce92cc55cebdc93391ac4740de4e274cf.replay) |
