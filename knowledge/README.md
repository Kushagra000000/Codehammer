# Knowledge index

| File | What it is | Who edits it |
| --- | --- | --- |
| `game_rules.md` | Authoritative rules, copied from the official docs, with a **rule change log at the top** | lead or human |
| `Strategy.md` | Reusable lessons, each labelled OBSERVED / HYPOTHESIS / TESTED | lead |
| `experiment_log.md` | One entry per tested bot version, new rules only | the agent that ran it |
| `v15_design.md` | Architecture spec for the new base bot bot-v15.0 (layers and file layout), shared by all agents | lead or human |
| `map_analysis.md` | Per-map facts (size, symmetry, portals, kelp). Needs a rerun for the 7 new maps | lead |
| `opponent_profiles.md` | Notes on other teams and our own older bots | lead |
| `leaderboard.md` | Public ladder observations | human or lead |
| `archive/` | Pre-2026-10-02 audits, prompts and notes. Read-only history | nobody |

Rule of thumb: if two files disagree, `game_rules.md` wins on rules and the newest dated
`experiment_log.md` entry wins on results.
