# Agent instructions: UNSW Battlecode (Codehammer)

You are an AI agent improving a C++ bot for UNSW Battlecode (https://battlecode.au),
a 1v1 turn-based "dragon" (snake) game. The goal is the best bot on the ladder by the
Qualifier (2026-10-10) and Grand Final (2026-10-17). Progress is measured only by
automated match results, never by intuition.

## RULE CHANGE YOU MUST KNOW (2026-10-02)
After 500 rounds the winner is now decided by, in order: (1) the longer **queen**
dragon (team A's queen is dragon ID 0, team B's is ID 1, observed 2026-10-03, so our queen is whichever matches our side; `ct.get_id() <= 1` means "I am the queen"), (2) the longest living
dragon, (3) the greatest total length, (4) draw. A dead queen has length 0, so a team
whose queen dies loses the tiebreak to a team whose queen lives. Elimination still ends
the game immediately. Full text and sources: `knowledge/game_rules.md`.
**All results from before 2026-10-02 used the old rule and are archived, not trusted.**
The old engine pin (`unswbc` 1.1.0) does not implement the new rule. Use 1.2.9.

## Read first, in this order
1. `knowledge/game_rules.md`: authoritative rules and the rule change log.
2. `CHAMPION.md`: the bot to beat and how promotion works.
3. `HYPOTHESES.md`: the work queue.
4. `knowledge/experiment_log.md`: what has been tried (only post-2026-10-02 entries count).
5. `knowledge/Strategy.md`: reusable lessons. Also `knowledge/v15_design.md` (architecture of the new base bot).
6. `battlecode-automation/README.md`: how to run matches.

## Repo map
```
AGENTS.md  CLAUDE.md  README.md  CHAMPION.md  HYPOTHESES.md  requirements.txt
knowledge/            rules, strategy, experiment log, opponent notes
  archive/            old audits and prompts (pre-rule-change, read-only)
bots/
  New/                active work: bot-vNN.N-hNNN projects created by lane agents
  Old/                frozen history: panel opponents and everything else (read-only)
battlecode-automation/
  scripts/            match runner and analysis tools (generic ones only)
  scripts/archive/    version-specific scripts from earlier rounds
  panel.json          the frozen opponent panel used to judge candidates
  data/ experiments/ reports/   outputs for the NEW rules (start empty)
  archive/pre-2026-10-02/       all results from the old rules
maps/                 .map files (symmetric, wrap-around grids)
replays/              local .replay files (gitignored, never commit)
```

## Environment
```bash
uv venv --python 3.13 .battlecode-codex
uv pip install --python .battlecode-codex/bin/python -r requirements.txt
.battlecode-codex/bin/unswbc maps          # adds missing bundled maps, keeps existing files
.battlecode-codex/bin/unswbc run maps/arena.map bots/Old/alan/alan-v2.1 bots/Old/alan/alan-v2.1
```
Useful flags: `-v` (explains how the game was decided), `--seed N`, `--sandbox`
(judge-like WebAssembly limits), `-o DIR` (replay output). Do not run `unswbc update`
on a bot unless told: it replaces that bot's helper files and the shared maps.
The `.battlecode-codex` folder is per machine and gitignored. In a git worktree, symlink it.

## Roles
- **Lead (Claude Code):** reads results and replays, writes hypotheses, reviews diffs,
  merges to the `integration` branch, updates `knowledge/`. Does not hand-edit lane code.
- **Lane agents (Codex A, Codex B, others):** each owns one lane (`pearls`, `combat`,
  `endgame`) in its own git worktree and branch `lane/<name>`. Implement one hypothesis at a time.
- **Human (Kushagra):** decides promotions and is the only one who submits.

## Workflow for a lane agent
1. Pull, then pick the highest-priority `open` hypothesis in your lane in `HYPOTHESES.md`.
2. Claim it: set `owner` and `status: claimed`, commit on your lane branch.
3. Copy the base bot (`bots/New/bot-v15.0`; its design is in `knowledge/v15_design.md`) to `bots/New/bot-v15.<n>-hNNN/` (never edit a bot in `bots/Old/`).
   Make the smallest change that tests the hypothesis. Keep it C++20 and within the
   judge limits (100M CPU points per dragon per turn, 48 MB per dragon).
4. Build and smoke-test: run one match against the champion; fix any crash or timeout.
5. Evaluate (see below). Record the numbers. Do not claim improvement from a few games.
6. Log the result in `knowledge/experiment_log.md` (template in that file), whether it
   won or lost. Update the hypothesis status. Commit. Stop and report.

## Evaluation
- Use explicit seeds, every map in `maps/`, both sides, against the champion **and** the panel.
- The statistical gate script (`gate.py`, SPRT) is being added. Until it exists, use
  `battlecode-automation/scripts/v12_eval.py` (run it with `--help`) and report W/D/L
  per opponent and per map. A result under about 150 games is "inconclusive", not a win.
- Error or crashed games are reported separately and never counted as W/D/L.
- Games decided at round 500 must record which tiebreak decided them.
- Before any promotion run a `--sandbox` check. Never optimise against one opponent or one map.

## Hard rules
- Never run `unswbc submit` or `unswbc auth`. Never print or store the contest key.
  Only the human submits.
- Never modify `bots/Old/`, `battlecode-automation/archive/`, `CHAMPION.md`,
  `battlecode-automation/panel.json` or the gate script unless the human or lead asks.
- Never commit replays, `.battlecode-codex`, secrets, tokens, or large dumps.
- Work only inside your own worktree. Use your own git identity (`codex-a`, `codex-b`, `claude`).
- Do not run with approvals or sandboxing disabled.
- One hypothesis per bot version. No bulk random edits.

## Definition of done for a task
A bot directory in `bots/New/`, a logged result with game counts and per-map numbers,
an updated hypothesis status, a clean commit on your lane branch, and a short report
that states what you are unsure about.
