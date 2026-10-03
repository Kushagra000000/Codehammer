# bot-v12.2-Kushagra

Copy of bot-v12.1-Kushagra plus one behaviour change (main.cpp only; Config `EXPLORE_*`):

Before round 250 (`EXPLORE_END_ROUND`), non-retaining dragons:
- never reverse, and never make two turns to the same side (right+right or left+left) less than
  6 rounds apart (`SAME_TURN_GAP`), so each newly seen band of tiles is new (`apply_turn_rule`,
  `breaks_turn_rule`, `remember_turns`);
- are exempt when taking or closing on a visible pearl within 4 (`chasing_visible_pearl`), for
  trades, and whenever no roomy rule-abiding move exists (safety first);
- get +45 per never-seen tile a move brings into view (cap 700) and +450 for portal moves.

Fix after analysis run 1: the rule-abiding alternative must have >= 2 exits and time-aware space at
least that of the best rule-breaking move (capped at length+8). Run 1 had 170 TRAPPED_SELF (alan 110)
and more early deaths; after the fix we led 61 vs 9 dragons at round 250.

Logging added for verification: `TURN_RULE n` (moves removed by the rule), `ASSIST_KILL steps`.
Verification runs (`unswbc run --sandbox -v maps/schooltime.map bots/alan/alan-v2.1
bots/Kushagra/bot-v12.2-Kushagra`, random seeds, one match each) are in
analysis-v12.2-vs-alan-v2.1.txt; summarise with battlecode-automation/scripts/analyze_verbose.py.
