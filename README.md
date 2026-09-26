# CodeHammer
UNSW battleCode

## Latest v11 result

528 local games completed. Final v11.1 scored62W/16D/32L over110 fresh games:
positive against v10.1/v10/v9/v8, but8W/4D/10L against v10.2. No promotion or
official submission. Keep existing competition bots; both new projects remain
reversible research candidates. See `battlecode-automation/experiments/v11/REPORT.md`
and `battlecode-automation/reports/v11.html`. Exact source ZIPs are beside the report.

## Active development: v11

The current research slots are `bots/candidate/bot-v11` and `bot-v11.1`.
Frozen opponents are v8, v9, v10, v10.1 and v10.2. Both sides and every map
are discovered by the bounded evaluator; source snapshots and results are
append-only. Development slots are not automatically promoted.

```bash
.battlecode-codex/bin/python battlecode-automation/scripts/v11_matches.py --candidate bot-v11 --opponents bot-v11.1 bot-v10.2 bot-v10.1 bot-v10 bot-v9 bot-v8 --seeds 8201 --experiment v11-manual-replication --dry-run
```

Dashboard: `battlecode-automation/reports/v11.html`.
Design and research: `Knowledge/v11_design.md`.

## Historical development: v10

The broad candidate search is retired from the active workflow. Only `bot-v10`
and `bot-v10.1` are developed, with frozen `bot-v8` and `bot-v9` as opponents.
The default ladder respects this four-bot allowlist. Historical experiments
remain available for audit; no candidate generation or promotion runs automatically.

```bash
.battlecode-codex/bin/python battlecode-automation/scripts/v10_matches.py --candidate bot-v10.1 --opponents bot-v10 bot-v8 bot-v9 --seeds 7201 --experiment v10-manual-validation --dry-run
```

Remove `--dry-run` to run that bounded schedule on every discovered map.
Results: `battlecode-automation/reports/v10.html`; design: `Knowledge/v10_design.md`.

## Setup
1. Install uv: https://docs.astral.sh/uv/
2. `uv tool install unswbc`
3. `unswbc auth set bc_...` (get your own key from https://game.battlecode.au/team)
4. `git clone https://github.com/Kushagra000000/Codehammer.git`

## Run a match
`unswbc run maps/arena.map mybot mybot`

## Replay
Open the `.replay` file in VS Code (viewer auto-installed) or at https://game.battlecode.au/visualiser

## Historical research environment and records

The 2026-09-26 research environment is `.battlecode-codex` (Python 3.13,
`unswbc==1.1.0`). Use its CLI explicitly; do not silently substitute a global
engine. Existing helpers already support Protocol 3.

```bash
.battlecode-codex/bin/python battlecode-automation/scripts/research.py --candidate bot-v9-exp003 --opponents bot-v5 bot-v8 --maps default.map stronghold.map --seeds 202 --experiment EXP003-replication --dry-run
.battlecode-codex/bin/python battlecode-automation/scripts/research_report.py
.battlecode-codex/bin/python battlecode-automation/scripts/analyse_maps.py
```

Remove `--dry-run` to execute a declared experiment. Matches use two workers,
isolated bot copies, explicit seeds, complete raw logs and replay/CLI agreement.
The append-only research ledger is separate from historical ladder results.
The dashboard is `battlecode-automation/reports/research.html`. Replays stay local.
Promotion is gated by the preregistered plan in the research experiment directory;
the normal ladder no longer replaces the champion solely because of Elo.
