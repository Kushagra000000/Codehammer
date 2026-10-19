# bot-v30.4.0-leo — changes and evaluation

Based on **bot-v30.0.0-leo** (the version live on the ladder since 8 Oct, rank ~28). v30.0 plays Sabotage-d's logic in
our own code; its decisions are turn-identical to Sabotage-d, so every test "vs Sabotage-d" below is also a test vs
v30.0 (a mirror: v30.0 scores exactly 50%).

- **bot-v30.3.0-leo** = v30.0 + changes 1 and 2 (bug fixes).
- **bot-v30.4.0-leo** = v30.3 + change 3 (the crown on big maps).

Every change is a switch in `tuning.hpp`; with the switch off the bot plays exactly as v30.0. No behaviour is tied to a
particular map: the only map property used is the area (width × height ≥ 1100 tiles), checked on random generated
maps as well as the official ones.

## Changes

| # | Change | Behaviour and limits | Switch / code |
|---|---|---|---|
| 1 | The queen never takes a dead-end harvest run | On maps under 1100 tiles the queen used to follow pearls into a dead end and then split to get out. The split hands the tail half (the part near the exit) to the new dragon; the queen keeps the head half, deep in the pocket, and dies there. Big maps already had this rule. | `QNOPOCKET_RUN = true`; `decide.hpp` (`pocket_run`) |
| 2 | Long dragons stop splitting late, whatever the contact clock says | Under the contact clock's "everything late" setting (`REG_LATE`/`REG_STOP` = 499) only the king was kept from splitting; any other long dragon shed a 2-long bud every turn (`BUD`, past half the unit limit) down to 3. From round 420 a dragon at least half the king's length now keeps its length. | `LATE_KEEP = 420`; `tactics.hpp` (`want_split`) |
| 3 | The crown on big maps (v30.4 only) | On maps of at least 1100 tiles, from round 150 our queen is the king: the king beacon, guards and feeding go to her, so she is the long one at round 500 (the first tiebreak is queen length). If no friend has seen or heard her for 6 rounds (she is dead or cut off), the usual king choice (our longest dragon) resumes. | `QKING_FROM = 150`, `QKING_AREA = 1100`, `QKING_SILENT = 6`; `decide.hpp` (`qk`), `perceive.hpp` (`hear_beacons`, `find_king`), `tactics.hpp` (`try_feed`) |

`QK_KEEP_LONG` (under the crown, our longest worker keeps the king's no-split rule) is in the code but off: it did not
help (below).

### Why

From 443 ladder games played by v30.0 (replays and analysis tools are on Leo's machine, not in the repo):

- **Change 1.** On maps under 1100 tiles our queen died boxed in 53 times (opponents' queens: 15), mostly after
  harvest runs on Tower Defense (24) and Portals (15).
- **Change 2.** Match 1452181 (Australia vs Computers). At round 433 our king (length 23) made an escape split. Its
  21-long tail became a new process that still heard the relayed beacon "king 704, length 23" (704 was now 2 long),
  so it was not the king and budded 21 → 3 in nine turns. We lost the longest-dragon tiebreak 10–19. The same
  budding was reproduced locally on Australia (13 long dragons in 4 games). On the ladder it decided 1 of the 243
  games that reached round 500.
- **Change 3.** On big maps the queens decide the result: both queens alive at r500 26–32, only ours alive 19–0,
  only theirs alive 0–25, both dead 57–20. In 16 of the 32 "both alive" losses our longest dragon was longer than
  their queen, while our own queen (kept near home by the big-map safety rules) was short.

## Results

Judge WASM sandbox (unswbc 1.2.9), vs Sabotage-d, seeds 2701–2708, both sides, unless noted.

| Build | Maps | Games | Score | Notes |
|---|---|---:|---:|---|
| v30.0 (reference) | 25 official | 400 | 200 (50%) | mirror |
| `QNOPOCKET_RUN` only | 25 official | 400 | 203 | flips +3 / −0 |
| `LATE_KEEP` 420 only | 25 official | 400 | 198 | flips +9 / −11 |
| `LATE_KEEP` 380 only | 25 official | 400 | 200 | |
| **v30.3** | 25 official | 400 | **201** | flips +12 / −11; queen alive at end 71 (v30.0: 69) |
| v30.3 changes | 94 random small maps (seeds 1, 2) | 376 | 195 (51.9%) | |
| **v30.4** | 10 official maps ≥ 1100 tiles | 160 | **97 (61%)** | small maps play as v30.3 |
| **v30.4** | 57 random maps ≥ 1100 tiles (seeds 1, 2) | 228 | **118 (52%)** | 1100–1999 tiles 40/80, 2000–2999 52/96, 3000+ 26/52 |

v30.4 on the official big maps, per map (of 16): Trauma 14, UNSW 14, Stronghold 11, Maze 10, Slithery Fight 10,
Help 9, Schooltime 8, Big Empty 8, Islands 7, Australia 6. Outcomes: both queens alive at r500 32–10 (v30.0 19–19),
only ours alive 24–0, only theirs alive 0–17, both dead 39–30, eliminations 2–6.

How the crown settings were chosen (official big maps, 160 games each, all with changes 1 and 2):

| Crown | Score | 64×64 maps | Both queens dead |
|---|---:|---:|---:|
| from r100, hand-back after 30 silent rounds | 87 (54%) | 32/64 | 39–34 |
| from r150, hand-back 30 | 86 (54%) | 27/64 | 28–37 |
| from r250, hand-back 30 | 83 (52%) | 28/64 | 26–35 |
| from r350, hand-back 30 | 83 (52%) | 31/64 | 33–36 |
| **from r150, hand-back 6** | **97 (61%)** | **37/64** | **39–30** |
| from r150, hand-back 30, `QK_KEEP_LONG` | 85 (53%) | 29/64 | 29–31 |
| from r150, hand-back 6, `QK_KEEP_LONG` | 99 (62%) | 37/64 | 40–30 |

With the 30-round hand-back the crown lost on the 64×64 maps: the team kept feeding and guarding a dead or cut-off
queen while the real longest dragon got no king protection. The first crown (hand-back 30) scored 49% on the random
big maps; hand-back 6 scores 52%.

## Limits

- All local tests are against Sabotage-d, which plays like v30.0. They cannot show what changes 1 and 2 are worth
  against the field: those situations are rare in the mirror (self-inflicted queen deaths 72 in both v30.0 and v30.3).
- The 61% on official big maps comes from one seed set; the fresh-seed confirmation was not run. The random-map
  result is the independent check.
- Queen survival on small maps is not addressed. There the queen is one of the team's few early eaters: pricing
  every enemy's reach for her (`QSAFE_SMALL` 3 / 6) kept her alive through the opening but cost 2–3.6 units by
  round 100 and many eliminations (174 and 164 / 400). Work continues (`QG2_W`, a graded guard, off).

## What to check in ladder replays

- Big maps: our queen grows from about round 150 and is the longest at r500; when she dies, a long worker becomes
  king within a few rounds.
- No long dragon (≥ half the king) splits after round 420.
- Tower Defense / Portals: the queen does not enter dead ends for pearls.
