# validation-head-to-head

Candidate: **bot-v10.5** (`5b3a1910a23e76454fb228eedbaf16f757b96f7cd7bbfd8279563941e6f0da32`).

Hypothesis: Frozen selected v10.5 directly versus parent on everymap,eachsideacrosstwofreshseeds

Completed 22/22; toolkit 1.1.0; one alternating side per opponent/map/seed.

Errors remain errors. Elimination comes from the replay end reason, not an inferred round cutoff. Score rate counts a draw as half; these reports do not promote a champion.

| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |
|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|
| TOTAL | 11 | 0 | 11 | 0 | 50.0% | 50.0% | 8/0/6 | 3/0/5 | 13.8 | 65 |

## Opponents

| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |
|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|
| bot-v10.4.1 | 11 | 0 | 11 | 0 | 50.0% | 50.0% | 8/0/6 | 3/0/5 | 13.8 | 65 |

## Maps

| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |
|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|
| Colosseum.map | 1 | 0 | 1 | 0 | 50.0% | 50.0% | 1/0/1 | 0/0/0 | 1.5 | 3 |
| arena.map | 1 | 0 | 1 | 0 | 50.0% | 50.0% | 1/0/1 | 0/0/0 | 2.0 | 4 |
| big_empty.map | 1 | 0 | 1 | 0 | 50.0% | 50.0% | 0/0/0 | 1/0/1 | 59.5 | 65 |
| default.map | 1 | 0 | 1 | 0 | 50.0% | 50.0% | 1/0/1 | 0/0/0 | 2.0 | 4 |
| default_small.map | 0 | 0 | 2 | 0 | 0.0% | 0.0% | 0/0/2 | 0/0/0 | 0.0 | 0 |
| devil.map | 2 | 0 | 0 | 0 | 100.0% | 100.0% | 2/0/0 | 0/0/0 | 4.0 | 5 |
| queen_of_spades.map | 1 | 0 | 1 | 0 | 50.0% | 50.0% | 1/0/1 | 0/0/0 | 1.5 | 3 |
| schooltime.map | 1 | 0 | 1 | 0 | 50.0% | 50.0% | 0/0/0 | 1/0/1 | 40.0 | 49 |
| stronghold.map | 0 | 0 | 2 | 0 | 0.0% | 0.0% | 0/0/0 | 0/0/2 | 20.0 | 26 |
| trauma.map | 1 | 0 | 1 | 0 | 50.0% | 50.0% | 0/0/0 | 1/0/1 | 14.0 | 20 |
| trophy.map | 2 | 0 | 0 | 0 | 100.0% | 100.0% | 2/0/0 | 0/0/0 | 7.0 | 11 |

## Matches

