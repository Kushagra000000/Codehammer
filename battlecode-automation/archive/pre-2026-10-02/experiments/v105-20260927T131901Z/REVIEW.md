# Independent review: bot-v10.5

## Code scope — PASS

Executable-source comparison against v10.4.1 changes only `main.cpp`: five opening-center constants, one guarded opening-center scoring helper, one call in existing move scoring, and alpha-feeding start 475 → 450. `coordination.hpp`, `helper.hpp`, and `bot.toml` are byte-identical. `NOTES.md` and `SOURCE_STATE.json` changes document the new identity. No patrol, navigation, splitting, collision, combat, or sonar implementation was changed.

Opening attraction adds coefficient 255 outside radius 3 to the inherited coefficient 45, yielding an outside-region total of 300. It stops at round 40 and is disabled by nearby pearl-objective cost ≤4, three visible allied center heads, or enough remembered evidence that central tiles are mostly infertile. This uses ordinary observations/memory, not hidden map access. Retained growers are excluded. Existing legal-move and collision gates remain in place; this is a scoring preference, not guaranteed ownership of the center.

Only the existing alpha-phase boundary moves to 450. Existing fresh request, strictly larger recipient, visible live head, reachable delivery tile, and enemy-proximity checks remain untouched. This does not guarantee a global alpha: knowledge remains constrained by sonar/vision. Grower selection remains 1-in-8; reductions were screened and excluded.

## Reproducibility and validation schedule — PASS

Current v10.5, selected-source, and validation candidate snapshot share fingerprint `5b3a1910a23e76454fb228eedbaf16f757b96f7cd7bbfd8279563941e6f0da32`. Parent snapshot is `04d66cc757c7796741312980e878e7483280d0b83eeb246b7a58efb13dfc5cbe`.

All candidate/opponent source and map snapshot bytes were rehashed against both validation manifests successfully. Each manifest contains 110 unique scheduled games: five identical frozen opponents, all 11 maps, seeds 105901 and 105902. Opponent hash, map hash, seed, side and sandbox identities match exactly between parent and candidate schedules. The seed-index side alternation gives each opponent/map one game on each side across the two seeds; it is not a same-seed reversed replication. Both use unswbc 1.1.0 in native mode. Validation seeds were absent from all nine development experiments.

Actual research tasks use the immutable source/map snapshots. `research.run` adds the Git commit to each normal result and compares the installed toolkit's CLI outcome with its replay outcome. Elimination is classified from `teamEliminated`, not guessed from the round number. Parse/setup failures remain errors and are excluded from win rate. No external current-team bots are evaluated; this supports claims about the declared internal opponent pool only.

## Statistical review — COMPLETE; improvement not established

All 110 scheduled games finished in each pool cohort with exact pairing, no unmatched games, no parser errors and no bot runtime failures. Actual result rows reference the frozen source snapshots, have their expected candidate hashes and retain Git commit metadata. Current and selected v10.5 hashes still match validation.

| Metric | v10.4.1 parent | v10.5 candidate | Change |
|---|---:|---:|---:|
| W/D/L | 73/6/31 | 74/6/30 | One additional win |
| Win rate | 66.36% | 67.27% | +0.91 percentage points |
| Elimination W/L | 41/22 | 44/18 | +3 wins, −4 losses |
| Mean final longest | 12.555 | 12.891 | +0.336 |

Paired scores improved in 19 games, worsened in 18, and tied in 73. All discordances were W↔L flips. The exact two-sided sign test gives **p = 1.0**. The 20,000-sample paired-game bootstrap gives a 95% mean-score-difference interval of **[−10.00, +11.82] percentage points**. These results do not establish a reliable overall improvement.

The additional **map-cluster bootstrap** resampled 11 complete map blocks, retaining all ten paired games per selected map (fixed seed 1052026, 20,000 samples). Its 95% score-difference interval is **[-10.91, +13.64] percentage points**, also including zero. This allows within-map dependence; shared opponents and only 11 maps still limit precision.

## Per-opponent and map audit

| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |
|---|---:|---|---|---:|---|---:|---:|---:|
| bot-v10 | 22 | 15/2/5 | 15/2/5 | +0.00 pp | 4/4/14 | +1 | +0 | -1.50 |
| bot-v10.1 | 22 | 16/1/5 | 17/1/4 | +4.55 pp | 4/3/15 | +0 | -2 | +2.68 |
| bot-v10.3.1 | 22 | 11/0/11 | 12/0/10 | +4.55 pp | 6/5/11 | +1 | +0 | +3.64 |
| bot-v11.1 | 22 | 14/1/7 | 15/1/6 | +4.55 pp | 3/2/17 | +1 | -3 | -0.82 |
| bot-v9 | 22 | 17/2/3 | 15/2/5 | -9.09 pp | 2/4/16 | +0 | +1 | -2.32 |

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

## Additional checks and interpretation

The separate 22-game direct parent comparison finished **11 wins / 11 losses**, which supplies no head-to-head advantage. Four judge-like sandbox games finished **3 wins / 1 loss**, with no errors; these check viability and are too few to establish strength.

The defensible positive finding is lower elimination loss count on this fresh internal pool (18 versus 22) and more elimination wins (44 versus 41). The aggregate win improvement is only one game, while map and opponent gains/regressions offset one another. Opening-center changes had much larger development-screen gains than fresh-validation gains, consistent with screening variability and candidate selection effects. Alpha450 was screened separately, but the final cohort measures the combined selected strategy, not independent causal contributions. Reduced grower counts were rejected in screening; the final retains 1-in-8.

**Conclusion:** v10.5 implements the requested narrow strategy changes and passes reproducibility/runtime checks. It is a plausible alternative with improved observed elimination counts, but these results do **not** prove statistically significant superiority over v10.4.1, every older version, or official external opponents. Neither source should be called an established new champion from this evidence.

Statistical artifacts: `validation-comparison/comparison.json`, `validation-comparison/map-cluster-bootstrap.json`, and `validation-comparison/REPORT.md`.
