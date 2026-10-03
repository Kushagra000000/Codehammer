# bot-v10.4.1 — v10.4 rewrite, locally tested

This replaces the rejected v10.4.1 implementation, which is preserved in the
v1041-rewrite experiment directory. Only main.cpp and coordination.hpp differ
in implementation from the current local v10.4. Existing phase thresholds,
feeding, sonar protocol, combat and general pathfinding are retained.

1. Choose patrol rectangles using known recurring spawn density and measured
   countdown reset intervals; prefer enough area for growth. Keep established
   productive loops rather than rebuilding them on each pearl detour. This is
   selection among locally known clear rectangles, not global map knowledge.
2. While patrolling, look for visible reachable pearls within four clear steps
   (PATROL_FOOD_RADIUS). Give the existing patrol preference to that first food
   direction. This applies whenever a grower patrol is active, without the old
   round-475 gate. Existing collision, threat, space and larger-ally priority
   checks remain. The normal cycle preference resumes when no such pearl exists.
   There is no new multi-turn detour planner or forced move.
3. Cornered workers and growers may split L-2 segments from the tail if legal,
   leaving two in the parent. Do not split merely because a legal head move has
   little estimated future space. Crucially, a doomed head need not see its tail:
   an offscreen child gets its own fresh vision and chance to escape. Known sealed
   or threatened tails are rejected. Unknown-tail rescue remains uncertain; team
   cap64 and minimum split length4 still apply. Long children retain existing heir
   behavior. Not every trap is escapable.

Build and behavioral fixtures passed. Official-toolkit local matches and replay
analysis were run on all11 repository maps, including Stronghold (the same map
geometry as the supplied M237041 reference). Read the experiment REPORT.md for
results by exact revision. Functionality is verified; universal superiority and
reference-like length151 on Stronghold are NOT established. No official submit
or champion promotion occurred.

Run a manual comparison from the repository root:
.battlecode-codex/bin/python battlecode-automation/scripts/compare_bot.py --bot bot-v10.4.1
