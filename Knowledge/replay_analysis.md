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

## v11 event-level replay checks — 2026-09-26

OBSERVED: v11 R1 Stronghold dragon111 used a two-step sprint at488 and a
three-step sprint at489, then transferred18 segments to a child at493. Its
self-death at494 was the discarded length-two parent, not a length20 grower.
A previous-round-length death metric would misclassify this. The new
`v11_replay_analysis.cjs` tracks split bodies and movement head/tail updates
before counting long deaths. It also reports multi-step actions and actual
whole-body ordering within logged rectangular claims; ordering does not prove
intent or protection from enemies.

OBSERVED: R6 Colosseum lost9/10 games across five baselines at seed8105, usually
ending by38–81. Losses were dominated by head-to-head deaths with small swarms.
That motivated the isolated opening-trade diagnostic; it was not attributed to
Hamiltonian self-collision. Original replays and all failed candidate replays
are preserved unchanged.

## OBSERVED — v10.4.1 rewrite validation (2026-09-27)

The supplied M237041 replay and local stronghold.map have identical terrain,
portal, starting-position and spawn-range data (map-name case differs).
Final native-screen analysis reconstructed bodies and checked against toolkit
snapshots:176 patrol-food actions,125 departures from the nominal cycle,167
food collections within four own turns,30 windows including sacrifice drops.
No same-turn food-action deaths; this does not imply future safety.135 reverse
splits produced134 birth-round survivors and112 five-round survivors. One
distant-tail child on BigEmpty was head-killed before its first move.
Detailed exact event rows and per-game summaries: `battlecode-automation/experiments/v1041-rewrite-20260927T113309Z`.

OBSERVED final sandbox example: Stronghold round497, trapped length38 dragon158
split a36-length child438 at an offscreen tail. Child438 moved north immediately,
survived, regrew to38 and was the final longest dragon in a38-vs35 win. This is
behavioral evidence, not an isolated causal win attribution. Final13-game
screen/sandbox totals:51 offscreen-tail escapes,50 birth-round survivors.
