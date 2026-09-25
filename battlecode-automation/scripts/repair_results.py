#!/usr/bin/env python3
"""Repair historical Battlecode match rows using stored stdout/stderr tails.

This avoids replaying matches when the match itself succeeded but the old parser
failed to recognise the official CLI result sentence.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
import autorun_matches as ladder  # type: ignore


def parse_result(row: dict) -> tuple[str, str | None, str | None]:
    winner = ladder.parse_winner(
        row.get("stdout_tail", ""),
        row.get("stderr_tail", ""),
        {"bot_a": None, "bot_b": None},
    )
    if winner == "bot_a":
        return "win", row.get("bot_a_uid"), row.get("bot_a")
    if winner == "bot_b":
        return "win", row.get("bot_b_uid"), row.get("bot_b")
    if winner == "draw":
        return "draw", None, None
    return row.get("result", "unparsed"), row.get("winner_uid"), row.get("winner_label")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", help="Write repaired matches.jsonl")
    args = ap.parse_args()

    path = ladder.MATCHES_PATH
    rows = ladder.read_jsonl(path)
    if not rows:
        print(f"No match history found at {path}")
        return 0

    repaired = 0
    remaining = 0
    for row in rows:
        if row.get("result") != "unparsed":
            continue
        result, winner_uid, winner_label = parse_result(row)
        if result in {"win", "draw"}:
            row["result"] = result
            row["winner_uid"] = winner_uid
            row["winner_label"] = winner_label
            repaired += 1
        else:
            remaining += 1

    print(f"Match rows: {len(rows)}")
    print(f"Repaired:   {repaired}")
    print(f"Remaining unparsed: {remaining}")

    if not args.apply:
        print("Dry run only. Re-run with --apply to write the repaired results.")
        return 0

    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")
    tmp.replace(path)

    print(f"Updated {path}")
    print("Now run: python3 battlecode-automation/scripts/update_dashboard.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
