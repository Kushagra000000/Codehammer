# v11 research report — 2026-09-26

**Decision: no champion promotion. Keep the established v10.1/v10.2 options for competition.**
The final new candidate is bot-v11.1 (R8). It implements the requested navigation features, but did not beat every requested baseline in fresh validation. No official submission was made.

## Exact versions and lineage

- v11.1: `bots/candidate/bot-v11.1`; SHA-256 `860e7ea84e5571907632bd51de196e0935576d7fc930f3aef2b66dcd4413ac30`.
- v11: `bots/candidate/bot-v11`; SHA-256 `760fed7cf9026f9fcbd89f9e3e400c9b07f03b7d9f662f26ee1edbc0160582ba`.
- Parent: frozen v10.2; v11.1 restores v10.1 food/exploration coefficients. R8 adds the small opening-trade correction. The direct v10.1 rebase (R7) was rejected and archived.
- Every tested revision has a hash-named source tarball. The two ZIP files contain the exact final working sources. Existing bots, user scripts, replays and old experiment records were preserved.

## Implementation

- Center attraction stops by round 25, or earlier after observing a barren/occupied center. Resource-aware deterministic sector targets spread workers.
- Body-aware portal food routes, visible body reconstruction for new split children, and exact intermediate sprint history.
- Rectangular Hamiltonian patrols require whole-body cyclic ordering. Expansion is accepted only when ordering survives. The currently occupied tail is forbidden.
- Four-move virtual-body continuation and bounded two-to-five-step emergency sprints (one-to-four segments spent), using visible sprint destinations. Other dragons and unseen cells prevent a global safety guarantee.
- Protocol 3 terrain/pearl/portal messages preserve sample time; role and territory messages remain. New 5/6/7 tags avoid legacy packet collisions. Aggregate echoes do not certify a specific portal.
- Existing growth, feeding, large-grower protection and reversal escape subsystems remain. R8 permits length<=3 direct trades with at least 2 living allies.

## Active parameters

|Parameter|v11|v11.1|
|---|---:|---:|
|Center cutoff / weight|25 /45|25 /45|
|Growth specialisation|250|250|
|Worker exploration weight / cap|10 /800|6 /360|
|Immediate food score worker / grower|2500 /3800|850 /2800|
|Sector gradient weight|75|75|
|Lookahead / maximum sprint steps|4 /5|4 /5|
|Patrol shortcuts|disabled|disabled|
|Feeding start / max feeder length / radius|380 /4 /3|380 /4 /3|
|Direct head-trade maximum length|3|3|
|Direct trade opening restriction|round>=50 and allies>=6|allies>=2|
|Sonar|4 directed rays/turn;1 terrain ray|same|

## Evidence

- Entire session: **528 parsed games**, 514 native and 14 sandbox; zero unparsed outcomes. Maps discovered automatically: 11.
- Exact final v11.1: 110 fresh validation games, 22 mutual games, 8 diagnostic games and 4 sandbox games. Diagnostic maps/seeds are not silently pooled into all-map validation.
- Final validation W / D / L: **62 / 16 / 32**. Mutual vs final v11: **8 / 4 / 10**.
- Final-source internal Elo: **1070.7** (starts1000, local/order-dependent; unrelated to official ratings).
- Mean final longest: **9.73**; maximum final longest: **55**. Elimination counts as zero.
- Sandbox maximum observed turn points: **51,800,000**; CPU-limit events: 0; invalid-action deaths: 0.
- Native fixtures: tail-before-movement collision, 500 ordered cycle steps, split-child body reconstruction, sprint intermediate history, multi-step escape paths, terrain/portal packet round trips, expiry, legacy packet rejection and small-trade protection.

|Opponent|Wins / draws / losses|Score rate|
|---|---:|---:|
|bot-v10.2|8 / 4 / 10|45.5%|
|bot-v10.1|13 / 3 / 6|65.9%|
|bot-v10|15 / 3 / 4|75.0%|
|bot-v9|11 / 3 / 8|56.8%|
|bot-v8|15 / 3 / 4|75.0%|

These are one fresh seed per map/side for the exact final source. The earlier R6 source had another 110-game cohort but is a different code identity; those results are not pooled to inflate R8. Small margins are uncertain. **v5 was excluded by the requested opponent pool; the historical five-map-v5 gate is untested and not satisfied by these results.**

## Maps

Strongest observed: queen_of_spades, Colosseum, default. Weakest observed: stronghold, default_small, trauma.

|Map|Wins / draws / losses|
|---|---:|
|Colosseum|8 / 0 / 2|
|arena|0 / 10 / 0|
|big_empty|5 / 0 / 5|
|default|8 / 0 / 2|
|default_small|4 / 1 / 5|
|devil|6 / 0 / 4|
|queen_of_spades|10 / 0 / 0|
|schooltime|8 / 0 / 2|
|stronghold|5 / 0 / 5|
|trauma|4 / 0 / 6|
|trophy|4 / 5 / 1|

## Rejected experiments and limitations

- R1 navigation: mixed/negative results and peaks near 99.8M instructions. Caches improved observed resource margin in later runs.
- Deeper eight-step planning and cycle shortcuts: did not beat the strict four-step screen.
- Requiring productive patrol rectangles: 4–4 vs v11 and 3–5 vs v10.2 on sparse maps; no gain.
- Early growers: reached 73 on Big Empty but finished5–11 in screening; rejected for cross-map regressions.
- Strong center opening:7–9; rejected. Direct v10.1 rebase did not meet its selection score.
- R8 opening trades: 6–1–1 on its diagnostic, but that improvement was not a guarantee on fresh seeds. The full fixed schedule was completed without selective extensions.
- Large snakes can still die. Partial vision, unknown portal exits, enemy moves, blocked feeding routes and incomplete new-child body knowledge remain limits. A successful Hamiltonian fixture is not proof of invulnerability in Battlecode.
- King length is not falsely inferred: several growers can coexist, so per-match king_length remains null. Feeding counters are logged attempts; confirmed transfer analysis requires replay-specific inspection.

## Pipeline and next work

`hypothesis → source snapshot → native fixtures → smoke → both-side screen → fresh all-map validation → sandbox → documented decision`

- The evaluator discovers new maps and bots, hashes source and maps, preserves raw outputs/replays, checks CLI/replay agreement, and never submits or promotes automatically.
- Strongest remaining ideas: test the untried interaction of v11’s higher food weights with v11.1’s small opening trades; time-expanded tail-reachability beyond local lookahead; resource-aware cycle acquisition without sacrificing early swarm; corridor reservations with turn-order-aware ally prediction; topology-limited early growers. Each needs an isolated, preregistered test.
- Before any formal promotion, run repeated balanced v5 comparisons with a threshold specified before results and satisfy at least five maps, plus broader robustness.
- Stopping condition: the fixed final validation is complete after multiple strategy families failed to produce a robust all-opponent winner. Keep these candidates as research artifacts; do not label them ultimate or replace the established bots automatically.

Research references: [John Tapsell’s cycle-ordering algorithm](https://johnflux.com/2015/05/02/nokia-6110-part-3-algorithms/) and [chynl’s actual Snake AI](https://github.com/chynl/snake).

Detailed ledger: `matches.jsonl`; cohorts: `summary.json`; dashboard: `../../reports/v11.html`; resource records: `resource_diagnostics.json`.
