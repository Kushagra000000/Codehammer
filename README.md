# Codehammer

Our UNSW Battlecode bot (C++) and the tooling for improving it with AI agents.
Competition: https://battlecode.au (Qualifier 2026-10-10, Grand Final 2026-10-17).

**New here? Read `AGENTS.md`.** It is the working manual for people and AI agents.

## Quickstart
```bash
git clone https://github.com/Kushagra000000/Codehammer.git && cd Codehammer
uv venv --python 3.13 .battlecode-codex
uv pip install --python .battlecode-codex/bin/python -r requirements.txt
.battlecode-codex/bin/unswbc maps
.battlecode-codex/bin/unswbc run maps/arena.map bots/Old/alan/alan-v2.1 bots/Old/alan/alan-v2.1
```
Replays open in VS Code (the viewer is installed by `unswbc`) or at https://game.battlecode.au/visualiser.
Contest key: `unswbc auth set bc_...` (get it at https://game.battlecode.au/team). Never commit it.

## What changed on 2026-10-02
The tiebreak at round 500 now checks the queen dragon's length first. Earlier results
are archived under `battlecode-automation/archive/pre-2026-10-02/`. See `knowledge/game_rules.md`.

## Layout
`bots/New` (active work) · `bots/Old` (frozen history and opponents) · `knowledge/` (rules, strategy,
experiment log) · `battlecode-automation/` (tools, panel, outputs) · `maps/` · `CHAMPION.md` · `HYPOTHESES.md`.
