#!/usr/bin/env python3
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
import autorun_matches as ladder  # type: ignore

cases = [
    ("team A wins after 39 rounds (by elimination) (0.4s)", "", "bot_a"),
    ("team B wins after 77 rounds (by elimination) (0.8s)", "", "bot_b"),
    ("game ended in a draw after 500 rounds", "", "draw"),
]

for stdout, stderr, expected in cases:
    got = ladder.parse_winner(stdout, stderr, {"bot_a": None, "bot_b": None})
    assert got == expected, (stdout, got, expected)
    print(f"PASS: {expected}")

print("All parser tests passed.")
