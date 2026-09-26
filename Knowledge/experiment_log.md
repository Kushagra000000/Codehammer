# Experiment Log

## 2026-09-26 research batch

Current user acceptance requirement is **five** statistically positive v5 maps;
the older six-map text below is retained as history. The preregistered protocol
is `battlecode-automation/experiments/research-20260926/PLAN.md`.

### EXP001 — parameterised v8 equivalent

TESTED: parent v8; candidate `bots/candidate/bot-v9-exp001`. Toolkit 1.1.0.
Centralised existing strategy coefficients; king/feeding/portal probability
remain disabled reserved controls. No claim that reserved controls implement
those future subsystems. Frozen source hashes and starting commit are recorded
in `initial_manifest.json`; exact candidate states remain separate directories.

Manual smoke: v8 vs v5, Arena, seed 20260926, v8 as A, won round 39.
Replay decoded using the parser shipped with the installed toolkit; final
result agreed with CLI and was saved in `smoke.result.json`.

SUPPORTED equivalence on Arena and Default Small, both sides, seed 20260926:
all four candidate replays have identical event streams to paired v8 references.
Each version won all four games against v5. This establishes refactor equivalence
on these cases, not strategic superiority or promotion eligibility.

### EXP002 / EXP003 — preregistered king and feeding screens

HYPOTHESIS EXP002: selecting the largest locally observed/fresh-sonar dragon as
a grower after round 300 preserves a useful leader. Ties prefer low IDs; missing
leaders fall back to the largest currently known survivor. Partial visibility
can produce multiple local kings; this is not claimed as global consensus.

HYPOTHESIS EXP003: EXP002 plus controlled self-collision after round 450 for
length <=3 feeders within distance 3 of a visibly larger leader, with no visible
enemy within distance 6, improves final longest length. Feeding stops after 490.
All head destinations are rejected. Intent is logged as FEED for replay metrics.
Results pending; neither candidate is accepted.

### EXP004–EXP013 — single-family screening plan

All derive from EXP001, except EXP003 which derives from EXP002. HYPOTHESES,
not established improvements. Exact code snapshots are separate candidate dirs.

* EXP004: frontier reward = 12 × sum of novelty over the destination's future
  7×7 vision; unseen contributes 8, stale age/12 capped at 8. Non-growers only.
* EXP005–009: explicit 5/10/15/20/25 percent deterministic Bernoulli choice of
  the best known, safe portal among candidate moves for non-growers length <=3.
  This is an additional exploration override, not a total portal-use rate;
  normal scoring can still select portals. Unknown exits are not eligible.
* EXP010: hash-selected growers retain length once population reaches 48, or
  after the usual round 300 threshold. Other defaults fixed.
* EXP011: tactical sprint head trades limited to length <=3.
* EXP012: pearl objective uses remembered topology BFS, including known portals
  and rejecting kelp/unpaired portals, while ordinary unknown edges stay open.
* EXP013: an enemy/ enemy-head echo activates the existing safe selection tier.
  Aggregate echoes do not provide a fabricated directional enemy location.

Screen eight development maps for EXP002–004; exploratory portal/adaptive/
combat/navigation/echo screens use Colosseum, Default, Devil and Trophy, both
sides, seed 101, against v5 and v8. Advance only promising components; independent
seed replication and holdouts are required before promotion. No exact config is
repeated without an explicitly named replication experiment.

## Baselines

### BASELINE-CURRENT — Uploaded Current Bot

Status: Baseline observation only. No new experimental result should be inferred from this section.

Current architecture:

* Maintains per-dragon map memory.
* Records visited tiles, pearl observations and edges.
* Learns portal endpoints from visible edges.
* Builds enemy and ally threat maps from visible heads.
* Uses flood-fill space estimation.
* Uses pearl-distance scoring.
* Uses teammate spacing.
* Uses deterministic movement jitter.
* Uses tactical head-trade sprints.
* Uses four-directional sonar status messages.

Current behavioural phases:

* Early game: splitting until approximately round 300.
* Middle game: approximately one quarter of IDs are selected as growers.
* Late game: all surviving dragons retain length after approximately round 425.

Important limitations to investigate:

