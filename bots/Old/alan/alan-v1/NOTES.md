# alan-v1

Parent: `bots/current/bot-v10.3.3`

## Hypothesis

The parent bot should be more robust across map sizes and incomplete information if it replaces fixed phase thresholds and optimistic estimates with bounded adaptive policies, keeps a late backup, and spends communication and search work only where it can be validated.

## Changes

- Extracted active tuning values into `config.hpp`; removed obsolete switches and fixed-round constants.
- Scales swarm target and phase transitions with map dimensions, unit limits, and current team growth.
- Limits center pull to the opening and disables it around nearby food, allied crowding, or an observed barren center.
- Treats unknown portals as a last resort when a known destination exists.
- Bases attacks and trades on confirmed visible enemy length rather than hidden-body estimates.
- Caps per-turn enemy reach analysis and territory search work.
- Learns a map symmetry only after observations eliminate the alternatives, then mirrors immutable edge topology only.
- Uses disjoint packet tags, wider keyed integrity fields, freshness checks, and conservative terrain sharing.
- Retains one viable late backup rather than making the elected king a single point of failure.
- Requires a complete body model and a sufficiently collectible death drop before a feeder sacrifices.
- Scores patrol rectangles for observed recurring food productivity as well as safety and area.

## Validation status

- Native C++20 `-O2 -Wall -Wextra -pedantic` compilation: passed locally.
- Focused fixture for adaptive phases, center guards, backup election, packet parsing, terrain sharing, feeding collectibility, confirmed-length trades, and symmetry: passed locally.
- The same focused fixture under AddressSanitizer and UndefinedBehaviorSanitizer: passed locally.
- Engine matches and judge-like sandbox validation: not run in this checkout because neither Python nor `unswbc` is installed or on `PATH`.

No Elo or win-rate claim should be made until the ladder has run. Once the toolkit is available, use:

```bash
python3 battlecode-automation/scripts/autorun_matches.py --mode new --workers 2
python3 battlecode-automation/scripts/autorun_matches.py --mode new --sandbox --workers 2
```

Inspect overall Elo, map splits, games played, and head-to-head results before promotion.

## Residual constraints

- Sonar integrity checks reject accidental collisions and unsophisticated spoofing, but a shared source-level key is not cryptographic authentication against an opponent that copies the protocol.
- Patrol remains rectangle-cycle based; productive rectangle selection and safe general navigation fallback reduce, but do not remove, geometric limitations.
- Adaptive formulas and heuristic weights are hypotheses pending match evidence.
