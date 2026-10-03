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
