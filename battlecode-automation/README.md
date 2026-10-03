# battlecode-automation

Tools for running matches and judging candidates. Outputs for the new rules go in
`data/`, `experiments/` and `reports/` (empty on 2026-10-03). Everything from the old
rules, including the old automation guide, is in `archive/pre-2026-10-02/`.

## Active scripts (`scripts/`)
| Script | Use |
| --- | --- |
| `v12_eval.py` | Play bot A against bot B on all maps, both sides, explicit seeds (run with `--help`) |
| `v12_paired.py`, `v12_pairs.py`, `v12_report.py` | Pair up and summarise `v12_eval.py` ledgers |
| `autorun_matches.py` | The older ladder runner (Elo style). Its state files are now in `archive`; use only if needed |
| `compare_bot.py` | One game per opponent and map, screening only. Paths assume the old `bots/current`; check before use |
| `analyze_verbose.py`, `replay_metrics.cjs`, `resource_diagnostics.py` | Analyse a verbose run or replay |
| `analyse_maps.py` | Per-map statistics (rerun after adding maps) |
| `validate_candidate.py`, `check_setup.py` | Sanity checks |

Some scripts still assume the pre-October layout (`bots/current`, `bots/candidate`).
Fixing paths to `bots/New` and `bots/Old` is a good first `infra` task.

## Planned
`gate.py`: runs a candidate against the champion and the panel in `panel.json` with
paired seeds and a sequential probability ratio test (SPRT), then prints PROMOTE,
REJECT or INCONCLUSIVE. Spec: see the project setup guide, step 6.

## Match basics
`unswbc run MAP BOT_A BOT_B [--seed N] [-v] [--sandbox] [-o DIR]`. Games are repeatable for a
given map, side and seed when the bots are deterministic.
