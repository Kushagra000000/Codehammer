# bot-v12.0-kushagra — preregistered evaluation plan (written before any v12 match result was inspected beyond the 2-game arena smoke test)

Baseline: bots/current/bot-v10.3.3 (unchanged, sha256 of main.cpp d82a7ea2…).
Engine: local unswbc 1.1.0 (engine wasm shipped in the package), native build; sandbox subset separately.
Runner: battlecode-automation/scripts/v12_eval.py (every map under maps/, both sides per map/seed, explicit seeds;
deterministic bots => a (map, seed, side) game is reproducible without --sandbox).

Seeds
- Development: 1201 1202 (may be inspected; any change after inspecting them is a new candidate name).
- Holdout: 1301 1302 1303 (run once, only on the final candidate; never used for tuning).
- Diverse opponents (v10.5, v10.4.1, v9, v11.1): seed 1401, all maps, both sides; baseline v10.3.3 is run on
  the identical schedule for comparison.

Acceptance criteria (declared in advance)
1. Correctness: all fixtures in test_v12.cpp pass; zero ERROR games; zero v12 "no valid action" deaths;
   sandbox points/turn max for v12 not more than 20% above v10.3.3 on the sandbox subset.
2. Head-to-head vs v10.3.3 on HOLDOUT seeds: score s = (W + D/2)/N. "Non-regression" if s >= 0.5.
   "Improvement established" only if the 95% Wilson lower bound of the win share (draws as half) > 0.5.
   Otherwise report as not established, whatever the point estimate.
3. Diverse opponents: v12 aggregate score not more than 5 percentage points below v10.3.3's aggregate on the
   same schedule.
4. Preserve escape behaviour: GROWER_REVERSE children outlive their birth round in >= 95% of cases
   (v10.3.3 audit: 89/89).
5. Report, per side: final longest (approx.), elimination rate, W/D/L per map, deaths by reason/intent/length,
   feed recipient-growth proxy, portal-turn deaths per traversal.
