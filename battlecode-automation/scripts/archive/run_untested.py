#!/usr/bin/env python3

"""
Run only previously-untested stable Battlecode bots against the established
bot pool.

Designed to sit alongside:

    battlecode-automation/scripts/autorun_matches.py

Behaviour
---------
- Discovers bots only from:
      bots/archive/
      bots/current/

- Completely ignores:
      bots/candidate/

- Excludes:
      bot-v1
      bot-v2

- Treats A vs B and B vs A as the SAME matchup.
  This is intended for symmetric maps.

- Automatically identifies bots with zero parsed historical matches.

- Each new bot is tested against every eligible stable bot.

- If multiple new bots exist, their mutual matchup is only scheduled once.

- Existing historical matches and Elo data are preserved.

Examples
--------

Automatically test all completely untested stable bots:

    python3 battlecode-automation/scripts/run_untested.py --workers 2

See what would run without executing:

    python3 battlecode-automation/scripts/run_untested.py --dry-run

Explicitly test only v10.1 and v10.2:

    python3 battlecode-automation/scripts/run_untested.py \
        --bots bot-v10.1 bot-v10.2 \
        --workers 2
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
AUTOMATION_ROOT = SCRIPT_DIR.parent
PROJECT_ROOT = AUTOMATION_ROOT.parent

BOTS_ROOT = PROJECT_ROOT / "bots"
ARCHIVE_ROOT = BOTS_ROOT / "archive"
CURRENT_ROOT = BOTS_ROOT / "current"

MATCHES_FILE = AUTOMATION_ROOT / "data" / "matches.jsonl"
RATINGS_FILE = AUTOMATION_ROOT / "data" / "ratings.json"

AUTORUN = SCRIPT_DIR / "autorun_matches.py"
UPDATE_DASHBOARD = SCRIPT_DIR / "update_dashboard.py"


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

EXCLUDED_BOTS = {
    "bot-v1",
    "bot-v2",
}

# Defensive exclusion in case an experiment ever accidentally gets copied
# outside bots/candidate/.
EXPERIMENT_PATTERN = re.compile(
    r".*-exp\d+.*",
    re.IGNORECASE,
)


# ---------------------------------------------------------------------------
# Bot discovery
# ---------------------------------------------------------------------------

def discover_bots() -> dict[str, Path]:
    """
    Return:
        {
            "bot-v5": Path(...),
            "bot-v9": Path(...),
            ...
        }

    Only archive/ and current/ are searched.
    """

    bots: dict[str, Path] = {}

    for root in (ARCHIVE_ROOT, CURRENT_ROOT):
        if not root.exists():
            continue

        for directory in sorted(root.iterdir()):
            if not directory.is_dir():
                continue

            name = directory.name

            # Must actually be a Battlecode bot.
            if not (directory / "bot.toml").exists():
                continue

            if name in EXCLUDED_BOTS:
                continue

            if EXPERIMENT_PATTERN.fullmatch(name):
                continue

            if name in bots:
                print(
                    f"ERROR: duplicate bot name '{name}' found at:\n"
                    f"  {bots[name]}\n"
                    f"  {directory}",
                    file=sys.stderr,
                )
                sys.exit(1)

            bots[name] = directory

    return bots


# ---------------------------------------------------------------------------
# Historical match parsing
# ---------------------------------------------------------------------------

def row_is_parsed(row: dict) -> bool:
    """
    Try to recognise successful historical rows without depending too tightly
    on one exact version of autorun_matches.py.
    """

    # Most likely representation.
    winner = row.get("winner")

    if winner not in (None, "", "unparsed"):
        return True

    # Some automation versions may use a result/status field.
    for key in ("result", "status", "outcome"):
        value = row.get(key)

        if not isinstance(value, str):
            continue

        value_lower = value.lower()

        if value_lower in {
            "draw",
            "tie",
            "bot_a",
            "bot_b",
            "team_a",
            "team_b",
            "a",
            "b",
        }:
            return True

        if value_lower.startswith("winner="):
            return True

    return False


def names_from_row(row: dict) -> set[str]:
    """
    Extract bot names from historical rows.

    Handles several reasonable field names so this wrapper survives changes
    to autorun_matches.py.
    """

    names: set[str] = set()

    possible_keys = (
        "bot_a",
        "bot_b",
        "bot1",
        "bot2",
        "bot_1",
        "bot_2",
        "team_a",
        "team_b",
        "a",
        "b",
    )

    for key in possible_keys:
        value = row.get(key)

        if isinstance(value, str) and value.startswith("bot-v"):
            names.add(value)

    # Winner itself may contain a bot name.
    winner = row.get("winner")

    if isinstance(winner, str) and winner.startswith("bot-v"):
        names.add(winner)

    return names


def parsed_game_counts(bot_names: set[str]) -> dict[str, int]:
    """
    Count parsed historical games for each currently eligible bot.
    """

    counts = {name: 0 for name in bot_names}

    if not MATCHES_FILE.exists():
        return counts

    with MATCHES_FILE.open("r", encoding="utf-8") as file:
        for line_no, line in enumerate(file, start=1):
            line = line.strip()

            if not line:
                continue

            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                print(
                    f"WARNING: could not parse matches.jsonl line {line_no}; "
                    "ignoring it."
                )
                continue

            if not isinstance(row, dict):
                continue

            if not row_is_parsed(row):
                continue

            involved = names_from_row(row)

            for name in involved:
                if name in counts:
                    counts[name] += 1

    return counts


# ---------------------------------------------------------------------------
# Match scheduling
# ---------------------------------------------------------------------------

def canonical_pair(a: str, b: str) -> tuple[str, str]:
    """
    A/B and B/A become the exact same identifier.
    """

    return tuple(sorted((a, b)))


def build_pairs(
    targets: list[str],
    all_bots: list[str],
) -> list[tuple[str, str]]:
    """
    Each target fights every eligible stable bot.

    Duplicate reverse matchups are removed.
    """

    pairs: list[tuple[str, str]] = []
    seen: set[tuple[str, str]] = set()

    for target in targets:
        for opponent in all_bots:
            if target == opponent:
                continue

            key = canonical_pair(target, opponent)

            if key in seen:
                continue

            seen.add(key)

            # Keep target first where possible simply to make logs easier to
            # read. It does NOT mean the reverse match will also run.
            pairs.append((target, opponent))

    return pairs


# ---------------------------------------------------------------------------
# Running autorun_matches.py
# ---------------------------------------------------------------------------

def run_pair(
    bot_a: str,
    bot_b: str,
    workers: int,
) -> int:

    command = [
        sys.executable,
        str(AUTORUN),
        "--mode",
        "all",
        "--bots",
        bot_a,
        bot_b,
        "--workers",
        str(workers),

        # Critical:
        # only one orientation of A/B.
        "--one-side",
    ]

    print()
    print("=" * 72)
    print(f"Running: {bot_a} vs {bot_b}")
    print("=" * 72)
    print(" ".join(command))
    print()

    result = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
    )

    return result.returncode


# ---------------------------------------------------------------------------
# Final validation
# ---------------------------------------------------------------------------

def verify_all_bots_now_have_games(
    eligible_names: set[str],
) -> bool:

    counts = parsed_game_counts(eligible_names)

    missing = [
        name
        for name, count in counts.items()
        if count == 0
    ]

    print()
    print("=== Parsed game coverage ===")

    for name in sorted(
        counts,
        key=version_sort_key,
    ):
        print(f"{name:<18} {counts[name]:>5} parsed games")

    if missing:
        print()
        print("WARNING: These eligible bots still have zero parsed games:")

        for name in missing:
            print(f"  - {name}")

        print()
        print(
            "Elo/dashboard refresh will NOT be run because the stable "
            "bot pool is incomplete."
        )

        return False

    print()
    print("All eligible stable bots have parsed match history.")

    return True


# ---------------------------------------------------------------------------
# Version sorting
# ---------------------------------------------------------------------------

def version_sort_key(name: str):
    """
    Sort:
        v3
        v5
        v5.1
        v8.2
        v9
        v10
        v10.1
        v10.2
    """

    match = re.search(r"v(\d+(?:\.\d+)*)", name)

    if not match:
        return (999999,)

    return tuple(
        int(part)
        for part in match.group(1).split(".")
    )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:

    parser = argparse.ArgumentParser(
        description=(
            "Run only untested stable Battlecode bots against the existing "
            "stable bot pool without reverse duplicate matches."
        )
    )

    parser.add_argument(
        "--workers",
        type=int,
        default=2,
        help="Parallel workers passed to autorun_matches.py (default: 2).",
    )

    parser.add_argument(
        "--bots",
        nargs="+",
        help=(
            "Explicit bots to test. Example: "
            "--bots bot-v10.1 bot-v10.2. "
            "If omitted, bots with zero parsed historical games are selected."
        ),
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print intended matchups without executing them.",
    )

    args = parser.parse_args()

    if args.workers < 1:
        parser.error("--workers must be at least 1")

    if not AUTORUN.exists():
        print(
            f"ERROR: cannot find:\n{AUTORUN}",
            file=sys.stderr,
        )
        return 1

    bots = discover_bots()

    if len(bots) < 2:
        print(
            "ERROR: fewer than two eligible bots discovered.",
            file=sys.stderr,
        )
        return 1

    all_names = sorted(
        bots.keys(),
        key=version_sort_key,
    )

    print("=== Eligible stable bot pool ===")

    for name in all_names:
        relative_path = bots[name].relative_to(PROJECT_ROOT)
        print(f"  {name:<18} {relative_path}")

    print()
    print(f"Total eligible bots: {len(all_names)}")

    # ---------------------------------------------------------------------
    # Determine targets
    # ---------------------------------------------------------------------

    game_counts = parsed_game_counts(set(all_names))

    if args.bots:

        unknown = [
            name
            for name in args.bots
            if name not in bots
        ]

        if unknown:
            print()
            print("ERROR: requested bot(s) are not in the eligible pool:")

            for name in unknown:
                print(f"  - {name}")

            return 1

        targets = list(dict.fromkeys(args.bots))

    else:

        targets = [
            name
            for name in all_names
            if game_counts[name] == 0
        ]

    print()
    print("=== Historical parsed games ===")

    for name in all_names:
        marker = " <-- NEW / UNTESTED" if game_counts[name] == 0 else ""

        print(
            f"{name:<18} "
            f"{game_counts[name]:>5} games"
            f"{marker}"
        )

    if not targets:
        print()
        print(
            "No completely untested stable bots were found."
        )
        print(
            "Nothing needs to be run."
        )
        return 0

    print()
    print("=== Bots selected for evaluation ===")

    for name in targets:
        print(f"  - {name}")

    # ---------------------------------------------------------------------
    # Build unordered matchups
    # ---------------------------------------------------------------------

    pairs = build_pairs(
        targets,
        all_names,
    )

    print()
    print("=== Matchups ===")

    for i, (a, b) in enumerate(pairs, start=1):
        print(
            f"{i:>3}. {a} vs {b}"
        )

    print()
    print(
        f"{len(pairs)} bot pairings × all available maps"
    )

    print()
    print(
        "Reverse duplicates are disabled: "
        "A vs B and B vs A are treated as one matchup."
    )

    if args.dry_run:
        print()
        print("Dry run complete. No matches were executed.")
        return 0

    # ---------------------------------------------------------------------
    # Execute
    # ---------------------------------------------------------------------

    failed_pairs: list[tuple[str, str]] = []

    for index, (bot_a, bot_b) in enumerate(
        pairs,
        start=1,
    ):

        print()
        print(
            f"PAIR {index}/{len(pairs)}"
        )

        return_code = run_pair(
            bot_a,
            bot_b,
            args.workers,
        )

        if return_code != 0:
            failed_pairs.append((bot_a, bot_b))

            print(
                f"WARNING: autorun returned {return_code} for "
                f"{bot_a} vs {bot_b}"
            )

    # ---------------------------------------------------------------------
    # Verify everyone is represented before displaying Elo
    # ---------------------------------------------------------------------

    complete = verify_all_bots_now_have_games(
        set(all_names)
    )

    if failed_pairs:
        print()
        print("=== Failed pair invocations ===")

        for a, b in failed_pairs:
            print(f"  - {a} vs {b}")

    if not complete:
        return 2

    # ---------------------------------------------------------------------
    # Dashboard
    # ---------------------------------------------------------------------

    if UPDATE_DASHBOARD.exists():

        print()
        print("Refreshing Elo/dashboard...")

        subprocess.run(
            [
                sys.executable,
                str(UPDATE_DASHBOARD),
            ],
            cwd=PROJECT_ROOT,
            check=False,
        )

    print()
    print("=" * 72)
    print("Evaluation complete.")
    print("=" * 72)

    print()
    print(
        f"Ratings:   {RATINGS_FILE.relative_to(PROJECT_ROOT)}"
    )

    print(
        "Dashboard: "
        "battlecode-automation/reports/ladder.html"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())