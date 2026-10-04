# Champion

**Status: champion confirmed by the human, results not yet re-baselined.** The rules changed on 2026-10-02 (queen-first tiebreak, see
`knowledge/game_rules.md`). Every rating and result from before that date was
produced under the old rules and is archived, not trusted.

| Field | Value |
| --- | --- |
| Champion (human-confirmed strongest, 2026-10-03) | `bots/Old/current/bot-v10.3.3` |
| Fingerprint | not yet recorded (record it when re-baselined) |
| Engine check | builds and runs on `unswbc` 1.2.9 (2026-10-03 smoke test: beat alan-v2.1 on arena, seed 1, 82 rounds; one game, not a rating) |
| Last re-baseline | none yet |

Other bots in the panel (unverified under the new rules): `bots/Old/alan/alan-v2.1`,
`bots/Old/alan/alan-v2.4`, `bots/Old/Kushagra/bot-v12.2-Kushagra`, `bots/Old/archive/bot-v11.1`.

## Rules for this file
- Only the human (or the lead, when the human says so) edits this file.
- To promote a candidate: it must pass the gate against the panel in
  `battlecode-automation/panel.json` on engine 1.2.9, be reviewed by a second
  model, pass a `--sandbox` check, then be recorded here with its commit hash and
  fingerprint, and tagged in git (for example `champion-2026-10-06`).
- Before any promotion, the first job is to **re-baseline**: round-robin the panel
  on 1.2.9 with the full map set and write the result in `knowledge/experiment_log.md`.