1. No explicit king/central-growth policy (as observed in one of the top ranking teams)
2. No explicit late-game feeder/sacrifice strategy (observed in one of the top teams)
3. No true configurable portal probability (looks like a variable we can play around with)
4. Exploration is local rather than explicitly sector-based.
5. Split children start with fresh memory.
6. Pearl route distance does not model kelp/portal topology.
7. Some combat decisions treat large and small dragons similarly.
8. Sonar communication is not yet a full team-control protocol.
9. Current phase transitions are mostly fixed to round numbers.
10. Some tactical behaviours may overfit the replay that originally inspired the bot.

---

# Experiment Format

Each experiment must contain:

## Experiment ID

Unique identifier such as `EXP-001`.

## Parent

Bot version used as the starting point.

## Candidate

Temporary candidate or resulting bot version.

## Hypothesis

What specific idea is being tested?

## Variables

Every changed parameter and its exact value.

## Unchanged Variables

Important controls that remain fixed.

## Maps

Maps tested (try testing on all the maps using the inbuild unswbc run or script I created (recheck script before running))

## Opponents

Versions / bots used as opponents (try using top elo teams as opponents)

## Games

Number of games and side assignments.

## Results

Wins, losses and draws.

## Per-map Results

A candidate should never be judged from aggregate results alone.

## Strategic Metrics

Record where possible:

* longest surviving dragon,
* total team length,
* final unit count,
* elimination round,
* exploration coverage,
* portal entries,
* portal deaths,
* sacrificial deaths,
* pearls collected by king,
* king survival.

## Conclusion

One of:

* Reject
* Weak improvement
* Promising
* Candidate champion
* Requires more testing

Never describe an experiment as successful if the result is unparsed or incomplete.

---

# Initial Experiment Queue

## EXP-001 — Parameterise Existing Behaviour

Goal:

Move important strategy values to a configuration section at the top of `main.cpp`.

Parameters should include:

* split cutoff,
* final growth cutoff,
* grower fraction,
* portal probability,
* portal bonus,
* exploration weight,
* visit penalty,
* space penalty,
* threat penalty,
* king mode,
* sacrifice start round.

No strategic behaviour should change in this experiment.

Purpose:

Create a clean baseline for automated optimisation.

---

## EXP-002 — Portal Probability

Hypothesis:

Increasing intentional portal exploration improves map coverage and pearl access.

Test:

* 5%
* 10%
* 25%
* 50%
* 75%

Keep the rest of the strategy identical.

Primary metrics:

* win rate,
* portal entries,
* portal-related deaths,
* map coverage.

---

## EXP-003 — Exploration Strength

Hypothesis:

The current swarm converges too strongly toward the same pearl regions.

Test different exploration weights and visit penalties.

Compare:

* current behaviour,
* low exploration,
* medium exploration,
* high exploration,
* deterministic sector exploration.

Primary metric:

Map-by-map performance plus exploration coverage.

---

## EXP-004 — Sector-Based Distribution

Hypothesis:

Assigning dragons deterministic map sectors produces better resource coverage than purely local exploration.

Test:

* no sectors,
* 4 sectors,
* 8 sectors,
* 16 sectors,
* hash-based dynamic sectors.

Measure:

* tile coverage,
* team spread,
* pearl collection,
* survival,
* win rate.

---

## EXP-005 — King Identification

Hypothesis:

A dedicated king produces a longer final dragon.

Test:

* no king,
* ID-based king,
* root dragon king,
* largest-dragon king,
* root-then-largest backup.
* Snake Id 1 as king (given priority since beginning of game and has low risk moves)

Do not assume `ID 1` is universally correct.

---

## EXP-006 — Late-Game Feeding

Hypothesis:

Small dragons should deliberately die near the king in the final phase.

Test:

* no sacrifice,
* sacrifice after round 400
* sacrifice after round 430,
* sacrifice after round 450,
* sacrifice after round 465.

Measure:

* final king length,
* team win rate,
* final total length,
* king survival rate.

---

## EXP-007 — Sacrifice Method

Compare:

1. self-collision,
2. collision with king body,
3. sprint death,
4. hybrid method.

Do not intentionally collide with the king's head.

---

## EXP-008 — Feeder Size

Test:

* maximum feeder length 2,
* maximum feeder length 3,
* maximum feeder length 4,
* no restriction.

Measure final king length versus total sacrificed pearl yield.

---

## EXP-009 — Adaptive Phase Timing

Replace fixed phase boundaries with state-dependent decisions.

Candidate inputs:

* round,
* rounds remaining,
* unit count,
* king length,
* enemy longest known length,
* number of small dragons,
* map size.

