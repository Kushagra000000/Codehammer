#!/usr/bin/env python3
from pathlib import Path
import shutil
import sys

SCRIPT = Path(__file__).resolve()
REPO = SCRIPT.parents[2]

checks = {
    "repository root": REPO,
    "bots directory": REPO / "bots",
    "maps directory": REPO / "maps",
    "automation directory": REPO / "battlecode-automation",
    "automation config": REPO / "battlecode-automation/scripts/automation_config.json",
}

ok = True
for label, path in checks.items():
    exists = path.exists()
    print(f"{'OK  ' if exists else 'FAIL'} {label}: {path}")
    ok &= exists

unswbc = shutil.which("unswbc")
print(f"{'OK  ' if unswbc else 'FAIL'} unswbc: {unswbc or 'not found'}")
ok &= bool(unswbc)

bots = list((REPO / "bots").glob("**/bot.toml")) if (REPO / "bots").exists() else []
maps = list((REPO / "maps").glob("**/*.map")) if (REPO / "maps").exists() else []
print(f"Found {len(bots)} bot projects")
print(f"Found {len(maps)} maps")

if len(bots) < 2:
    print("FAIL: need at least 2 bot projects")
    ok = False
if not maps:
    print("FAIL: need at least 1 map")
    ok = False

sys.exit(0 if ok else 1)
