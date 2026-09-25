# Agent instructions for UNSW Battlecode

## Mission
Improve the local Battlecode bot through measurable experiments. Prefer evidence from automated matches over intuition.

## Read first
1. `Knowledge/game_rules.md` — authoritative local rules/constraints.
2. `Knowledge/Strategy.md` — current strategic knowledge.
3. `Knowledge/experiment_log.md` — what has already been tried and what happened.
4. `README.md` — human project notes.
5. `scripts/automation_config.json` — ladder/evaluation settings.

## Bot versioning
- Never overwrite anything in `bots/archive/`.
- Create a new version for a meaningful strategy/code change, preferably named `bot-vN`, `bot-vN.1`, etc.
- A bot project is a directory containing `bot.toml` and its source/helper files.
- The ladder computes a source fingerprint, so changing code under an existing version name is treated as a new code identity. Prefer new version names anyway so experiment lineage stays understandable.
- Do not delete old bots merely because they perform poorly.

## Evaluation loop
After creating a candidate:

```bash
python3 scripts/autorun_matches.py --mode new --workers 2
```

For a full round-robin re-evaluation:

```bash
python3 scripts/autorun_matches.py --mode all --workers 2
```

For judge-like resource checking:

```bash
python3 scripts/autorun_matches.py --mode new --sandbox --workers 2
```

Use `--dry-run` before expensive tournaments if changing the schedule.

## Decision discipline
- Do not promote a candidate solely because it wins one short match.
- Inspect overall Elo, games played, map performance and head-to-head results.
- Treat a bot with very few games as low-confidence even if its Elo is high.
- Avoid optimizing only against one opponent; preserve diverse opponents and maps.
- Prefer changes with a clear hypothesis that can be tested.
- Log accepted/rejected experiments in `Knowledge/experiment_log.md`.
- Update `Knowledge/Strategy.md` when a reusable strategic insight is established.

## Replays
Replays belong in `replays/` for local visual inspection but are not committed to Git. Do not add replay files to commits.

## Safety / contest actions
Do not submit a bot to the contest automatically unless the user explicitly asks for submission. Local build/run/evaluation is expected; network submission is not.

## Game constraints
The current toolkit docs say C/C++ are judged as C++20/C17 with `-O2` and judge-specific WebAssembly constraints. Use the local `--sandbox` option for periodic judge-like validation.

## Autonomous experimentation
A useful agent loop is:
1. Inspect rules and historical experiments.
2. Form one concrete hypothesis.
3. Create a new bot version.
4. Run the new-version ladder.
5. Inspect Elo, map and head-to-head results.
6. Keep/reject the change based on evidence.
7. Record the experiment and create the next hypothesis.

Do not make hundreds of random edits with no logged hypothesis.
