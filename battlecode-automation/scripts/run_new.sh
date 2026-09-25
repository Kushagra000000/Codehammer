#!/bin/bash
set -euo pipefail

# This script lives in <repo>/battlecode-automation/scripts.
# Always run the Python runner from the repository root.
cd "$(dirname "$0")/../.."
python3 battlecode-automation/scripts/autorun_matches.py --mode new "$@"
