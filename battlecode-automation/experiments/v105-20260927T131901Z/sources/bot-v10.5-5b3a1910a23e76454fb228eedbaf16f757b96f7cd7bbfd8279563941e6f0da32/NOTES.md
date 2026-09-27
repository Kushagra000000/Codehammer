# bot-v10.5

Parent: bot-v10.4.1. This experiment is restricted to opening center control,
the alpha-feeding start round, and the hash-selected grower fraction.
All strategy parameters are near the top of main.cpp.

The opening adds modest attraction towards a central region. Nearby food takes
priority. Observed crowding or poor central pearl-spawn coverage disables the
extra attraction; the extra pull expires at the configured round. Existing
collision checks, pearl pathfinding, splitting, sonar, patrol cycles, emergency
escape and late feeding safeguards remain inherited from v10.4.1.

Exact source snapshots, maps, seeds, raw engine output, replays and results are
preserved under battlecode-automation/experiments/v105-20260927T131901Z.
Use its FINAL_REPORT.md for the selected parameters and measured results.
No official submission or automatic champion promotion is performed.

Selected parameters: center coefficient300 outside radius3 through round39;
food priority within4 objective-cost steps;extra center attraction disabled at
3 visible friendly central heads or observed barren center;alpha feeding450.
Grower selection remains1-in-8:1-in-16 tied the11/16 win screen and1-in-32
could not exceed it. No role,combat,pathfinding,patrol or splitting code changed.
Fresh validation uses all11maps and v9,v10,v10.1,v10.3.1,v11.1, plus a separate
head-to-head against v10.4.1. Results are in the external experiment report.
