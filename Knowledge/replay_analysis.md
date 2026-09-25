# Replay analysis — 2026-09-26

OBSERVED: no pre-existing replay files were present at audit time. Historical
knowledge references replay IDs but files were absent. New local replays are
retained under `replays/` and ignored by Git.

The toolkit's shipped viewer contains a Cap'n Proto decoder. Research extracts
its decoding portion locally, checks CLI/replay winner agreement, and records
final longest dragon, total length, population, split timing, maximum population,
death reasons, head-visited tiles and union of 7×7 head vision windows.
Coverage is team observation coverage, not proof of shared bot memory.
Unimplemented portal/king/sacrifice metrics remain null until measured reliably.

Arena smoke, v8 A vs v5 B, seed 20260926: v8 won by elimination in round 39,
25 final dragons, longest 4, total length 63; v5 had no survivors.
The short game provides no evidence for late feeding or final growth.


Metrics schema 4 refines maximum population using every split/death event, and
counts observation coverage at turn start, when the dragon actually receives
vision. Earlier schema coverage was a union of head-position vision footprints
(and earlier maximum population was sampled at round boundaries). Historical
rows retain their original schema; do not treat these definitions as identical.
Successful portal transitions are now counted from canonical entry edges and
observed matching exits. Portal-associated deaths remain UNKNOWN. FEED log
counts measure explicit intentional sacrifice commands in instrumented candidates.


## Trauma holdout weakness, seed 707

OBSERVED: EXP012 lost both Trauma games to v4 at round 500 by a single segment
in the primary tiebreak (4–5 and 6–7). In the A-side game, EXP012 explored 98.4%
of the map and reached a sampled longest length 9 at round 450, but finished at
4 after substantial self-collision attrition. v4 explored only 35.4% and retained
length 5. In the B-side game both teams had limited coverage around 41%.
Coverage/population alone are therefore not reliable substitutes for preserving
the longest dragon. This is a documented narrow-map weakness, not a claim that
BFS caused every self-collision. Root cause and remedies remain HYPOTHESES.

Against incumbent v8 on Trauma the finalist split the side pair 1–1: a 4–7
longest-length loss, then a longest-length tie 6–6 won on total length 36–27.

## OBSERVED — user-supplied M237041 (2026-09-26)

Stronghold, 48x24, 500 rounds; A wins, longest 151 versus 102. A already had
length 29 at round 100 and 79 at 250. The largest-dragon identity changes via
splits (A: 0 at 300, 419 at 325, 459 from 350 onward). Twelve small late self-deaths
occur near the prior-round leader head; this is evidence consistent with feeding,
not proof of intent. Raw replay copied to `replays/M237041.replay`, not committed.
Detailed sampled leaders: `battlecode-automation/experiments/v10/reference-analysis.json`.

TESTED — v10 Stronghold smoke, seed 7101: nine FEED logs correspond to nine
self-deaths; sixteen dropped pearls were subsequently collected, nine by the
named recipient and seven by other allies, none by enemies. This demonstrates
working donations but also imperfect resource priority. Accounting script:
`battlecode-automation/scripts/v10_replay_analysis.cjs`.

TESTED — final v10.1 source, Stronghold seed 7203, both sides against v9:
four confirmed self-feeds produced eight pearls. Named growers collected five,
another ally collected one, two remained uncollected, no enemy collected them.
One checked grower split reversal was executed. Final lengths were 18–15 (win)
and 17–18 (loss). Detailed JSON files are in the v10 experiment directory.
The reference and local Stronghold replay match exactly in dimensions,
symmetry, horizontal/vertical edges, portals, respawn rules and initial dragons.