---

## EXP-010 — Portal-Aware Pathfinding

Replace the Manhattan/toroidal objective distance with:

* local BFS,
* known-map BFS,
* portal-aware BFS,
* risk-adjusted BFS.

Compare performance and compute cost.

---

## EXP-011 — Sonar Echoes

Use Protocol 3 sonar echoes as additional information.

Test whether they can improve:

* danger detection,
* corridor detection,
* enemy proximity,
* portal exploration,
* king protection.

---

## EXP-012 — Sonar Team Command System

Build structured messages for:

* king,
* feeder,
* explorer,
* scout,
* defender,
* backup king.

Test whether coordination beats independent local decisions.

---

## EXP-013 — Large-Dragon Combat Restriction

Hypothesis:

Large dragons should not be allowed to make ordinary head-trade attacks.

Test:

* current behaviour,
* only length <=3 may head-trade,
* only length <=4,
* adaptive trade based on role.

---

## EXP-014 — Memory Sharing

Problem:

Split children start with fresh program state.

Test sonar-based sharing of:

* portal endpoints,
* high-value pearl locations,
* exploration targets,
* king location,
* danger zones.

Measure whether redundant exploration falls.

---

## EXP-015 — Full Late-Game Economy

Combine:

* king,
* feeders,
* portal routing,
* king protection,
* adaptive phase,
* pearl prioritisation.

Only run this after individual components have been measured and only their individual best
values are taken into account.

---

## EXP-016 — When to start increasing length

Having the snakes switch to increasing length afte continusouly splitting (current model). But change 300 to try other values.

* Stop splitting after round 300
* Stop splitting after round 100
* Stop splitting after round 150
* Stop splitting after round 200
* Stop splitting after round 350
* Stop splitting after round 400

Only run this after individual components have been measured and only their individual best
values are taken into account.

---

# Search Method

Do NOT immediately brute-force every parameter combination.

Use staged search:

1. Establish a frozen baseline.
2. Test one variable at a time.
3. Keep the strongest few variants.
4. Test interactions between promising variables.
5. Combine only proven components.
6. Re-test the combined strategy on holdout maps.
7. Compare against the frozen v5 baseline.
8. Promote only robust candidates.

This avoids wasting thousands of matches on obviously bad combinations.

---

# Baseline Acceptance Rule

The team's stated minimum requirement is:

> A final candidate should beat v5 on at least 6 different maps.

For automated testing, define "beat a map" precisely, for example:

`candidate wins more games than v5 over repeated games on different maps`

using both side assignments.

A single lucky game is not sufficient.

---

# New Map Rule

Any new `.map` file added to the repository must automatically enter evaluation.

The automation system should:

1. detect the new map,
2. run the frozen v5 baseline,
3. run the current champion,
4. run the newest candidate,
5. record results,
6. add the map to the dashboard,
7. never silently exclude it because it was not known when the strategy was written.


## 2026-09-26 interim decisions after EXP004

TESTED: EXP001 baseline screen 22–0–10; EXP002 local king 21–0–11;
EXP003 local king + feeding 24–0–8; EXP004 frontier 25–1–6 (W/D/L).
All used the same eight maps, seed 101, both sides, v5/v8. EXP004 beat v5
16/16 and v8 9–6–1. EXP003 beat v8 9–7. These are screening results only.

REJECTED for promotion: king alone. UNKNOWN general value: king+feeding,
because only six scheduled games reached round 300. FEED logs on Stronghold
against v8 show 21 and 4 intentional sacrifices in the two side assignments;
both were wins. This is narrow evidence, not a general conclusion.

EXP014 HYPOTHESIS (declared before testing): frontier exploration plus the local
king/self-feeding subsystem can combine early distribution and late growth.
Parent EXP003, adding only EXP004 frontier code/weight 12. Compare with EXP004
and v8 on Default, Stronghold, Queen of Spades and Trophy, both sides, seed 202.
This is a pairwise interaction experiment, not an accepted release.


### Portal sweep interim result

TESTED EXP005/006/007/008: 5%, 10%, 15%, 20% safe-known-portal exploration
respectively scored 12/16, 12/16, 13/16 and 12/16, against the matched baseline
12/16. The 15% single extra win is low-confidence; no portal setting is promoted.


### EXP012 result and next interaction decision

