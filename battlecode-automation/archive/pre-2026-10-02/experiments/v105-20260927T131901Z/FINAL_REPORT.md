# bot-v10.5 — scoped experiment report

**Created and tested, with better observed elimination results but no established overall superiority over v10.4.1.** The search stopped after the permitted strategy areas reached a local plateau. No bot was submitted and no champion registry was changed.

478 official-toolkit local games across 16 experiments completed with zero parser/CLI errors and zero candidate runtime failures. Replays, raw output, source and map snapshots remain preserved. Failed variants were retained.

## Selected bot and exact changes

- Path: `bots/current/bot-v10.5`; parent: `bots/current/bot-v10.4.1`.
- Only `main.cpp` differs in executable strategy code. `coordination.hpp`, `helper.hpp` and `bot.toml` are byte-identical to the parent.
- Extra center attraction operates during rounds 0–39, toward a radius-3 central area. It yields to food within 4 objective-cost steps, stops when 3 visible friendly heads occupy the central area, and disables on sufficiently sampled barren central terrain. The original weak center score remains afterward.
- Other growers may respond to a larger grower's feeding request from round 450 rather than 475. All existing freshness, recipient-size, visibility and safe-sacrifice checks remain.
- Hash-selected growers remain 1-in-8. Reducing them to 1-in-16 or 1-in-32 did not increase screening wins and lowered mean final longest length. Founders, heirs and the post-400 top-two rule are unchanged.
- Splitting schedules, combat, pathfinding, memory, sonar format, patrol cycles, pearl detours and escape splitting remain inherited. In particular the actual parent specialization round is 300; it was preserved.

| Parameter | Value |
|---|---:|
| OPENING_CENTER_END_ROUND | 40 |
| OPENING_CENTER_WEIGHT | 300 |
| OPENING_CENTER_RADIUS | 3 |
| OPENING_FOOD_PRIORITY_RADIUS | 4 |
| OPENING_CENTER_CROWD_LIMIT | 3 |
| ALPHA_FEED_START_ROUND | 450 |
| GROWER_NUMERATOR / GROWER_DENOMINATOR | 1 / 8 |

## Fresh validation

Both sources played the same 110-game schedule: all 11 maps, v9/v10/v10.1/v10.3.1/v11.1, seeds 105901 and 105902. Each opponent/map pair has one game on each side across the two seeds. Parameters were frozen before the results were inspected.

| Measure | v10.4.1 | v10.5 |
|---|---:|---:|
| W/D/L | 73/6/31 | 74/6/30 |
| Win rate | 66.4% | 67.3% |
| Score rate (draw = half) | 69.1% | 70.0% |
| Elimination W/L | 41/22 | 44/18 |
| Round-limit W/L | 32/9 | 30/12 |
| Mean final longest, all games | 12.55 | 12.89 |
| Maximum final longest | 68 | 56 |

The gain is only one additional win (+0.91 percentage points). There were 19 improved paired outcomes, 18 worsened and 73 tied; exact paired sign-test p=1.0. A fixed-seed, 20,000-resample map-cluster bootstrap gives a 95% score-difference interval of −10.91 to +13.64 percentage points. This does not establish overall superiority. The separate direct-parent test finished 11 wins and 11 losses, with elimination wins/losses 8/6.

### By opponent

| Opponent | Parent W/D/L | v10.5 W/D/L |
|---|---|---|
| bot-v10 | 15/2/5 | 15/2/5 |
| bot-v10.1 | 16/1/5 | 17/1/4 |
| bot-v10.3.1 | 11/0/11 | 12/0/10 |
| bot-v11.1 | 14/1/7 | 15/1/6 |
| bot-v9 | 17/2/3 | 15/2/5 |

### By map

| Map | Parent W/D/L | v10.5 W/D/L |
|---|---|---|
| Colosseum.map | 7/0/3 | 9/0/1 |
| arena.map | 2/6/2 | 2/6/2 |
| big_empty.map | 8/0/2 | 9/0/1 |
| default.map | 7/0/3 | 6/0/4 |
| default_small.map | 5/0/5 | 7/0/3 |
| devil.map | 5/0/5 | 9/0/1 |
| queen_of_spades.map | 6/0/4 | 3/0/7 |
| schooltime.map | 9/0/1 | 7/0/3 |
| stronghold.map | 7/0/3 | 8/0/2 |
| trauma.map | 9/0/1 | 6/0/4 |
| trophy.map | 8/0/2 | 8/0/2 |

