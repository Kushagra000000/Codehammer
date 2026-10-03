# UNSW Battlecode automated ladder

This folder contains a drop-in automation layer for the Battlecode repository. It does not modify your bot source code.

## Target project layout

The scripts expect your existing layout, including:

```text
Knowledge/
  Strategy.md
  experiment_log.md
  game_rules.md
bots/
  archive/
    bot-v1/
    bot-v2/
    ...
  current/
    bot-v8.2/
  # other bot folders are also discovered automatically
maps/
  *.map
replays/
README.md
AGENTS.md
scripts/
data/
reports/
```

Every bot project is discovered by finding a `bot.toml` under `bots/`, so later you may add `bots/candidates/`, `bots/experiments/`, etc. without changing the runner.

Every `.map` under `maps/` is discovered automatically. Adding a new map therefore automatically adds it to the next tournament.

## Install prerequisites

You already have `unswbc`. The scripts use Python's standard library only; no extra Python package is required.

Check:

```bash
unswbc
python3 --version
```

## 1. See what the runner finds

From the repository root:

```bash
python3 scripts/autorun_matches.py --list
```

You should see every bot and every `.map`.

## 2. First ladder / bootstrap

Elo needs match history. A compact first pass is:

```bash
python3 scripts/autorun_matches.py --mode bootstrap --workers 2
```

This has the newer versions play a limited opponent set across all discovered maps.

For a complete round-robin from the beginning:

```bash
python3 scripts/autorun_matches.py --mode all --workers 2
```

With N bots, M maps, two side orders and one game per order, a full ladder is roughly:

`N × (N-1) / 2 × M × 2` matches.

That grows quickly, which is why `--mode new` is the normal day-to-day mode.

## 3. Add a new bot and automatically test it

When an AI creates, for example:

```text
bots/current/bot-v8.3/
```

run:

```bash
python3 scripts/autorun_matches.py --mode new --workers 2
```

The runner:

1. fingerprints every bot project;
2. identifies code identities it has not seen before;
3. identifies the current champion from the Elo ladder;
4. schedules each new model against the champion and several other strong/representative models;
5. runs every scheduled pairing on every discovered map;
6. runs both bot orders by default to reduce side-order bias;
7. runs matches in isolated temporary directories so parallel workers have independent build/replay output;
8. moves local `.replay` files into `./replays/`;
9. stores match results in `data/matches.jsonl`;
10. recomputes Elo and generates `reports/ladder.html`.

Because maps are globbed from `maps/**/*.map`, new maps do not require script changes.

## 4. Parallel execution

The runner uses separate `unswbc run` subprocesses through a Python thread pool. The game processes themselves are independent; each match gets its own temporary working directory and its own replay directory.

Start conservatively on your Mac:

```bash
python3 scripts/autorun_matches.py --mode new --workers 2
```

Then try 3 or 4 if your CPU/thermal load is comfortable:

```bash
python3 scripts/autorun_matches.py --mode new --workers 4
```

More workers are not automatically faster because all workers share the same machine CPU, memory and disk.

The isolation also avoids the replay filename collision risk that can occur when multiple matches finish in the same second.

## 5. Judge-like validation

`unswbc run` locally does not impose the same CPU-point accounting as the judge. The toolkit provides `--sandbox` to run locally under the judge-style sandbox.

Use it for serious candidate checks:

```bash
python3 scripts/autorun_matches.py --mode new --sandbox --workers 2
```

The current documentation says the judge runs C/C++ bots in WebAssembly, uses C++20/C17, and measures work in CPU points; local `--sandbox` is intended to mirror that environment. Keep a margin rather than tuning right to the limit. 

## 6. View the ladder

Open the generated file:

```bash
open reports/ladder.html
```

It contains:
- champion and ladder summary;
- ranked Elo table;
- Elo bar chart;
- Elo history for leading bots;
- map performance;
- head-to-head table.

Regenerate without running any matches:

```bash
python3 scripts/update_dashboard.py
```

A CSV snapshot is also available:

```bash
python3 scripts/ladder.csv.py
```

## 7. Run a specific pairing

The normal runner is tournament-oriented. For one quick check, use the native command exactly as documented by Battlecode:

```bash
unswbc run maps/arena.map bots/current/bot-v8.2 bots/archive/bot-v8
```

The automation runner itself uses the same `unswbc run <map> <bot-a> <bot-b>` shape inside its isolated workspace.

## 8. Useful schedule controls

