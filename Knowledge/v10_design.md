# v10 implementation and evaluation contract — 2026-09-26

User-directed replacement for the broad version search. Only bot-v10 and
bot-v10.1 are edited, and they are compared with each other and frozen v8/v9.
Older candidates, v5, champion v9, replays and historical results are preserved.
No official submission is part of this work.

## Architecture

Parent: v9 portal-aware pearl BFS, per-instance terrain memory, threat maps,
flood-fill safety, spacing, split swarm and deterministic move scoring.
`coordination.hpp` adds roles, territory, patrol and feeding without replacing
the underlying movement safety checks. Splits still start a fresh program.

- Growth specialisation begins at round **250**, inclusive. Founders, stable
  ID-selected growers, already-large dragons and last-survivor backups retain
  length. Small workers keep distinct feeding/attack roles in the late game.
- Grower role, exact advertised length and position are sent as four directed
  Protocol 3 sonar messages. One rotating direction carries a territory claim
  when available. Reception is ray-limited, not global communication.
- Claims describe a rectangle whose internal edges are known clear. Larger
  growers win conflicts; equal lengths use lower ID. Position reports expire
  after two rounds and claims after twelve. No territory is assumed exclusive
  of enemies. Sonar has no authenticated sender identity in the engine.
- Growers can follow a continuous serpentine cycle with a reserved return
  column. The cycle is a scoring preference below safety filtering. Irregular
  terrain, insufficient space and occupied steps use ordinary safe navigation.
- Small workers stage near confirmed growers from round 380. A donation
  requires a visible living allied head, an unblocked short route from its
  mouth to the feeder, and no nearby visible enemy head. The feeder reverses
  into its own body; it never deliberately hits an allied head.
- Small workers pursue locally observed large enemy heads late in the game.
  One- and two-step verified head trades are available; valuable growers do
  not make tactical sprint trades.
- Undersized growers yield their role near a much larger grower. Workers share
  large-enemy positions through short-lived sonar packets; relays preserve the
  observation timestamp instead of reviving stale targets.
- Own head history supplements local vision with remembered own-body occupancy.
- v10.1 also shares static interior claim geometry and can transfer most of a
  trapped grower's length to a reversed split child if its tail has a locally
  checked escape. The child qualifies as a grower by length.

## Limits and hypotheses

HYPOTHESIS: resource concentration improves final longest length. Death returns
ceil(length/2), so feeding transfers resources with loss; it does not create
exponential resources. Reverse collision preserves more yield than spending
segments on a sprint before dying. A screenshot cannot establish enemy intent.

TESTED: native deterministic fixtures exercise phase boundary, food priority,
friendly-head safety, recipient absence, blocked routes, nearby enemy abort,
cycle continuity, claim packet decoding, team filtering and expiry.

OBSERVED: supplied M237041 replay is Stronghold, 48x24, round limit 500. A wins
151 vs 102; A's longest was already 29 at round 100 and 79 at round 250. Largest
dragon IDs change through splits. Twelve late small self-deaths occur within
four tiles of the previous-round leader's head (approximate proximity).
UNKNOWN: source code and deliberate intent of those unlogged reference deaths.

## Evaluation

`v10_matches.py` runs a bounded schedule and never creates variants or promotes.
Both sides, explicit seeds, automatic map discovery, exact source archives,
source hashes, raw logs, strict CLI/replay agreement, append-only ledger and
separate revision reports are retained. A changed v10 source is a new identity.
Do not edit either bot while its batch is running.

The historical v5 promotion rule is not evaluated by this user-requested
four-bot schedule. A v10 recommendation must report this limitation explicitly.
Champion metadata is not automatically changed by these focused comparisons.

## Toolchain reproducibility finding

OBSERVED: unswbc 1.1.0 `clangtool.build` includes C/C++ source bytes but not
header bytes in its WebAssembly cache key. The local harness now incorporates
the full bot fingerprint into each temporary project basename. This forces a
fresh judge build after header-only changes. Engine/toolkit itself is unchanged.

## Current development slots

`bot-v10` now preserves the complete r3 subsystem implementation tested over
all maps (15–7 versus v8, 10–12 versus v9). `bot-v10.1` carries r4: assigned
grower fraction 1/8, actual-pearl right-of-way scoring, and territory capped at
max(80, 4*length). Its initial four-map concentration screen was 4–4 against
stable v10 and 5–3 against v9. Final fresh-seed validation is separate.
Both preserve growth specialisation at 250, feeding at 380 for length <=4,
recipient minimum length 10, radius 3, claim lifetime 12, status lifetime 2,
and tactical sprint maximum length 4. Current identities are recorded in
`battlecode-automation/experiments/v10/current_sources.json`.
