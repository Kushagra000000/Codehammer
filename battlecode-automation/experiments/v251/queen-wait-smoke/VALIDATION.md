# bot-v25.1.0 queen wait-split validation

Date: 2026-10-06. Base: `bots/New/current/bot-v25.0.0-omar`.
Candidate: `bots/New/current/bot-v25.1.0`.

The candidate copies the base folder. Only `endgame.hpp` changes executable
behavior; `queen.hpp` updates the description of that behavior. The base is
unchanged.

The queen keeps the existing test for having no survivable move. When that test
requests an escape split, she now detaches exactly two segments and keeps her
head and remaining body in place. She repeats the normal movement assessment
next turn. This also works when her full body is not visible or the child has
no exit. She is not marked as a sacrificial escape remnant. The existing sonar
status and body handover still run.

Length must be at least four, and the existing escape-slot reservation and team
cap checks still apply. At length two or three, another split is illegal. This
buys time for a blocker to leave; it cannot guarantee escape from a permanent box.
Normal queen seeding, movement, worker escapes, and all other strategy settings
are unchanged.

## Focused tests

```sh
python3 battlecode-automation/scripts/test_v251.py
```

**53 checks passed** using unswbc 1.2.9, the judge's C++ compiler, metered wasm
bots, and the actual game engine. The complete output is in `behavior-tests.txt`.

Coverage includes both queen IDs, all queen phases, lengths 4/5/8/10, repeated
splits, incomplete and complete body knowledge, a boxed child that dies, movement
after release, minimum-size boundaries, unit cap, existing reserved slots, and
matching baseline actions for ordinary queen and worker behavior.

Actual engine trace with an ally blocking the queen's only exit:

| Engine round | Queen head before action | Queen length | Action |
| --- | --- | ---: | --- |
| 0 | (8, 8) | 8 | SPLIT 2 |
| 1 | (8, 8) | 6 | SPLIT 2 |
| 2 | (8, 8) | 4 | MOVE E |
| 3 | (9, 8) | 4 | MOVE S or E, depending on body layout |

The blocker clears the exit after the queen's turn in round 1. The queen survives
and moves on round 2. In the same straight-body fixture, the base issues SPLIT 6,
retains only two segments, and dies before the blocker clears. A longer blocked
fixture confirms 8 -> 6 -> 4 -> 2 through three consecutive SPLIT 2 actions.

## Full-match smoke tests

```sh
XDG_CACHE_HOME=/private/tmp/unswbc-v251-cache python3 battlecode-automation/scripts/v221_eval.py \
  --a bots/New/current/bot-v25.1.0 --b bots/New/current/bot-v25.0.0-omar \
  --maps maps/default_small.map maps/schooltime.map maps/slithery_fight.map \
  --seeds 25101 --out battlecode-automation/experiments/v251/queen-wait-smoke \
  --sandbox --workers 2
```

The existing paired evaluator stages source snapshots using hashes that include
headers, avoiding the toolkit's header-cache limitation. Source and map hashes
are recorded in `manifest.json`; per-match results and replay paths are in
`matches.jsonl`.

- Six completed 500-round games: **3 wins, 0 draws, 3 losses**.
- Zero evaluation errors and zero bot runtime errors across both versions.
- Candidate maximum measured turn cost: 94,449,969 of 100,000,000 points.
- Each map was played from both sides with seed 25101.

This small smoke sample does not establish a win-rate improvement. The focused
engine scenarios demonstrate the requested behavior; the matches check that the
complete bot continues running under the sandbox limits.
