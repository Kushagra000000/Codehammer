# Codehammer

[![Language: C++](https://img.shields.io/badge/language-C%2B%2B-00599C)](#install)
[![Engine: unswbc 1.2.9](https://img.shields.io/badge/engine-unswbc%201.2.9-informational)](requirements.txt)
[![Current bot: v40.1.0](https://img.shields.io/badge/current%20bot-v40.1.0-success)](bots/New/current/bot-v40.1.0)

> C++ bots and testing tools from Team Codehammer for UNSW Battlecode 2026 ("dragons, pearls and portals").

Read [Background](#background) to learn the game, follow [Install](#install) and
[Usage](#usage) to play your first match, then see
[About the Bot](#about-the-bot) for what our bot does.

## Table of Contents

- [Background](#background)
  - [UNSW Battlecode](#unsw-battlecode)
  - [The game in one minute](#the-game-in-one-minute)
  - [How our bot came to be](#how-our-bot-came-to-be)
- [Install](#install)
- [Usage](#usage)
  - [Play one game](#play-one-game)
  - [Test a bot on every map](#test-a-bot-on-every-map)
  - [Submitting](#submitting)
- [About the Bot](#about-the-bot)
- [How the Bot Works](#how-the-bot-works)
- [Repository Layout](#repository-layout)
- [Maintainers](#maintainers)
- [Thanks](#thanks)
- [Contributing](#contributing)
  - [Getting help](#getting-help)

## Background

### UNSW Battlecode

UNSW Battlecode is a programming competition. Each team writes a bot, and the bots play a
strategy game against each other with no human input once a game starts. In 2026 the game was
"dragons, pearls and portals"; the qualifier ran on 10 October and the Grand Final on
17 October 2026.

Teams submit source code rather than programs. The judge compiles C++ bots with clang 20 and
libc++ to WebAssembly and gives every dragon a fixed computing budget each turn, so a bot has
to play well and think fast.

### A brief description of the game

- Two teams play on a rectangular grid that wraps around at the edges. Some tile edges are
  blocked by kelp, and portals link pairs of edges that are far apart.
- A team is comprised of dragons: snakes made of segments. Each team has one queen; every other
  dragon is a worker, created when a dragon splits in two.
- Dragons grow by eating pearls, which reappear on their tiles on a map-dependent timer.
- A dragon dies if its head runs into kelp or into any dragon's body, and two heads that meet
  both die. A dead dragon leaves pearls behind.
- Every dragon gets one free step per turn for each four segments of its length (rounded up).
  Steps beyond the free ones are allowed but cost a segment each.
- Each dragon is controlled by its own copy of the bot. It sees only the 7×7 square around its
  head and can talk to teammates only through short sonar signals.
- A game lasts 500 rounds. A team wins by eliminating the other team; otherwise the team with
  the longer queen wins, then the team with the longest dragon, then the team with the greater
  total length.

### Our bot's development process

Over the season, team members built several bot lines in parallel. Version 40
merged our two strongest, Omar's v31 and Leo's v30.6, then added the behaviours described in [About the Bot](#about-the-bot). Each new
version was tested against the earlier ones over hundreds of local games before we adopted it.

## Install

You need:

- **macOS or Linux** (on Windows, use WSL). The test scripts are written for bash.
- **Python 3 and pip**, to install the game engine.
- **A C++ compiler.** We recommend clang because it is what the judge uses.

Then clone the repository and install the engine in a virtual environment:

```sh
git clone https://github.com/OWNER/Codehammer.git
cd Codehammer
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Keep that exact version: older releases, such as 1.1.0, do not implement the rule changes of
2 October 2026, so they play different games.

To check the install, run `unswbc --help`; it should list the engine's commands.

## Usage

### Play one game

```sh
unswbc run maps/default.map bots/New/current/bot-v40.1.0 bots/New/current/bot-v31.0.0-omar
```

The first bot plays as team A and the second as team B. The engine builds both bots, plays the
game and ends with the result, for example:

```text
team A wins after 499 rounds (longer queen, 23 to 9)
```

The reason in brackets tells you how the game was decided: by elimination, by the longer queen,
by the longest dragon or by total length.

Some useful options:

| Option | What it does |
| --- | --- |
| `--seed N` | Fixes the random seed. With the same map, sides and seed, a game replays exactly. |
| `--sandbox` | Runs the bots in the engine's sandbox, closer to how the judge runs them. Use it before submitting. |
| `-v` | Prints a detailed log, including deaths and, with `--sandbox`, each bot's computing cost per turn. |
| `-o DIR` | Sets the directory for the game's output files, such as the replay. |
| `--no-replay` | Skips saving the replay, for a large number of runs. |

### Test a bot on every map

`run_all.sh` plays
every map in `maps/` several times and summarises the results:

```sh
./run_all.sh -j 8 4 bots/New/current/bot-v40.1.0 bots/New/current/bot-v31.0.0-omar
```

This plays each map 4 times, 8 games at a time, with the first bot as team A. It prints every
result as it finishes, then a table of how each side lost. The end of the output looks like
this (your numbers will differ):

```text
A = bots/New/current/bot-v40.1.0
B = bots/New/current/bot-v31.0.0-omar
final A - B: 52 - 44, draws 4, over 100 games

lost by                           A      B
elimination                      18     21
queen (its queen dead)            6      9
queen (its queen shorter)         2      1
longest dragon                   18     21
total length                      0      0
total losses                     44     52
draws: 1 with both teams eliminated, 3 at the round limit with equal lengths
```

For deeper analysis (paired seeds, both sides, replay statistics), see the scripts listed in
[`battlecode-automation/README.md`](battlecode-automation/README.md).

### Submitting

Submissions are made by the team member who holds the contest key (see [Security](#security)).
To propose a bot for submission, follow [Contributing](#contributing).

## About the Bot

The bot we maintain is **bot-v40.1.0**, in `bots/New/current/`. It plays without hardcoded
knowledge of any particular map; every dragon learns the map as it goes. A brief overview
of our strategy is as follows:

- **Grow the team early:** Dragons eat nearby pearls, split into workers to cover more of the
  map, and head into unexplored area when the pearls they know about run out. On smaller maps,
  short workers also go through portals early to find food on the other side.
- **Farm narrow lanes:** When a dragon finds a one-tile-wide dead end with pearls in it, it
  splits on the way out and leaves one half behind to keep farming.
- **Protect the queen:** Workers that can see the queen stay out of her path, and a worker that
  has boxed her in sacrifices itself to free the way. The queen never enters a portal unless it
  is her only safe move.
- **Fight when advantageous:** Workers ram enemies when the trade favours us, always ram the enemy queen, and short workers gang up on enemies where we outnumber them.
- **Endgame:** From round 250 the queen becomes the team's focus. Workers pass on where
  they saw the enemy queen, hunt her, and feed their own length to our queen, because a game
  that reaches round 500 is decided first by queen length.

v40.1.0 is our strongest bot. In our final tests (200 games against each
opponent across all maps), it won 52–60% of games against each of our previous best bots and
the strongest rival bot we had. We think v40.1.0 is a good place to start modifying.

## How the Bot Works

Every turn, each dragon's copy of the bot goes through the same five steps:

1. **Perceive:** Read what the dragon can see this turn and any sonar messages from teammates.
2. **Remember:** Add what it saw to its own map memory; kelp, portals and when each pearl will
   next appear.
3. **Check for danger and chances:** Work out which enemies could reach it and whether it can
   ram anyone safely.
4. **Decide on a move:** Score each direction for food, safety, room to keep moving and the team
   rules above. When nothing worth eating is in view, plan a route to remembered pearls or
   unexplored water.
5. **Perform move:** Send the move (or a split) to the engine, plus a sonar message for teammates.

The numbers behind these choices, such as how much risk to accept or when the late game starts,
are named constants in `tuning.hpp` in each bot folder, so most experiments start there. The
changelog in `bots/New/current/changelog/` explains what each version changed and why.

## Repository Layout

```text
Codehammer/
├── bots/
│   ├── New/
│   │   ├── current/          # bots in use: our latest versions and the rival bots we test against
│   │   │   └── changelog/    # one part per version: what changed and how it tested
│   │   └── archive/          # retired versions
│   ├── Old/                  # frozen history: early bot lines (do not edit)
│   └── README.md
├── battlecode-automation/    # scripts for evaluating bots and analysing games (see its README)
├── maps/                     # the competition's .map files
├── requirements.txt          # pins the engine version
├── run_all.sh                # plays every map N times between two bots, in parallel
└── run_battles.sh            # quick loop over all maps (edit the bot paths inside first)
```

Every bot folder holds a `bot.toml` file (created by `unswbc init`) and the bot's C++ source.

## Maintainers

Team Codehammer:

| Name | Main work |
| --- | --- |
| Omar | v27, v31 and v40 bot lines; this README |
| Leo | v30 bot line |
| Kushagra | v12 bot line |
| Alan | alan bot line |
| Mehak | Mehak bot line |

## Thanks

The UNSW Battlecode organisers, for the game, the engine and the judge.

## Contributing

Changes from teammates and future contributors are welcome.

1. **Start from the current bot:** Copy `bots/New/current/bot-v40.1.0` to a new folder with the
   next version number, such as `bot-v40.2.0` (add `-yourname`).
   Never edit a bot in place, and never change anything in `bots/Old/` or
   `battlecode-automation/panel.json`.
2. **Change one idea at a time:** So that testing can help determine whether it helped.
3. **Test it:** Run `run_all.sh` against the current bot in both side orders, and against the
   other bots in `bots/New/current/`. Keep the change only if it wins more than half its games
   against the current bot and does not lose more often to any other opponent or in any loss
   category.
4. **Update the changelog:** Add a part for your version to the changelog in `bots/New/current/changelog/`.
5. **Open a pull request:** Describing the change and your test results. A contributor will review it.

### Getting help

- **Questions about this code, or a bug:** open an issue in this repository or reach out to one of the
  [maintainers](#maintainers).
- **Questions about the game rules:** see the rules documentation from the UNSW Battlecode
  organisers. 