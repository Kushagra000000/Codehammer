# alan-v2.2 path-aware sacrifice experiment

Parent: `bots/alan/alan-v2.1`

Hypothesis: the early sacrifice trigger should require a visible, known,
non-portal path to the elected king. The geometric distance used by v2.1 can
declare dragons close through kelp barriers or across portal boundaries.

Implementation: before round 400, breadth-first search the currently visible
terrain for a passable path of at most seven moves to the visible allied king.
Portal edges and paths leaving current vision do not qualify. Round 400 remains
the unconditional hard deadline. No movement, combat, feeding placement, or
other phase logic changed.

Decision: rejected. Across seven late-phase maps, both sides and three fresh
seeds, it finished 20-22 against alan-v2.1 and 21-21 against v10.3.3 (41-43
overall, with no errors). It improved Portals to 5-1 against its parent, but
the strict current-vision requirement regressed Slithery Fight to 1-5 and did
not improve the aggregate.

Report: `battlecode-automation/experiments/v105/experiments/alan-v2-2-path-aware-20260929/report.md`
