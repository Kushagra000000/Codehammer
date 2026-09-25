# v10 feature implementation report — 2026-09-26

Best new prototype: **bot-v10.1**, parent **bot-v10** (preserved r3 source),
ultimately based on v9. The formal champion remains **bot-v9**. No official
submission was made. The broad candidate search has been replaced by bounded
comparisons of v10, v10.1, v8 and v9.

## Result

The final source won 30/44 fresh-seed all-map games: 17–5 versus v8 and 13–9
versus v9. This is a positive observed result, not evidence of exponential or
universal superiority. Its earlier direct screen against stable v10 was 4–4.
The v5 five-map acceptance requirement was **not tested** because the user
restricted this work to the four-bot pool. Champion metadata is not promoted.

## Implemented

- Growth specialisation from round 250, inclusive; grower food priority.
- Directed sonar role/position/length and bounded territory messages; larger
  growers have precedence, with deterministic ID tie-breaking and expiry.
- Mapped clear rectangles, smaller-worker exclusion preferences and a safe
  serpentine patrol cycle with a reserved return column. Safety overrides patrol.
- Late self-collision feeding beside a visible, reachable grower. Friendly
  heads are excluded; stale reports and nearby enemy heads abort a donation.
- Small late attackers pursue and relay large-enemy head positions. Tactical
  sprint head trades are restricted to small dragons.
- Undersized growers yield to larger growers; split children can recover clear
  terrain through claims. A trapped grower can reverse most of its length into
  a split child when the tail escape is locally checked.
- Remembered own-body positions and self-collision fallback protect allies.

The bot uses reverse feeding rather than oversprinting: sprinting consumes
segments before death, reducing resource yield. Sonar is local ray communication,
not guaranteed global consensus. The patrol is a safe preference, not an
unconditional motion command on obstructed or contested terrain.

## Effective parameters

| Parameter | Final value |
|---|---:|
| Growth start | 250 |
| Stable hash grower fraction | 1/8, plus founders, large dragons and survival backups |
| Feeding start | 380 |
| Maximum feeder length | 4 |
| Minimum recipient length | 10 |
| Feeding radius | 3 |
| Initial territory capacity | max(80, 4 × grower length) |
| Minimum patrol grower length | 16 |
| Minimum cycle capacity | max(80, length + 20) |
| Claim expiry / position expiry | 12 / 2 rounds |
| Enemy report expiry | 2 rounds; forwarding preserves timestamp |
| Direct head-trade / sprint length cap | 3 / 4 |
| Sonar frequency | Every turn, up to four directions |

## Final validation by map

Seed 7203, both sides on every discovered map. Entries are W/D/L.

| Map | vs v8 | vs v9 |
|---|---|---|
| Colosseum | 2/0/0 | 2/0/0 |
| arena | 0/0/2 | 0/0/2 |
| big_empty | 2/0/0 | 2/0/0 |
| default | 2/0/0 | 1/0/1 |
| default_small | 0/0/2 | 1/0/1 |
| devil | 2/0/0 | 0/0/2 |
| queen_of_spades | 2/0/0 | 1/0/1 |
| schooltime | 2/0/0 | 1/0/1 |
| stronghold | 2/0/0 | 1/0/1 |
| trauma | 1/0/1 | 2/0/0 |
| trophy | 2/0/0 | 2/0/0 |

Final-validation win rate: 68.2%. v9 win rate: 59.1%; Wilson 95% lower bound 38.7%, below 50%, so statistical superiority over v9 remains unestablished.

Internal native-cohort Elo for the final source: 1083.8; this is separate from official team ratings. Mean final longest dragon 10.7, maximum 44. Early eliminations count as zero.

## Experiment history

| Experiment | Games | W/D/L | Disposition |
|---|---:|---|---|
| v10-r1-screen | 16 | 8/2/6 | Rejected: loses to v9 |
| v10.1-r1-screen | 24 | 13/5/6 | Advanced to broader tests |
| v10.1-r2-sandbox | 4 | 2/0/2 | Resource checks passed; open-map losses |
| v10.1-r3-allmaps | 44 | 25/0/19 | Rejected for promotion: 10–12 vs v9; preserved as v10 |
| v10.1-r4-concentration | 16 | 9/0/7 | 4–4 vs v10, 5–3 vs v9; advanced |
| v10.1-r4-validation | 44 | 30/0/14 | Best new prototype; no formal promotion |
| v10.1-r4-final-sandbox | 2 | 0/0/2 | Final-source resource validation |

Total: 150 ledger games (144 native, 6 sandbox), plus one separately recorded manual smoke. Final source: 60 native games and 2 sandbox games. All ledger results parsed; raw stdout/stderr and replays preserved.

## Resource and correctness checks

- Big Empty, side A: maximum 63,800,000 points, 0 CPU-limit events, 0 invalid actions.
- Big Empty, side B: maximum 63,700,000 points, 0 CPU-limit events, 0 invalid actions.

Toolkit: isolated unswbc 1.1.0, Python 3.13, Protocol 3. C++ safety fixtures
cover phase boundaries, priority, safe feeding, stale reports, blocked mouths,
enemy aborts, cycle continuity, packet expiry/team separation, split escapes,
friendly-head fallback, claim precedence/size, and grower sprint protection.
Python parser tests pass. Header-sensitive wasm cache invalidation is repaired
in both local runners without modifying the installed engine.

## Replay evidence and remaining weaknesses

The provided M237041 is Stronghold: A wins 151–102 in final longest length,
with A already at length 29 by round 100. Our requested round-250 specialisation
does not reproduce that early resource concentration. Source code and deliberate
intent cannot be recovered from the screenshot/replay.

In the initial v10 smoke, nine confirmed self-feeds dropped sixteen pearls:
nine went to named recipients, seven to other allies, none to enemies. This
proves the mechanism works but also shows imperfect recipient priority.
Detailed current-source replay accounting is saved separately when available.

Two final-source Stronghold replays confirm four self-feeds dropping eight
pearls: five collected by named growers, one by another ally, two uncollected
at the end, and none by enemies. One checked grower split reversal occurred.
Their geometry, portals, pearl respawn rules and initial dragons match the
supplied replay's map exactly (see `reference-map-comparison.json`).

Weaknesses remain map- and seed-dependent. Big Empty varied from two losses in
earlier tests to two v9 wins in final native validation, then two losses on
the fresh final sandbox seed. Several early-elimination
maps still regress. Stronghold improved in most comparisons, but one final
v9 loss was 17–18 at round 500. Neither that gain nor the all-map aggregate
establishes universal superiority. Multiple changes were bundled; individual
causal effects are unknown.

Promising next work: stronger protection of the longest surviving grower,
more reliable donation collection, contested-food reachability rather than
distance alone, and independent paired seeds before champion promotion. No
further search or official submission is running automatically.

## Reproducibility and paths

All tested candidate states are preserved in SHA-256-named source archives.
Matches record the existing Git HEAD plus source and map fingerprints; these
changes are not represented as a new Git commit. `current_sources.json` and
`packages.json` identify the final source trees and downloadable ZIPs.

- **bot-v10**: `bots/candidate/bot-v10`; ZIP `bots/candidate/bot-v10.zip`; source SHA-256 `528c355b5cc850be539b98f48ba1badbce1f7ce1fcdf4ce0faf55dbd6616d2e3`.
- **bot-v10.1**: `bots/candidate/bot-v10.1`; ZIP `bots/candidate/bot-v10.1.zip`; source SHA-256 `800e07868144c581a93707f7b8948c5ed5afa00abdcf22d81469ef0269b1303a`.
