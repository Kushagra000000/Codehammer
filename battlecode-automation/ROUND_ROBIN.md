# Current-bot round robin

From the repository root:

```sh
.battlecode-codex/bin/python battlecode-automation/scripts/round_robin.py
```

The script discovers every immediate bot project (`*/bot.toml`) in
`bots/New/current` and all `.map` files recursively under `maps`. Each unordered
pair plays once per map, using seed 1. There are no self matches or reverse
fixtures. Bot names are sorted to select sides; side order can affect results.
This is a single-leg comparison, not a side-balanced tournament.

Matches run sequentially in the judge sandbox. There is **no match wall-time
limit** and no runner timeout; normal engine round limits and judge per-turn
CPU/memory/time limits still apply. Ctrl-C stops the current worker process group
and preserves completed results. Sources are copied into temporary directories
with content fingerprints to avoid stale header builds. Original bots are untouched.

Preview the schedule without playing or creating outputs:

```sh
.battlecode-codex/bin/python battlecode-automation/scripts/round_robin.py --dry-run
```

Optional flags: `--seeds 1 2 3`, `--bots-dir PATH`, `--maps-dir PATH`, and
`--out PATH`. Multiple seeds intentionally add one game per pair/map **per seed**;
no reverse fixture is added. Output directories must be new, preventing accidental
overwrite or mixing old results. There is no resume mode.

## Engine version

The runner uses the Python environment used to launch it. It checks PyPI before
starting and refuses to silently run an outdated engine. The inspected engine
adapter currently supports **unswbc 1.2.9**. If a new release appears, upgrade the
environment and review the adapter before changing `SUPPORTED`; do not just bypass
its version guard. `--offline` explicitly skips only the PyPI check. The network
check has a 15-second timeout; matches do not. No automatic upgrade, authentication,
submission or bot-helper update is performed.

## Results

Default destination: `battlecode-automation/experiments/round-robin-TIMESTAMP/`.
Open **report.html** in a browser; it is fully offline with no dependencies.
It contains a win-rate bar chart, standings, head-to-head tables, per-map tables,
queen survival, mean final queen length, mean rounds, side counts, ending reasons,
and a match ledger with links to logs/replays.

- Win rate = wins / valid games. Draws are included in the denominator.
- Score rate = (wins + 0.5 × draws) / valid games. Standings sort by score rate.
- Worker/build/engine failures and detected bot runtime errors are excluded from
  W/D/L, reported separately, and retain any available engine verdict for diagnosis.
- `matches.jsonl` includes authoritative final queen, longest-dragon and team
  lengths, remaining dragons, death reason codes, notices, runtime errors and
  CPU-point mean/p99/max. These are not inferred from winner strings.
- `standings.csv`, `matches.csv`, `summary.json`, and `manifest.json` provide
  spreadsheet data, grouped statistics, source/map hashes, engine version and schedule.
- Reports update after each match and on interruption. A partial report states
  attempted versus scheduled matches; missing games are never counted as draws.

Death codes follow the installed engine: W wall, S self, O other dragon,
H head collision, A no valid action. No action failure is treated as a normal loss.
Run regression tests with:

```sh
.battlecode-codex/bin/python -m unittest discover -s battlecode-automation/scripts -p test_round_robin.py
```