Only selected maps:

```bash
python3 scripts/autorun_matches.py --mode new --maps arena.map devil.map --workers 2
```

Only selected bots:

```bash
python3 scripts/autorun_matches.py --mode all --bots bot-v8 bot-v8.2 --workers 2
```

More opponents for a new candidate:

```bash
python3 scripts/autorun_matches.py --mode new --opponents 8 --workers 2
```

Repeat each side order twice:

```bash
python3 scripts/autorun_matches.py --mode new --games-per-order 2 --workers 2
```

Run only one side order to halve the match count:

```bash
python3 scripts/autorun_matches.py --mode new --one-side --workers 2
```

Preview before spending time:

```bash
python3 scripts/autorun_matches.py --mode new --dry-run
```

## 9. Persistent files

`data/matches.jsonl` is the source-of-truth match ledger. Each completed match has the map, bot identities, outcome, runtime, replay path, and captured stdout/stderr tails.

`data/ratings.json` is the current Elo snapshot.

`data/elo_history.jsonl` is the per-match Elo history used by the graph.

`data/champion.json` identifies the current champion.

`reports/ladder.html` is generated from those files.

## 10. Git behaviour

`replays/` and `*.replay` are ignored. If replay files were already committed before the new `.gitignore` was installed, remove them from Git's index once:

```bash
git rm --cached replays/*.replay
```

Then commit the `.gitignore`.

The generated dashboard is also ignored; regenerate it locally at any time.

## 11. Codex / autonomous-agent workflow

The supplied `AGENTS.md` tells Codex to read the game rules, strategy notes and experiment history before editing code.

The intended autonomous cycle is:

```text
read knowledge
   -> propose hypothesis
   -> create new bot version
   -> run --mode new
   -> inspect Elo/maps/head-to-head
   -> accept or reject
   -> update experiment_log.md / Strategy.md
   -> repeat
```

For this to work well, the human project should follow one important convention: **one meaningful experiment = one new bot version/fingerprint**. Do not repeatedly mutate the only working copy and lose the lineage of what changed.

## 12. Important limitation: winner parsing

The runner intentionally refuses to guess an outcome when `unswbc` exits successfully but its output does not match one of the known winner patterns. Such a record is stored as `result=unparsed` and is not used for Elo.

Run one manual match once on your installed `unswbc 0.3.13` and inspect the terminal output. If its exact result wording is different, add that wording to `parse_winner()` in `scripts/autorun_matches.py`. This prevents silent Elo corruption.

## 13. Recommended configuration

`workers: 2` is a safe starting point.

`both_sides: true` is recommended for fairer comparisons.

`games_per_order: 1` keeps experimentation manageable; raise it for finalists.

`opponents_per_new: 5` is a useful default. Increase it when the ladder has many strong bots.

`champion_min_games: 8` prevents the champion from being selected solely because of one or two lucky games. Raise this threshold as the ladder accumulates history.

For high-stakes final evaluation, run a candidate on all maps, both side orders, with multiple games per order and `--sandbox`.


## Repository layout

The automation directory is intentionally nested:

    <repository>/
      bots/
      maps/
      replays/
      Knowledge/
      battlecode-automation/
        scripts/

The scripts therefore treat the repository root as two levels above
`autorun_matches.py`. Do not move `scripts/` unless you also update
`PROJECT_ROOT` in `scripts/autorun_matches.py`.

## Quick commands

From the repository root:

    ./battlecode-automation/scripts/run_bootstrap.sh --workers 2
    ./battlecode-automation/scripts/run_new.sh --workers 2
    ./battlecode-automation/scripts/run_all.sh --workers 2

You can also run:

    python3 battlecode-automation/scripts/autorun_matches.py --list

The runner discovers bots from:

    bots/**/bot.toml

and maps from:

    maps/**/*.map

Results are stored under `battlecode-automation/data/`.
Replays are stored under the repository-level `replays/` directory.

### Repairing old `unparsed` results

The runner stores each match's stdout/stderr tail in `data/matches.jsonl`. If a match completed but its winner was not parsed, use the repair script instead of replaying the match:

```bash
python3 battlecode-automation/scripts/test_parser.py
python3 battlecode-automation/scripts/repair_results.py
python3 battlecode-automation/scripts/repair_results.py --apply
python3 battlecode-automation/scripts/update_dashboard.py
```

This repairs authoritative `team A wins ...` / `team B wins ...` CLI output already stored in the match rows.
