# Current Project State

## Accepted update — 2026-09-26, after 400 research games

BEST_DISCOVERED_BOT / CURRENT_CHAMPION: `bots/current/bot-v9`.
Parent: v8 via parameter-equivalent EXP001. Exact tested source: EXP012.
The only accepted strategy change is portal-aware remembered-map BFS for pearl
objectives. All project files are byte-identical to the tested candidate.

Independent v5 validation: 82 wins / 1 draw / 5 losses on 11 maps, four seeds,
both sides; 10 maps pass the preregistered Wilson gate. v8 comparison: 11–3
across screening and holdouts; v4 holdouts: 4–2. Four sandbox checks passed for
v9, maximum reported 40.6M points versus 100M limit. Trauma remains weak.

Frozen v5 and previous v8 remain unchanged. Latest experimental candidate is
v9-exp015 (rejected interaction), not the champion. State/certificate, full
results and report: `battlecode-automation/experiments/research-20260926/`.
The initial match budget is exhausted; no plateau or optimality is claimed.
Working changes are preserved by source manifests and tar snapshots, not a new
Git commit. No submission was made. The older state below is historical.

Last updated: 2026-09-26

## Active bot

The currently active/baseline development bot is:

bots/current/bot-v8

Do NOT assume the highest numeric version is the current champion.

## Archived bots

bots/archive/bot-v8.2 exists but is archived intentionally.

It should be analysed as prior work but is NOT automatically considered better than v8.

## Frozen baseline

bot-v5 is a permanent frozen comparison baseline.

Never modify:

bots/archive/bot-v5

## Next version

Determine the next candidate based on repository history and experiments.

A new accepted major candidate would normally be named bot-v9.

Experimental versions may use names such as:

bot-v9-exp001
bot-v9-exp002

until promotion.

## Submission policy

Never call:

unswbc submit

unless the user explicitly requests an official submission.

## Current automation

Local match automation is under:

battlecode-automation/

The winner parser has been updated to recognise output such as:

team A wins after 39 rounds (by elimination)

and:

team B wins after ...

Existing match data may contain historical repaired results.

## Latest feature development — 2026-09-26

Best new prototype: `bots/candidate/bot-v10.1` (source hash starts `800e07868144`).
Stable comparison slot: `bots/candidate/bot-v10` (hash starts `528c355b5cc8`).
Final fresh all-map native results: 17–5 versus v8, 13–9 versus v9. Final
sandbox checks pass, maximum 63.8M points. CURRENT_CHAMPION remains v9: the v5
promotion gate was outside the newly requested four-bot pool, and superiority
over v9 is not statistically established. Active broad search is retired;
`v10_matches.py` performs bounded, explicit comparisons only. Dashboard:
`battlecode-automation/reports/v10.html`. No background search or submission.
