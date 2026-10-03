# alan-v2.3 portal-aware sacrifice experiment

Parent: `bots/alan/alan-v2.1`

Hypothesis: v2.2 proved that path validation helps on Portals but that requiring
current-vision proof everywhere is too conservative. Preserve v2.1's geometric
trigger on ordinary terrain; only when a portal edge is currently visible must
the dragon also prove a visible, passable, non-portal path of at most seven
moves to the allied king.

Round 400 remains the unconditional hard deadline. No movement, combat,
feeding placement, or other phase logic changed.

Decision: rejected. In an 18-game screen against alan-v2.1 across Big Empty,
Portals and Slithery Fight, both sides and three fresh seeds, it finished 9-9
with no errors. Big Empty was 3-3, Portals improved to 4-2, but Slithery Fight
fell to 2-4. The narrower condition removed v2.2's severe global regression,
but still did not improve the aggregate.

Report: `battlecode-automation/experiments/v105/experiments/alan-v2-3-portal-screen-20260929/report.md`