TESTED EXP012 portal-aware objective BFS: 15/16 wins, including 7/8 against v8
and 8/8 against v5, on Colosseum/Default/Devil/Trophy, seed 101, both sides.
This is the strongest four-map screening result so far; independent validation
is still required. EXP009 25% portals scored 11/16, EXP010 adaptive growth 12/16,
and EXP011 trade length restriction 12/16, versus matched baseline 12/16.

EXP014 remains an untested, preserved candidate. The next interaction screen
is changed *before running it* to prioritize the stronger navigation result:
EXP015 = EXP012 plus EXP004 frontier scoring. Compare to EXP012 and v8 on
Default, Stronghold, Queen of Spades and Trophy, both sides, fresh seed 202.
Select the combined candidate only if it beats the parent in this screen and
scores above 0.5 against v8; otherwise advance the simpler EXP012.
Final v5 validation seeds fixed to 303,404,505,606 on all discovered maps.
No tuning will follow holdout results in this batch.


### EXP015 interaction decision / final validation plan

TESTED: adding frontier to portal-aware BFS scored only 2 wins, 1 draw, 5 losses
against the simpler EXP012 on four maps at fresh seed 202. REJECTED combination;
advance EXP012 without frontier, king or feeding. This decision is determined by
the predefined parent-comparison rule, regardless of its remaining v8 games.

Final validation: EXP012 vs v5, seeds 303/404/505/606, both sides, all 11 maps
(88 games). Robustness: EXP012 vs v8 and v4 on untouched Big Empty, Schooltime,
Trauma, seed 707, both sides (12 games). Resource check: EXP012 vs v5, Default
and Big Empty, seed 808, both sides under sandbox (4 games). Total research
ledger budget = 400 games including equivalence/screens; manual infrastructure
smokes are recorded separately. No further tuning on these results.

Before seeing validation, a zero-win map in the eight-game v5 set, or zero wins
in both incumbent side assignments on a holdout map, triggers a regression flag
requiring explanation/replication before promotion. Runtime failures also block
promotion. Passing five maps alone is insufficient.


## Completed screening results — 2026-09-26

|Experiment|Games|W/D/L|
|---|---:|---|
|EXP001-screen|32|22/0/10|
|EXP002-screen|32|21/0/11|
|EXP003-screen|32|24/0/8|
|EXP004-screen|32|25/1/6|
|EXP005-screen|16|12/0/4|
|EXP006-screen|16|12/0/4|
|EXP007-screen|16|13/0/3|
|EXP008-screen|16|12/0/4|
|EXP009-screen|16|11/0/5|
|EXP010-screen|16|12/0/4|
|EXP011-screen|16|12/0/4|
|EXP012-screen|16|15/0/1|
|EXP013-screen|16|13/0/3|
|EXP015-interaction|16|8/2/6|

REJECTED for this release: king alone, 25% portal override, and frontier+BFS interaction. No measured aggregate improvement in the tested schedule: 5/10/20% portals, population-triggered growth, length-limited sprint trades. PROMISING but not promoted: local feeding, frontier alone, 15% portal override, echo caution. Selected for final validation: EXP012 BFS. Results are schedule-specific, not universal claims.


## Acceptance decision — v9 / EXP012

SUPPORTED / ACCEPTED under the predefined local criteria. Release path:
`bots/current/bot-v9`, byte-identical to EXP012, SHA-256 source fingerprint
`ab9771e0741ba4ae76b71483ae3dc9e1bdc742cf1b31ca1a2a2f0ea02a7b3ad5`.
Parent v8 via EXP001. Only strategic change: remembered-topology pearl BFS.

Final validation: **82 W / 1 D / 5 L**, 88 v5 games, all 11 maps, seeds
303/404/505/606 and both sides. Ten maps pass; Trauma does not. Earlier live
commentary tallies were provisional; this final ledger tally is authoritative.
Native v8 comparison: 11 W / 3 L (7–1 screen, 4–2 holdouts). v4 holdouts: 4–2.
The tested source participated in 124 native games plus four sandbox checks.
Batch-native Elo: 1031.5; this unbalanced sequential cohort is not comparable to
v8's historical 1220.1. Promotion rests on direct comparisons and validation,
not being the highest Elo entry in this differently scheduled cohort.

Sandbox Default and Big Empty, both sides, seed 808: no candidate CPU-limit or
invalid-action events; maximum reported candidate turn cost 40.6M / 100M.
Frozen v5 exceeded CPU limits on Big Empty (28 / 16 events by side), so resource
games are excluded from strategy Elo. Raw output and supplemental diagnostics
are retained; old match rows were not rewritten when metric extraction improved.