Strongest candidate maps were Colosseum, BigEmpty and Devil (9/10 wins each). Queen was weakest (3/10 wins); Arena produced six mutual-elimination draws. Regressions remain on Queen, Trauma, Schooltime and Default, and against v9 overall. These offset much of the improvement on Devil, DefaultSmall and Colosseum.

## What the staged tests showed

| Experiment | W/D/L | Interpretation |
|---|---|---|
| Original opening reference | 8/2/11 | Same 21-game screen |
| Center point 300, cutoff 40 | 14/2/5 | Better screen; inferior to central area |
| Center area radius 3, 300, cutoff 40 | 16/2/3 | Selected opening |
| Center point 200, cutoff 40 | 12/3/6 | Rejected |
| Center area 300, cutoff 80 | 12/2/7 | Rejected: longer commitment hurt |
| Alpha start 475 | 9/0/7 | 16-game late-map control |
| Alpha start 450 | 11/0/5 | Kept; small and uncertain isolated gain |
| Grower denominator 16 | 11/0/5 | No win gain; mean longest 28.31 vs 29.75 |
| Grower denominator 32 | 11/0/5 | No win gain; mean longest 27.88 vs 29.75 |
| Final opening-control replication, alpha 450 | 16/2/3 | Additional screen after weak validation gain |
| Shorter opening, cutoff 15 | 13/2/6 | Rejected |
| Shorter opening, cutoff 25 | 12/2/7 | Rejected |

The extra opening sweep was explicitly added after the first fresh result showed only a marginal aggregate gain. Both alternatives lost to the unchanged control on the development screen; the validated source stayed unchanged. No threshold or historical result was rewritten to make a candidate pass.

## Replay evidence and tradeoffs

OBSERVED: in a paired DefaultSmall opening, center entry moved from round 46 to 2, the first split from 12 to 9, and pearls collected before round 40 from 10 to 14. The loss became a win, but unit counts were not uniformly higher at every checkpoint. This is an example, not a general causal guarantee.

OBSERVED: all 16 alpha-timing replay pairs have identical action/state prefixes before round 450. Donations during rounds 450–474 rose from 59 to 79; donors of length at least 10 rose from 15 to 26. Recipient families collected 137 versus 166 donated pearls. Mean final longest changed only 29.50→29.75.

One Schooltime recipient reached 33 and won, versus 25 and a loss in the later-feeding control. However, one Stronghold second-longest donor sacrificed at length 13 to a length-14 recipient; only two of seven pearls reached the recipient. The control donor survived to length 35 and won. Early donation can destroy a productive backup. See [the detailed provenance](late-feeding-evidence.md).

UNKNOWN: external-team performance, generalization to unseen maps/seeds, and whether reducing the post-400 top-two grower count would help. Only the hash-selected fraction before round 400 was reduced in this task. Further improvements should focus on protecting productive donors and on center policy regressions; they were not implemented outside the requested scope.

## Verification and reproducibility

- Isolated Python environment: `.battlecode-codex`; engine/toolkit `unswbc 1.1.0`, Protocol 3 helper inherited unchanged.
- Native build and focused opening/feeding fixtures passed; inherited productive-patrol, pearl-detour and trapped-worker/grower escape fixtures passed.
- Four judge-like sandbox matches: 3 wins/1 loss, zero runtime failures, maximum reported candidate turn cost 14.3M points. Candidate final lengths were 48, 58, 18 and 36.
- No v5 acceptance test or formal champion promotion was performed in this narrow requested opponent pool. No internal Elo was inferred from the screening tables; official team ratings were not touched.
- Source snapshots and raw ledgers identify every experimental revision. Current parent still exactly matches its preserved snapshot. Replays remain under ignored `replays/` and were not committed.

Selected source SHA-256: `5b3a1910a23e76454fb228eedbaf16f757b96f7cd7bbfd8279563941e6f0da32`.

Full manifests and raw output: `experiments/<experiment-id>/`. Statistical review: [REVIEW.md](REVIEW.md). Dashboard: [dashboard.html](dashboard.html). Exact executable diff: [final-main.diff](final-main.diff).

For a new manual all-map screen from the repository root:

```bash
.battlecode-codex/bin/python battlecode-automation/scripts/compare_bot.py --bot bot-v10.5
```

Conclusion: keep v10.5 as the best scoped candidate found in these trials, while treating its overall strength as comparable to v10.4.1 until stronger evidence emerges. It achieved the requested direction in observed elimination results, not a demonstrated substantial overall win-rate increase.
