# Current Project State

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