Important weakness: Trauma has 4 W / 1 D / 3 L against v5 in validation, a 1–1
holdout pair against v8, and 0–2 against v4 by one segment each. This is not an
incumbent side-pair collapse, but late survival needs more work. No claim of
uniform map superiority is made.

Stopping condition: initial **400-game research budget exhausted**, with two
manual infrastructure smoke games recorded separately. No plateau is claimed.
All candidates, including failed/untested variants, remain available. Exact
states are preserved in manifests and tar archives; no new Git commit is claimed.
Next promising work: independent replication, long-dragon survival on Trauma,
selective risk-aware BFS, structured portal knowledge sharing, and more rigorous
king/feeding election/timing experiments. No official submission was made.

## v10 feature implementation — 2026-09-26

User superseded broad search with two mutable development versions, v10 and
v10.1, compared only with each other and frozen v8/v9. Each tested source is
archived by SHA-256 under `battlecode-automation/experiments/v10/`.

TESTED: v10-r1-screen, seed 7102, Arena/Default/Stronghold/Trauma, both sides,
v8 and v9, 16 parsed games: 8W/2D/6L; vs v8 6/1/1; vs v9 2/1/5. No runtime
failures. REJECTED for champion promotion: loses to v9. Nine additional smoke
feeding events were confirmed; named growers collected 9/16 donated pearls.

TESTED: v10.1-r1-screen, same screening seed/maps, v10/v8/v9, 24 parsed games:
13W/5D/6L. Against v10 5/1/2; against v8 4/2/2; against v9 4/2/2. Seventy
logged feeding attempts; no runtime failures. SUPPORTED for further validation,
not champion promotion. Changes: milder early food reservation, delayed larger
patrol areas, terrain sharing from claims, checked grower split reversal.

HYPOTHESIS: v10.1 revision 2 adds undersized-grower demotion to feeders and
short-lived shared enemy-head targets. Safety fixtures cover forwarding TTL,
terrain boundaries, stale-recipient abort and role demotion. Sandbox checks
and broader fresh-seed comparisons are pending.

TESTED: v10.1-r2 sandbox, seed 7199, both sides on Big Empty and Stronghold:
2W/0D/2L. Stronghold 2/0/0, Big Empty 0/0/2. All games fully parsed, zero
candidate CPU-limit events or invalid actions. Maximum candidate turn cost
65.6M points (100M cap). WebAssembly header-cache invalidation repaired in
harness by including the full source fingerprint in temporary project names.

TESTED: v10.1-r3-allmaps, seed 7201, every one of 11 maps, both sides versus
v8/v9, 44 parsed games: 25W/0D/19L. Against v8 15/0/7; against v9 10/0/12.
126 FEED intents and zero runtime failures. REJECTED for champion promotion:
aggregate v9 regression. This exact source is retained as mutable development
slot `bot-v10`; historical initial v10 remains in its hash-named archive.

HYPOTHESIS: v10.1-r4 concentration revision narrows right-of-way to actual
pearls, limits initial territory to max(80, 4*length) cells, and reduces the
stable ID-assigned grower fraction from 1/4 to 1/8. Already-large dragons and
founders still qualify. Screen: fresh seed 7202, both sides, Big Empty,
Stronghold, Default, Trophy, versus stable v10 and v9 (16 games). This is a
combined coordination revision, not evidence isolating each component.

TESTED: v10.1-r4 concentration, 16 fully parsed native games: 9W/0D/7L;
4/0/4 against stable v10 and 5/0/3 against v9. Stronghold 4/0/0 across both
opponents; Big Empty 1/0/3. Advanced to fresh all-map validation, source frozen.

TESTED: v10.1-r4-validation, fresh seed 7203, all 11 maps, both sides, 44 parsed
native games: 30W/0D/14L; vs v8 17/0/5, vs v9 13/0/9. No runtime failures.
SUPPORTED as best new prototype in this bounded development pass, not as a
statistically established replacement for v9. Exact v5 promotion gate was
excluded by the user's four-bot scope, so CURRENT_CHAMPION remains v9.
Final source resource checks and packaging are recorded in the v10 report.

