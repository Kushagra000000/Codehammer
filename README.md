# CodeHammer
UNSW battleCode

## Active development: v10

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
