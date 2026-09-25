#!/usr/bin/env python3
"""Recompute Elo and regenerate reports/ladder.html without running matches."""
from pathlib import Path
import importlib.util
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import autorun_matches as ladder

bots = ladder.discover_bots()
rows = ladder.read_jsonl(ladder.MATCHES_PATH)
config = ladder.load_config()
ladder.update_state_and_dashboard(bots, rows, config)
print(f"Refreshed {len(bots)} bots from {len(rows)} stored match records.")
