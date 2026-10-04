# Paired experiment comparison

Baseline: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v105-20260927T131901Z/experiments/validation-parent/matches.jsonl`. Candidate: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v105-20260927T131901Z/experiments/validation-candidate/matches.jsonl`.

Pairs match opponent source, map bytes, seed, side, engine and sandbox setting. Differences below are candidate minus baseline; lower elimination losses is better.

Errors: baseline 0, candidate 0. Unmatched valid games: baseline 0, candidate 0.

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| TOTAL | 110 | 73/6/31 | 74/6/30 | +0.91 pp | 19/18/73 | +3 | -4 | +0.34 |

Exact two-sided sign-test p = 1; 19 improved, 18 worsened; tied scores excluded.

Paired bootstrap 95% mean score-delta interval: [-10.00 pp, +11.82 pp]; 20000 samples, fixed seed 1052026.

Discordant wins: candidate wins / baseline nonwins 19; baseline wins / candidate nonwins 18. Strict W↔L flips: 19 for candidate, 18 for baseline.

**Exploratory evidence:** maps/opponents may be correlated; these statistics do not correct for repeated candidate selection or certify superiority on unseen opponents. Final validation needs fresh seeds/maps.

## Opponents

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| bot-v10 | 22 | 15/2/5 | 15/2/5 | +0.00 pp | 4/4/14 | +1 | +0 | -1.50 |
| bot-v10.1 | 22 | 16/1/5 | 17/1/4 | +4.55 pp | 4/3/15 | +0 | -2 | +2.68 |
| bot-v10.3.1 | 22 | 11/0/11 | 12/0/10 | +4.55 pp | 6/5/11 | +1 | +0 | +3.64 |
| bot-v11.1 | 22 | 14/1/7 | 15/1/6 | +4.55 pp | 3/2/17 | +1 | -3 | -0.82 |
| bot-v9 | 22 | 17/2/3 | 15/2/5 | -9.09 pp | 2/4/16 | +0 | +1 | -2.32 |

## Maps

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| Colosseum.map | 10 | 7/0/3 | 9/0/1 | +20.00 pp | 2/0/8 | +2 | -2 | +1.10 |
| arena.map | 10 | 2/6/2 | 2/6/2 | +0.00 pp | 0/0/10 | +0 | +0 | -0.10 |
| big_empty.map | 10 | 8/0/2 | 9/0/1 | +10.00 pp | 1/0/9 | +0 | +0 | -1.20 |
| default.map | 10 | 7/0/3 | 6/0/4 | -10.00 pp | 2/3/5 | -1 | +1 | +0.20 |
| default_small.map | 10 | 5/0/5 | 7/0/3 | +20.00 pp | 3/1/6 | +2 | -2 | +0.40 |
| devil.map | 10 | 5/0/5 | 9/0/1 | +40.00 pp | 5/1/4 | +4 | -4 | +1.80 |
| queen_of_spades.map | 10 | 6/0/4 | 3/0/7 | -30.00 pp | 0/3/7 | -3 | +3 | -1.20 |
| schooltime.map | 10 | 9/0/1 | 7/0/3 | -20.00 pp | 1/3/6 | -1 | +0 | -4.50 |
| stronghold.map | 10 | 7/0/3 | 8/0/2 | +10.00 pp | 3/2/5 | +0 | +0 | +7.20 |
| trauma.map | 10 | 9/0/1 | 6/0/4 | -30.00 pp | 0/3/7 | +0 | +0 | -0.50 |
| trophy.map | 10 | 8/0/2 | 8/0/2 | +0.00 pp | 2/2/6 | +0 | +0 | +0.50 |
