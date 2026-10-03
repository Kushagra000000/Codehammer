# Paired experiment comparison

Baseline: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v105-20260927T131901Z/experiments/late-alpha475/matches.jsonl`. Candidate: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/battlecode-automation/experiments/v105-20260927T131901Z/experiments/late-alpha450/matches.jsonl`.

Pairs match opponent source, map bytes, seed, side, engine and sandbox setting. Differences below are candidate minus baseline; lower elimination losses is better.

Errors: baseline 0, candidate 0. Unmatched valid games: baseline 0, candidate 0.

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| TOTAL | 16 | 9/0/7 | 11/0/5 | +12.50 pp | 3/1/12 | +0 | +0 | +0.25 |

Exact two-sided sign-test p = 0.625; 3 improved, 1 worsened; tied scores excluded.

Paired bootstrap 95% mean score-delta interval: [-12.50 pp, +37.50 pp]; 5000 samples, fixed seed 1052026.

Discordant wins: candidate wins / baseline nonwins 3; baseline wins / candidate nonwins 1. Strict W↔L flips: 3 for candidate, 1 for baseline.

**Exploratory evidence:** maps/opponents may be correlated; these statistics do not correct for repeated candidate selection or certify superiority on unseen opponents. Final validation needs fresh seeds/maps.

## Opponents

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| bot-v10.3.1 | 8 | 5/0/3 | 5/0/3 | +0.00 pp | 1/1/6 | +0 | +0 | -0.50 |
| bot-v11.1 | 8 | 4/0/4 | 6/0/2 | +25.00 pp | 2/0/6 | +0 | +0 | +1.00 |

## Maps

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| big_empty.map | 4 | 2/0/2 | 3/0/1 | +25.00 pp | 1/0/3 | +0 | +0 | +0.25 |
| schooltime.map | 4 | 2/0/2 | 3/0/1 | +25.00 pp | 1/0/3 | +0 | +0 | +1.25 |
| stronghold.map | 4 | 2/0/2 | 2/0/2 | +0.00 pp | 1/1/2 | +0 | +0 | -0.25 |
| trauma.map | 4 | 3/0/1 | 3/0/1 | +0.00 pp | 0/0/4 | +0 | +0 | -0.25 |
