# Paired experiment comparison

Baseline: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v105-20260927T131901Z/experiments/opening-parent/matches.jsonl`. Candidate: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v105-20260927T131901Z/experiments/opening-point300/matches.jsonl`.

Pairs match opponent source, map bytes, seed, side, engine and sandbox setting. Differences below are candidate minus baseline; lower elimination losses is better.

Errors: baseline 0, candidate 0. Unmatched valid games: baseline 0, candidate 0.

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| TOTAL | 21 | 8/2/11 | 14/2/5 | +28.57 pp | 7/1/13 | +5 | -5 | +1.81 |

Exact two-sided sign-test p = 0.0703125; 7 improved, 1 worsened; tied scores excluded.

Paired bootstrap 95% mean score-delta interval: [+4.76 pp, +52.38 pp]; 5000 samples, fixed seed 1052026.

Discordant wins: candidate wins / baseline nonwins 7; baseline wins / candidate nonwins 1. Strict W↔L flips: 7 for candidate, 1 for baseline.

**Exploratory evidence:** maps/opponents may be correlated; these statistics do not correct for repeated candidate selection or certify superiority on unseen opponents. Final validation needs fresh seeds/maps.

## Opponents

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| bot-v10.1 | 7 | 3/1/3 | 5/1/1 | +28.57 pp | 2/0/5 | +2 | -1 | -2.43 |
| bot-v10.3.1 | 7 | 3/0/4 | 6/0/1 | +42.86 pp | 4/1/2 | +3 | -3 | +5.86 |
| bot-v11.1 | 7 | 2/1/4 | 3/1/3 | +14.29 pp | 1/0/6 | +0 | -1 | +2.00 |

## Maps

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| Colosseum.map | 3 | 2/0/1 | 3/0/0 | +33.33 pp | 1/0/2 | +1 | -1 | +0.67 |
| arena.map | 3 | 1/2/0 | 0/2/1 | -33.33 pp | 0/1/2 | -1 | +1 | -1.33 |
| default.map | 3 | 2/0/1 | 3/0/0 | +33.33 pp | 1/0/2 | +0 | -1 | +14.00 |
| default_small.map | 3 | 0/0/3 | 2/0/1 | +66.67 pp | 2/0/1 | +2 | -2 | +2.33 |
| devil.map | 3 | 2/0/1 | 2/0/1 | +0.00 pp | 0/0/3 | +0 | +0 | +0.33 |
| queen_of_spades.map | 3 | 1/0/2 | 1/0/2 | +0.00 pp | 0/0/3 | +0 | +0 | +0.00 |
| trophy.map | 3 | 0/0/3 | 3/0/0 | +100.00 pp | 3/0/0 | +3 | -2 | -3.33 |