TESTED: final v10.1 source, sandbox Big Empty seed 7299, both sides. Both games
lost to v9 strategically; both passed resource validation: maxima 63.8M / 63.7M
points, zero CPU-limit events and zero invalid actions. This variability is
retained in the report, not presented as an unconditional Big Empty improvement.
Completed pass: 150 append-only ledger games plus one manual smoke. Final source
has 60 native and two sandbox games. ZIPs verified byte-for-byte against source;
no official submission and no automatic champion promotion. Final report:
`battlecode-automation/experiments/v10/REPORT.md`.

## V11-R1 — navigation subsystem (2026-09-26)

Parent frozen v10.2. TESTED: native fixtures pass for both strict and shortcut
variants. Stronghold smoke seed8100, both sides: v11 1–1 vs v10.2; baseline
v10.2 1–1 vs v10.1. Four sandbox matches seed8199: v11 0–4 vs v10.2.
No invalid actions or CPU-limit events, but Big Empty peaks 99.2M/99.8M
instructions, uncomfortably near 100M. NOT ACCEPTED as an improvement.
Sources and raw replays are fingerprinted under experiments/v11.

OBSERVED: replay inspection shows real two/three-step defensive sprints and
length-preserving reversal splits. Counting a dragon's previous-round length
at its death falsely labels the discarded length-two parent as a long death;
v11 supplemental metrics now track event-level bodies, including splits.
HYPOTHESIS for R2: cache repeated route distances and body occupancy, allow
cycle expansion only when whole-body ordering is preserved, and let verified
cycle motion override static flood-space penalties. These changes address both
instruction margin and premature abandonment of safe narrow patrol corridors.

## V11-R2 — cache, ordered expansion and cycle priority

TESTED: seed8101 four-map screen, both sides vs v10.1/v10.2: **10W/2D/4L**,
5W/1D/2L against each. Native fixtures include 500 invariant-preserving cycle
moves, tail collision rejection and bounded emergency dodge simulation.
Advanced to fresh-seed8102 validation across all 11 maps and five baselines.

## V11.1-R2 — eight-step remembered-terrain lookahead + cycle shortcuts

TESTED: identical seed8101 screening schedule: **9W/2D/5L**. Did not improve
aggregate screening over strict v11 R2. Archived before replacing the v11.1
working slot. This does not establish that deeper planning is always worse.

## V11.1-R3 — productive patrol territory

HYPOTHESIS: safe rectangles that lack spawning tiles strand growers in empty
loops. Build on v11 R2, requiring four known spawning cells in each new claim.
Predeclared sparse-map screen: Devil, Schooltime, Stronghold, Trauma; seed8103,
both sides vs v11 and v10.2. Exact sources, per-game metrics, runtime and raw
outputs are in the separate append-only experiments/v11 ledger.

## V11.1-R3 outcome

REJECTED as a strength improvement: seed8103 sparse-map screen finished 4–4
against v11 and 3–5 against v10.2. Source archived. The four-productive-tile
claim requirement alone did not improve this sample.

## V11.1-R4 — limited early growers

HYPOTHESIS supported by the reference replay's early longest-dragon trajectory,
not yet established in our bots: waiting until250 for every grower leaves too
little time to build a large survivor. Preserve v11 R2 navigation and normal
round250 phase; allow a stable 1/32 ID-hash subset to retain from100 only after
16 living allies exist. Latch the role once selected. This avoids choosing a
map-specific ID and keeps early swarm creation first. Screen both current bests
on Big Empty, Default, Stronghold and Trophy, both sides seed8104.

## V11.1-R4 outcome and R5

REJECTED: early growth screen finished **5W/0D/11L**. Big Empty reached final
longest73 and59 in separate games, but Stronghold and Trophy regressed severely.
This is evidence of a map-dependent tradeoff, not an improved default.

R5 HYPOTHESIS: retain v11 R2 navigation but restore v10.1's worker exploration
weight6/cap360 and immediate pearl values (grower2800, worker850). This isolates
whether v10.2's more forceful movement scores undermine the new safety logic.
Same four-map, both-side screening seed8101 as R2; parent and candidate source
fingerprints distinguish this controlled comparison from replication.

## V11-R2 all-map validation

TESTED: fresh seed8102, all11 maps, both sides, 110 games. v10.2:11W/2D/9L;
v10.1:8W/1D/13L; v10:9W/1D/12L; v9:8W/2D/12L; v8:16W/2D/4L.
REJECTED for promotion: it does not beat the required opponent pool despite
its promising screen. All110 outcomes parsed with replay/CLI agreement.