| Map | Opponent | Seed | Side | Result | End reason | Round | Longest | Opponent longest | Seconds | Replay / error |
|---|---|---:|---|---|---|---:|---:|---:|---:|---|
| Colosseum.map | bot-v10.4.1 | 105901 | A | L | teamEliminated | 57 | 0 | 3 | 11.38 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-e56fb26ad52c5519080058249a3d914df3456839bae5bd9a67ff60e084ebb702.replay) |
| Colosseum.map | bot-v10.4.1 | 105902 | B | W | teamEliminated | 65 | 3 | 0 | 11.4 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-a1713d7adb4a3081bf21b0cc338bfd5294d226a47d5944b3daac80727bd8ccef.replay) |
| arena.map | bot-v10.4.1 | 105901 | B | W | teamEliminated | 51 | 4 | 0 | 10.2 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-e73dd841536b1a4c90b21732081612d5470da95d83571e48519dccdda81b3ecf.replay) |
| arena.map | bot-v10.4.1 | 105902 | A | L | teamEliminated | 62 | 0 | 4 | 10.23 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-664e64db17fc9249b52aa4c2796cde83c0301277ba32bed66f5dd01d90eb7bec.replay) |
| big_empty.map | bot-v10.4.1 | 105901 | A | W | roundLimit | 500 | 65 | 53 | 47.0 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-c2b361a3700d21ef22647e186be6a3c6db0080f59a4e403bb95151b7b089f8fe.replay) |
| big_empty.map | bot-v10.4.1 | 105902 | B | L | roundLimit | 500 | 54 | 61 | 44.02 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-773e4ae68415b45ff346d33c916cba0df9752bccf14374cd742c90b85dce27ab.replay) |
| default.map | bot-v10.4.1 | 105901 | B | W | teamEliminated | 225 | 4 | 0 | 19.55 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-40a8647d522d8f1c584e6f889c82c4265a1dd971061fb66b5a46985626e7273e.replay) |
| default.map | bot-v10.4.1 | 105902 | A | L | teamEliminated | 250 | 0 | 4 | 31.36 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-19c8180fa27d8f3ba15036a65e61b68eda1e06845961508e974d793a25ee4bba.replay) |
| default_small.map | bot-v10.4.1 | 105901 | A | L | teamEliminated | 111 | 0 | 3 | 30.69 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-6d181316cd2acf03766b390f75f312914793e5eb7ebe92f9bef9c52186efee91.replay) |
| default_small.map | bot-v10.4.1 | 105902 | B | L | teamEliminated | 111 | 0 | 3 | 23.98 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-24037bfadaa5d3933c9fd23d63d98996456a6fd17d1c28a469f375aea52f2163.replay) |
| devil.map | bot-v10.4.1 | 105901 | B | W | teamEliminated | 207 | 5 | 0 | 19.62 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-78b530214bcc9eb45f6439043891447899dd2010a05bb89a9a88bb2d0fa53f89.replay) |
| devil.map | bot-v10.4.1 | 105902 | A | W | teamEliminated | 170 | 3 | 0 | 15.31 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-676f8884ca82b1883a0cff5c2c058642ffeb870149a88c8b38c21049f01ada09.replay) |
| queen_of_spades.map | bot-v10.4.1 | 105901 | A | W | teamEliminated | 163 | 3 | 0 | 12.17 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-05a56973f47b13e1eaec9f6140f53deb7e1568e3ce1bbe7f2ee0cd358b6467a7.replay) |
| queen_of_spades.map | bot-v10.4.1 | 105902 | B | L | teamEliminated | 116 | 0 | 4 | 13.14 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-e0cbdb9f3fb50996ddb93949c774b8d2bc37d56eab59b44fc2acac612467ddd9.replay) |
| schooltime.map | bot-v10.4.1 | 105901 | B | W | roundLimit | 500 | 49 | 28 | 30.67 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-097dab57bc773e32fb8eb717f95c638f94620ca6c007dd6a502768f9801b906d.replay) |
| schooltime.map | bot-v10.4.1 | 105902 | A | L | roundLimit | 500 | 31 | 33 | 39.56 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-d13fb65b8b8039c1625731772234c2111709fa453f5c0e988a52679a788b8622.replay) |
| stronghold.map | bot-v10.4.1 | 105901 | A | L | roundLimit | 500 | 26 | 33 | 40.57 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-d2c6763d48cb962bede7d89314dbe9e27da2ac7f4046f32ad28dae9bce04de58.replay) |
| stronghold.map | bot-v10.4.1 | 105902 | B | L | roundLimit | 500 | 14 | 19 | 42.84 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-4cc7a1cf30386caee04e2ac03bd71a0800ec1fb12445d01156f5fe5ea30f2c5f.replay) |
| trauma.map | bot-v10.4.1 | 105901 | B | L | roundLimit | 500 | 8 | 16 | 48.49 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-31ea0b969fd10f1c234b1a2061a9787e5ee97cf0c700c28117f2d30415ad2e57.replay) |
| trauma.map | bot-v10.4.1 | 105902 | A | W | roundLimit | 500 | 20 | 6 | 53.47 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-ac4926739c9cab848cf36c3ae34f5a059955e11612e8b44e0cacd104989fdf5d.replay) |
| trophy.map | bot-v10.4.1 | 105901 | A | W | teamEliminated | 140 | 3 | 0 | 23.75 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-f7958bd7efe78bb16e9f3daecb8f375ee024820d68fff3e320f024b8f9433cda.replay) |
| trophy.map | bot-v10.4.1 | 105902 | B | W | teamEliminated | 352 | 11 | 0 | 22.51 | [replay](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/replays/research-6a006d45c63fc4f89d8d3b4b0fddb2b763c4d67bf0b34ee89867ff9504174a3c.replay) |