## V11.1-R5 and R6

R5 restored v10.1 food/exploration coefficients and screened 9W/2D/5L.
R6 fixes a source-confirmed communication compatibility bug: the inherited
4-bit C claim tag overlaps legacy C8/CA/CB packets. For teamB, certain legacy
messages pass the newer claim's team/time/bounds checks with incorrect field
interpretation. New claim/status/terrain tags5/6/7 do not overlap any tested
baseline format. A deterministic fixture reproduces the ambiguous packet and
verifies rejection. No claim of authenticated sonar is made.
R6 advances to seed8105, all11 maps, both sides, five baselines (110 games).
Historical R5 and R2 source states remain archived.

## V11.1-R6 full validation result

TESTED: seed8105, 110 parsed games, **63W/6D/41L**.
Against v10.2:9W/2D/11L; v10.1:13W/1D/8L; v10:12W/1D/9L;
v9:11W/1D/10L; v8:18W/1D/3L. Best broad new candidate so far, but fails the
requested positive result against every baseline. No promotion.

## Final comparison budget and selection rule

R7 directly ports the subsystem onto v10.1 and screens the same four maps and
seed8101 (16 games). It must score at least12/16 to displace the more extensively
validated R6 for the final stage; otherwise restore R6 exactly from its tarball.
Then run a fixed110-game all-map/all-five-baseline replication at seed8110,
and22 mutual v11/v11.1 games at seed8111. Complete all games regardless of the
running score. Final source also needs exact-source sandbox checks. Stop this
session after those checks and report success or failure honestly; do not add
seeds selectively until a losing comparison turns positive. This is a bounded
final validation after multiple rejected strategy families, not automatic
candidate generation or official submission.

## Final diagnostic exception: R8 opening trades

R7 did not reach the12/16 selection score; R6 was restored exactly (hash checked).
Before final replication, replay inspection found a concrete early failure:
R6 lost9/10 Colosseum games, generally by round38–81, dominated by head-to-head
rather than long-snake deaths. The inherited policy disables direct small head
trades until50 and six allies; v10.1 has no such restriction.

Additional bounded diagnostic, explicitly a CODE change rather than selective
replication: allow direct trades only at length<=3 with at least2 living allies,
retaining all large-grower protection. Eight games: Colosseum, both sides,
v10.1/v10.2, seeds8112/8113. R8 must score>=5/8 to enter the fixed110-game
seed8110 full-pool run; otherwise restore R6. This is the final diagnostic
exception to the preceding budget, motivated by a specific observed failure.
Do not extend the final replication after seeing its outcome.

## Final v11 decision — 2026-09-26

R8 diagnostic:6W/1D/1L, passed its5/8 score criterion. Fixed final validation,
seed8110, all11 maps, both sides, five opponents: **62W/16D/32L**.

|Opponent|W / D / L|
|---|---:|
|v10.2|8 /4 /10|
|v10.1|13 /3 /6|
|v10|15 /3 /4|
|v9|11 /3 /8|
|v8|15 /3 /4|

Final mutual seed8111 vs v11: **8W/4D/10L**. v11.1 is the strongest new
full-pool-tested candidate in this session, but is not a universal winner.
**REJECTED for champion promotion.** The requested all-baseline improvement was
not achieved, and the historical v5 gate was not tested. Existing competition
bots and champion bookkeeping are preserved; nothing was submitted.

Exact final v11.1 source: `860e7ea84e5571907632bd51de196e0935576d7fc930f3aef2b66dcd4413ac30`.
Exact final v11 source: `760fed7cf9026f9fcbd89f9e3e400c9b07f03b7d9f662f26ee1edbc0160582ba`.
528 games total:514 native,14 sandbox; zero parse failures. Final v11.1 has
140 native games (110 validation,22 mutual,8 diagnostic) and4 exact-source
sandbox games. Its sandbox max51.8M instructions; zero CPU-limit events or
invalid-action deaths. v11 exact-source sandbox also passes.

Bounded stopping condition: completed the fixed final schedule after the
recorded prototype comparisons and final diagnostic exception. No additional
seeds were added to turn the unfavorable v10.2/mutual result positive.
The untested high-food-score × early-small-trade interaction is a concrete
next hypothesis, not a claimed discovery. All failures, raw outputs, replays,
source snapshots, packages and decision metadata remain in experiments/v11.
