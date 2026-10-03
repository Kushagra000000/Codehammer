# RULE CHANGE LOG (read this first)

## 2026-10-02: tiebreak now checks the QUEEN first
Sources: https://game.battlecode.au/docs/structure (read 2026-10-03); engine `unswbc` 1.2.9
(`run.py` explains results as "longer queen", "longest dragon", "total length"; the engine
reports each team's queen length).

OLD rule (until 2026-10-01). After 500 rounds without elimination:
1. the team with the longest living dragon wins; 2. else the greatest total length; 3. else draw.

NEW rule (from 2026-10-02). After 500 rounds without elimination:
1. the team with the longer **queen dragon** wins (each team's queen is its dragon with ID 0 or 1: ID 0 for team A, ID 1 for team B, observed 2026-10-03);
2. else the team with the longest living dragon; 3. else the greatest total length; 4. else draw.
A dead queen has length 0, so a team whose queen dies loses to a team whose queen lives.

UNCHANGED (per the docs page): at most 500 rounds; a team is eliminated when all its dragons
die; if one team is eliminated the other wins; if both are eliminated in the same round it is a draw.

OBSERVED 2026-10-03 (unswbc 1.2.9, seed 1, bot-v10.3.3 vs alan-v2.1, maps arena and default, `-v` log):
dragon IDs are handed out in turn order, alternating team A then team B. ID 0 is team A's first
dragon and ID 1 is team B's first dragon, so team A's queen is ID 0 and team B's queen is ID 1. Which
side we get varies, so our queen is whichever of ID 0 or 1 matches our side: a bot is its own queen when
`ct.get_id() <= 1`, and an enemy dragon with ID 0 or 1 is the enemy queen. On default.map each team
starts with 6 dragons (IDs 0 to 11 alternating A, B); only IDs 0 and 1 are queens, IDs 2 to 11 are ordinary.
Dragons created by splitting get new, higher IDs (seen on arena: IDs 2 and 3 appear at round 8).
STILL UNVERIFIED: whether anything besides the tiebreak changed, and the ID rule on maps other than
arena and default (check once on each new map and record it here).

ENGINE: `unswbc` 1.1.0 (the repo's old pin) does NOT implement this. Use 1.2.9 (`requirements.txt`).
The helper protocol is still major version 3 and `get_id()` is available on a dragon.
`unswbc` 1.2.x also ships 22 maps; this repo's `maps/` had 15. New: australia, islands, maze,
stripes, tower_defense, unsw, weakhold. Run `unswbc maps` to add them (it keeps existing files).

STRATEGY IMPLICATIONS (hypotheses, see `knowledge/Strategy.md` and `HYPOTHESES.md`): protect and
grow the queen; do not let it split or take risks late; consider hunting the enemy queen.

All text below this line was copied from the docs before the change. Where it lists the
tiebreaks it is **superseded** by the rule above.

---

## Current protocol verification — 2026-09-26

OBSERVED: isolated toolkit `unswbc 1.1.0`; its bundled C++ helper differs from
v8's helper only by the final newline. Both negotiate Protocol 3 and parse
64-bit messages plus five aggregate echo counts. Current official references:
https://game.battlecode.au/docs/protocol and https://game.battlecode.au/docs/sonar.
Four directed sonars per turn are supported, one per cardinal direction.
The Protocol 2.1 / one-sonar / 32-bit excerpts below are historical and superseded.
Own-body sonar can emerge from the tail; echoes do not identify which ray hit.
Engine SHA-256: `12e58ad88650da698a4b70c25f4d2c1b0341898787eca60fa95d219fc91ab566`.

The rules and documentations are:

[SUPERSEDED on 2026-10-02: the tiebreak list in this excerpt is the OLD rule. See the rule change log at the top of this file.]

https://game.battlecode.au/docs/structure "Rounds and Turns
Each game lasts at most 500 rounds. Within each round, every currently living dragon will perform some number of actions. One of these time steps is known as a turn.
Turns are carried out in ascending order of dragon ID. This means on a given dragon's turn, all dragons with lesser IDs have already acted, and all dragons with greater IDs have not. For more details about the game loop, see [Execution Order](https://game.battlecode.au/docs/execution-order).
Scoring
Each game has three possible outcomes: team A wins, team B wins, or a draw.
Your team is eliminated when all of your dragons perish. Upon the elimination of any team, the game automatically ends, regardless of the current round number. If exactly one team is eliminated, the other wins. If both teams are eliminated in the same round, the game is a draw. Otherwise, after 500 rounds, the winner is decided by the following tiebreaks in order:

1. The team with the longest living dragon.
2. The team with the greatest total length across all of its dragons.
3. Draw (no winning team decided)."


https://game.battlecode.au/docs/map-info: "Game Map
Dimensions
Games will be played on maps which are two-dimensional rectangular grids. Both width and height will be between 10 and 64 cells inclusive.
The top left corner will be denoted with coordinates (0, 0), with the x-coordinate increasing towards the right and the y-coordinate increasing down. North is oriented upwards.
Dragons can query the dimensions of the map as follows:
PythonCC++
Copy Code

```
auto [width, height] = game.get_map_size();
```

Symmetry
To ensure fairness between teams, each map is guaranteed to be symmetric. This will either be via vertical mirroring, horizontal mirroring, or a 180 degree rotation.
Dragons are not able to easily query the symmetry of the map; that one's up to you!
Topology
The map edges 'wrap around'; specifically, a dragon that moves outside the bounds of the map will find itself on the other side.
On any sort of visual representation of a map, this is denoted by dashed lines running down the east and south borders. Nothing renders on these borders (instead, see their equivalent west and north borders respectively).
R0
A dragon can keep moving right past the east border.
Composition
Maps are composed of tiles and edges. Again, note that edges along the east and north map border are equivalent to their west and north counterparts (as dictated by the wrap-around).
See [Pearls](https://game.battlecode.au/docs/pearls) for information about certain tiles, and [Kelp and Portals](https://game.battlecode.au/docs/kelp-and-portals) for information about nonempty edge types."

https://game.battlecode.au/docs/pearls: "Pearls
Pearls are the sole resource available, and are used to increase the length of dragons.
Pearl Spawning
Each tile has a countdown which is visible to all dragons which can inspect the tile (see [Vision](https://game.battlecode.au/docs/vision)). This countdown represents the number of rounds until said tile next attempts to spawn a pearl. Thus, each round the countdown decrements by 1. Countdowns are decremented at the start of each round, before any dragon moves (see [Execution order](https://game.battlecode.au/docs/execution-order)). When the countdown reaches zero, the tile attempts to spawn a pearl. Regardless of the outcome of this attempt, the countdown resets.
An attempt to spawn a pearl succeeds only when the tile is empty (i.e. no dragon segments nor pearls occupy it).
Resetting the countdown involves drawing uniformly at random from [`min_gap`, `max_gap`]. These values are tile-specific properties set on the map, but are not publicly visible to dragons. A tile with `max_gap` 0 never spawns pearls and always reports a countdown of -1.
R3
The marked tile's countdown reaches zero while it is empty, and so a pearl spawns.
PythonCC++
Copy Code

```
unswbc::Tile const* soonest = nullptr;
for (auto const& tile : ct.get_tiles())
{
    if (tile.get_pearl_time() < 0)
    {
        continue; // never spawns
    }
    if (!soonest || tile.get_pearl_time() < soonest->get_pearl_time())
    {
        soonest = &tile;
    }
}

if (soonest)
{
    auto const where = soonest->get_position();
    ct.set_indicator_string("pearl at " + std::to_string(where.x) + "," + std::to_string(where.y) + " in " +
                            std::to_string(soonest->get_pearl_time()));
}
```

Symmetric Pearls
On a symmetric map, each tile shares a countdown with its mirror image, so pearls will attempt to spawn symmetrically. Of course, if one of the tiles is blocked when the countdown reaches 0, a pearl will only spawn on the other tile.
Death
When a dragon dies, it will drop half its length in pearls. See [Death](https://game.battlecode.au/docs/death) for more."

https://game.battlecode.au/docs/kelp-and-portals: "Kelp and Portals
Kelp and portals exist on the edges between tiles, not the tiles themselves. This includes the edges along the map border.
R2
This poor dragon travels through a portal, straight into kelp.
Kelp
Kelp is the first type of special edge. Their job is to function as barriers; in particular, a dragon cannot pass from one tile to another across a kelp edge. If it tries, it dies (hit wall) and drops [pearls](https://game.battlecode.au/docs/death) like any other dead dragon.
Kelp is a plant. There are no other special properties of kelp.
PythonCC++
Copy Code

```
bool is_safe(unswbc::Controller const& ct, unswbc::Direction direction)
{
    auto const here = ct.get_position();
    auto const* tile = ct.get_tile(here);
    if (!tile)
    {
        return false;
    }

    if (!tile->get_edge(direction).is_passable())
    {
        return false;
    }

    auto const* ahead = ct.get_tile(here.add_dir(direction));
    return !ahead || !ahead->get_dragon();
}
```

Portals
Portals are the second type of special edge, and they allow for quick traversal of the map, conditional on your bravery. Each portal has an ID, which is an almost-unique non-negative integer. Exactly one other portal shares the same ID: these two portals are connected.
There are a couple constraints on connected portals. A pair of connected portals are guaranteed to have the same orientation (i.e. both lying along a vertical edge or a horizontal edge). Furthermore, both portals either lie on the map's line of symmetry, or both lie off it. This ensures that each portal maps to exactly one other portal without ambiguity.
Portals are double-sided, so you can enter from either side. Entering a portal from the left causes the head of your dragon to emerge from the right, and vice versa. The corresponding behaviour for vertically oriented portals is left as an exercise for you.
Dragons cannot see through to the other side of portals (see [Vision](https://game.battlecode.au/docs/vision)), so it usually won't know what's on the other side.
[Sonar](https://game.battlecode.au/docs/sonar) travels through portals.
PythonCC++
Copy Code

```
auto const& edge = tile->get_edge(direction);
if (edge.is_portal())
{
    ct.output_log("portal", edge.get_portal_id(), "to the", direction);
}
```

"

https://game.battlecode.au/docs/vision: "Vision
Every dragon can only see the 7 by 7 square of tiles centred on its head; that is, all 49 tiles with a Chebyshev distance no greater than 3. This vision square wraps around the map as well due to its topology.
Again, note that vision does not extend to the far side of a portal, which is visible only if it is within three tiles of the head by ordinary distance.
R4
A visualisation of a dragon's vision square as it moves. Note the interactions with portals and map borders.
Vision Information
For each tile in view, the dragon receives its (absolute) coordinates, its contents (pearls and dragon parts), its pearl countdown (see [Pearls](https://game.battlecode.au/docs/pearls)) and its four edges (see [Kelp and Portals](https://game.battlecode.au/docs/kelp-and-portals)).
PythonCC++
Copy Code

```
auto const* tile = ct.get_tile(pos);   // nullptr when pos is outside the window

tile->get_position();                   // board coordinates, already wrapped
tile->get_pearl_time();                 // rounds until a pearl spawns here; -1 if never
tile->has_pearl();                      // true if a pearl is on it now

auto const& edge = tile->get_edge(unswbc::Direction::NORTH);
edge.is_passable();                     // false if it is kelp
edge.is_portal();                       // true if crossing it teleports you
edge.get_portal_id();                   // id shared with the partner edge; -1 if not a portal

if (auto const* part = tile->get_dragon())   // nullptr if no dragon is on it
{
    part->get_team();                   // Team::A or Team::B
    part->get_id();                     // which dragon
    part->is_head();                    // true for the head
    part->get_dir();                    // head: its heading; body: the way to the head
}
```

A turn that steps onto a neighbouring pearl when it can see one, and onto a tile with no dragon on it otherwise:
PythonCC++
Copy Code

```
auto const here = ct.get_position();
unswbc::Direction fallback = unswbc::Direction::NORTH;

for (auto const direction : unswbc::Direction::get_direction_list())
{
    auto const* tile = ct.get_tile(here.add_dir(direction));
    if (!tile)
    {
        continue;
    }

    if (tile->has_pearl())
    {
        ct.make_move(direction);
        return;
    }
    if (!tile->get_dragon())
    {
        fallback = direction;
    }
}

ct.make_move(fallback);
```

"

https://game.battlecode.au/docs/movement: "Movement
Movement is considered a type of action that a dragon can perform on a turn. Note that each dragon must output at least one action per turn (only the last one is actually performed; see [Death](https://game.battlecode.au/docs/death) for a list of death conditions). The only other type of action which can be performed is [splitting](https://game.battlecode.au/docs/splitting).
Movement, like all other unit actions, is performed only once the turn is complete.
Standard Movement
R0
Dragons move eastwards.
Each dragon can move one tile in any cardinal direction (north, east, south, west) each turn. This causes the head to move in said direction, with the rest of the body following along.
Dragons can even move backwards, though doing so causes its head to squeeze into its own neck, a move which often proves fatal.
A dragon's facing direction is the direction of its head from its first body (non-head) segment. The facing direction of its body segments is defined similarly (direction to the next segment towards the head).
PythonCC++Raw
Copy Code

```
ct.make_move(unswbc::Direction::EAST);
```

Eating Pearls
R4
A dragon eats a pearl and grows longer by a unit.
When a dragon moves its head onto a pearl, the pearl is automatically eaten and the tail does not advance that turn. In other words, the dragon grows a body segment.
Eating pearls is the only way to gain length.
Collisions
R0
Everyone dies via various means except the orange dragon.
A dragon dies if its head moves into any obstacles. This includes [kelp](https://game.battlecode.au/docs/kelp-and-portals), any segment of another dragon, or any segment of itself.
Note that this [death](https://game.battlecode.au/docs/death) condition is checked before any actual movement. This means a dragon will die moving into own tail, even if the tail would have moved out of the way in the same turn.
If the destination contains another dragon's head, both dragons die, regardless of their team.
PythonCC++
Copy Code

```
auto const* ahead = ct.get_tile(here.add_dir(direction));
if (auto const* part = ahead ? ahead->get_dragon() : nullptr) {
    // part->is_head() means both of us die
    continue;
}
```

A dragon that dies drops pearls along its body. See [Death](https://game.battlecode.au/docs/death) for more information.
Sprinting
R2
Dragon decides to sprint north + north + east.
Dragons may decide to move several times in one turn in varying directions.
Although all moves happen within a single turn, collision checking (and pearl eating) is performed at every step. This means that in theory, a dragon might die performing `NNE`, but live with `ENN`, even though they target the same destination.
Every additional move costs a unit segment of the sprinting dragon. More rigorously, moving x steps in a turn removes x - 1 segments.
PythonCC++Raw
Copy Code

```
ct.make_moves({
    unswbc::Direction::NORTH,
    unswbc::Direction::NORTH,
    unswbc::Direction::EAST,
});
```

Death by Sprint
R1
Dragon implodes due to oversprinting.
A move with more steps than the dragon can pay for is nottruncated. The dragon will perform the steps that can be paid for, and then will pay for the next step with its own life."

https://game.battlecode.au/docs/splitting: "Splitting
Splitting is another action that dragons can perform. A dragon can only perform one action per turn.
R3
A length 6 dragon splits. The parent continues east. The child leaves west, head first, from the old tail.
A dragon may split itself to create a new dragon on its team. Note that the minimum length for any dragon is 2. The child consists of the parent's rear segments in reverse order, so the parent's old tail becomes the child's head and the child faces away from the parent.
The child receives the next unused dragon id, and takes its first turn later in the same round. The child is controlled by a brand new instance of your program, and has no memory of the parent.
Constraints
A split is legal only if all of the following hold:

1. The child has length at least 2. `SPLIT 1` is never legal.
2. The parent has length at least 2 after the split. A dragon of length 6 may split 2, 3 or 4.
3. The team has fewer than 64 living dragons.

Requesting an illegal split kills the parent with no valid action.
PythonCC++Raw
Copy Code

```
if (ct.can_split(2))
{
    ct.do_split(2);
}
else
{
    ct.make_move(unswbc::Direction::NORTH);
}
```

"

https://game.battlecode.au/docs/sonar: "Sonar
Sending
R2
Two teammates. Each turn the ray leaves the sender and stops at the first body it meets.
Once per turn a dragon may send one unsigned 64-bit integer. After the dragon's action has been applied, if the dragon is still alive, the signal travels in a straight line from its head in the direction it now faces. This signal, wraps around the board and passes through portals, stopping at the first kelp or dragon part it hits. If it hits a dragon, the message is delivered to that dragon, which may be the sender itself. If it reaches no dragon within `WIDTH + HEIGHT` tiles, it is lost.
PythonCC++Raw
Copy Code

```
ct.send_sonar(1806);

for (auto const message : ct.get_sonar_messages())
{
    ct.output_log("heard", message);
}
```

Receiving
R0
The first message reaches our friend, but then an enemy gets in the way and intercepts all our later messages.
Signals are read at the start of the recipient's next turn. A dragon with a higher id than the sender therefore receives the signal later in the same round, and a dragon with a lower id receives it in the following round. Several signals may arrive in one turn, in the order they were sent. A signal is lost if the sender died during its move, since no signal is cast, or if it reached nothing. The replay shows the ray in either case. A signal may be received by a dragon on either team. No information about the sender or team is carried."

https://game.battlecode.au/docs/death: "Death

A dragon dies when its head collides with something, or when its program fails to produce a valid action on its turn.

Causes of Death

Reason	Cause
Hit Wall	Moved into kelp.
Hit Self	Moved into its own body, including its tail.
Hit Other Body	Moved into a segment of another dragon from any team.
Head to Head	Moved into another dragon's head, causing both dragons to die.
No Valid Action	Requested a sprint it could not pay for, requested an illegal split, produced a malformed command, or outputted no action.
Mandatory Action

The engine treats a turn with no valid action as suicide. Any dragon whose program crashes or outputs nothing (or garbage) will be killed. Dragons which fail to end their turn before the time limit but have outputted at least one valid action will experience a different fate (see Timeouts).

Pearl Dropping

Upon death, dragons exhibit a startling phenomenon: starting from their head, every second body segment crystallises into pearls. A dragon of length L therefore drops ⌈L/2⌉ pearls.




R3
A dragon dies and drops pearls at every second segment."

https://game.battlecode.au/docs/cli: "The CLI
`unswbc` creates a bot project, builds it, plays matches and uploads the result. The commands below are in the order you will need them. `unswbc help <command>` lists the options for any one of them.
Recommended
`unswbc` is a Python package, and it carries the judge's clang, so C and C++ bots build in the sandbox with no further installing. Install [uv](https://docs.astral.sh/uv/) if you do not have it, open a new shell so it is on your PATH, then install the toolkit. Later, `uv tool install unswbc@latest` takes the newest version and `uv tool install unswbc==0.3.0` pins one. The toolkit carries the engine, so an old one plays by different rules than the judge: it asks PyPI once a day and offers to install a newer one when there is one. `UNSWBC_NO_UPDATE=1` silences that.
RawCopy Code

```
uv tool install unswbc
```

If you don't have uv
pip installs it just as well, given Python 3.11 or newer, and puts `unswbc` on your PATH. Debian, Ubuntu and Homebrew refuse a plain pip install and say `externally-managed-environment`; on those, uv is the way.
RawCopy Code

```
pip install unswbc
```

unswbc
On its own it lists the commands and checks this machine: the Python interpreter, the C and C++ compilers, VS Code and the replay viewer. Anything missing is named with the step that installs it.
RawCopy Code

```
unswbc
```

unswbc init
Creates a project in `c`, `cpp` or `python`, holding a starter bot and the helper library. The directory defaults to the current one. Maps do not go inside it: they land in a `maps/` folder beside the project, which every bot of yours can share.
RawCopy Code

```
unswbc init python mybot
```

unswbc maps
Adds any bundled map the folder is missing, and leaves your own files alone, so run it after upgrading the toolkit. The folder defaults to `maps`; name another to choose it, such as `unswbc maps ../maps` for a project you made in the current directory.
RawCopy Code

```
unswbc maps
```

unswbc run
Plays one match between two bots on a map and writes a replay. Each bot is a project directory, rebuilt before it plays, and naming the same one twice plays it against itself. Add `-v` to watch every round, or `--sandbox` to run a Python, C, C++ or wasm bot the way the judge does: priced in CPU points, with its logs, indicators and drawings capped. Without it, a local match keeps all of them.
RawCopy Code

```
unswbc run maps/arena.map mybot mybot
```

unswbc vscode
Installs the replay viewer into VS Code, Cursor or VSCodium, so any `.replay` file opens in the editor. `init`already does this when it finds one.
RawCopy Code

```
unswbc vscode
```

unswbc auth
Stores the API key from your [team page](https://game.battlecode.au/team), so this machine may submit. `unswbc auth status` shows the key in use, and `unswbc auth clear` removes it.
RawCopy Code

```
unswbc auth set bc_...
```

unswbc submit
Zips the project and uploads it as a new submission. `-n` names the version and `-d` records what changed. See [Submitting via website](https://game.battlecode.au/docs/submitting).
RawCopy Code

```
unswbc submit mybot
```

unswbc log
Prints the errors the toolkit has hit on this machine. Send it with any bug report; API keys are masked.
RawCopy Code

```
unswbc log
```

"

https://game.battlecode.au/docs/execution-order:

https://game.battlecode.au/docs/timeouts:

https://game.battlecode.au/docs/libraries: 

https://game.battlecode.au/docs/helper: 

https://game.battlecode.au/docs/protocol: "Wire protocol

Version 2.1.0. The engine writes plain text to the bot's stdin and reads plain text from its stdout. The helper library only parses and prints this format and may be replaced. Blank lines may be skipped, and the C and C++ helpers strip anything after #.

Init block

Sent once, when the process starts.

Raw
Copy Code
ID 0
TEAM A
MAP 11 11
UNIT_LIMIT 64
ID: the dragon's id, unique within the game.
TEAM: A or B.
MAP: width, then height.
UNIT_LIMIT: the maximum number of living dragons per team. Default 64.
A split child and a restarted bot each receive their own init block. It marks the start of a process, not the start of the game.

Round block

Sent once per turn, in this order.

1. Header

Raw
Copy Code
ROUND 12
DIR N
LENGTH 5
UNIT_COUNT 2
NUM_MSGS 1
90210
ROUND, DIR (the dragon's facing), LENGTH, UNIT_COUNT (living dragons on the dragon's team), and NUM_MSGS, followed by that many lines, each one unsigned 32-bit sonar value.

2. Tiles, 49 lines

Raw
Copy Code
2 1 0 -1
3 1 1 8
4 1 0 3
...
The 7×7 window, row by row. Row 0 is three tiles north of the head, and column 0 is three tiles west. Each line is x y hasPearl pearlIn. Coordinates are already wrapped. pearlIn is −1 for a tile that never spawns.

3. Dragon bodies

Raw
Copy Code
DRAGON_BODIES 4
A 0 5 4 N 1
A 0 5 5 N 0
B 1 7 3 W 1
B 1 8 3 W 0
DRAGON_BODIES and a count, then one line team id x y facing isHead for every segment of every living dragon inside the window, the bot's own included. A head's facing is its heading; a body segment's facing points towards the head.

4. Horizontal edges, 8 lines of 7

Raw
Copy Code
. . . . . . .
. . w w . . .
. . . . . . .
. . . . . . .
. . . 3 . . .
. . . . . . .
. . . . . . .
. . . . . . .
The north edge of each row of tiles, then the south edge of the last row.

5. Vertical edges, 7 lines of 8

Raw
Copy Code
. . . . . . . .
. . . . . . . .
. w . . . . . .
. . . . . . . .
. . . . . . . .
. . . . . . . .
. . . . . . . .
The west edge of each column of tiles, then the east edge of the last column.

Edge symbols

Symbol	Meaning
.	Open.
w	Kelp.
An integer	A portal. The other edge with that id is the far end.
Split on whitespace, since a portal id may have more than one digit.

Reply

Lines to stdout, ending with ENDTURN, then a flush. One flush per turn is the cheap way (see Timeouts).

Command	Arguments	Effect
MOVE	N/E/S/W letters	Action: move. MOVE NNE is three steps.
SPLIT	Segment count	Action: split off the rear segments.
SONAR	Unsigned 32-bit integer	Ping along the facing after the action.
INDICATOR	Text	Label on the dragon this turn.
LOG	Text	Message attached to this turn.
DOT	x y r g b	Dot on the board in the replay.
LINE	x1 y1 x2 y2 r g b	Line on the board in the replay.
ENDTURN		Stop reading.
The first four overwrite each other, and the last one read takes effect. The drawing commands are applied as they are read. See Execution order for the exact sequence.

Ending

When the game ends for a dragon, the engine closes its stdin. Reading a line then returns EOF, and the process should exit. The helpers also stop on ENDGAME, which the current engine does not send."

https://game.battlecode.au/docs/map-files: "Map Files

A .map file is plain text with one directive per line. The map editor writes them, and they can be written by hand.

Raw
Copy Code
MAP 11 11
SYMMETRY xy
TILE_COUNT 81
TILE 1 1 8 20
TILE 2 1 8 20
...
EDGE_COUNT 44
EDGE 0 1 -1
EDGE 12 2 3
...
DRAGON_COUNT 2
DRAGON 0 3 3 5 2 5 1 5
DRAGON 1 3 7 5 8 5 9 5
Directives

MAP width height

Required, and first. Everything after it is bounds-checked against it.

MAP_NAME name

Optional. The visualiser and the VS Code viewer display this name.

SYMMETRY x | y | xy

Optional. Mirrored tiles share one pearl countdown and fire together, so a symmetric map is fair in timing as well as in shape. See Pearl spawns.

TILE x y minGap maxGap

The tile's spawn range. After each attempt the tile draws its next countdown uniformly from minGap to maxGap inclusive. A maxGap of 0 means the tile never spawns. TILE_COUNT must match the number of TILE lines, and likewise EDGE_COUNT and DRAGON_COUNT.

EDGE index kind portalId

Kind	Meaning
0	Open.
1	Kelp. Portal id -1.
2	Portal. Exactly two edges share the id, and both must have the same orientation.
Rows of horizontal edges (a tile row's north side) alternate with rows of vertical edges (its west side). Each row is width + 1 long, so the index runs to (2·height + 1)·(width + 1). The editor is the practical way to write these.

DRAGON team segmentCount x y x y ...

team is 0 (A) or 1 (B), followed by the segment count and then the coordinates, head first. Segments must be adjacent, on the board, and must not overlap another dragon. The minimum length is 2. Ids are assigned in file order from 0, which is also turn order."

https://game.battlecode.au/docs/api: "API
Everything a team does on Submissions and Battles, and everything readable on the site, is available over JSON. Team settings, membership and accounts are changed on the site only.
Keys
Each member makes one key on [Team](https://game.battlecode.au/team). It acts as that member, so uploads and challenges are recorded against their name. The key is shown once. Making a new one replaces the old one, leaving the team deletes it, and the leader can revoke any member's key.
RawCopy Code

```
curl https://game.battlecode.au/api/v1/me \
  -H "Authorization: Bearer bc_..."
```

Conventions
The base URL is `https://game.battlecode.au/api/v1`. Requests and responses are JSON, except the upload (multipart) and the two downloads. Errors carry the status and a message:
RawCopy Code

```
{ "error": "That API key is not valid. Make a new one on your team page." }
```

The limits are 120 requests a minute per key, and 30 a minute for `/leaderboard` and `/ratings`, the two expensive reads. Over either limit the response is 429 with a `Retry-After` header. An unknown path under `/api/v1` is a 404 in the same shape, never an HTML page.
You and your team

* `GET /me` your user, team and key.
* `GET /team` your team with members, record, rank and pending join requests.

Submissions

* `GET /submissions` every version with wins, draws and losses.
* `POST /submissions` upload a version. Fields: name, language (python, c, cpp), description, zip. The zip is 4 MB at most, and a team can upload 12 versions an hour.
* `GET /submissions/:id` one version with its build log.
* `GET /submissions/:id/download` the zip you uploaded.
* `POST /submissions/:id/activate` make it the version that plays.

RawCopy Code

```
curl https://game.battlecode.au/api/v1/submissions \
  -H "Authorization: Bearer $KEY" \
  -F name=v12 -F language=python -F description="wider search" -F zip=@mybot.zip
```

RawCopy Code

```
curl -X POST https://game.battlecode.au/api/v1/submissions/42/activate \
  -H "Authorization: Bearer $KEY"
```

Battles

* `GET /battles?limit=50` your battles, newest first, up to 200.
* `POST /battles` challenge a team. Body: teamId, ranked, mapIds (optional). Ranked is a five-game series. Same limits as the site: one pending battle per opponent, 60 games an hour.
* `GET /battles/:id` one battle with its games and, if unplayed, its place in the queue. For your own battles it also carries `log`, which says why a game crashed or timed out.
* `GET /battles/:id/replay` the replay file, once the game has finished. Answers with a redirect to a temporary download link, so follow redirects (`curl -L`). The link carries its own signature, so your key must not be sent on to it. curl drops the header across hosts for you; some clients keep it, Python's `urllib` among them, and the download is refused.

RawCopy Code

```
curl https://game.battlecode.au/api/v1/battles \
  -H "Authorization: Bearer $KEY" -H "Content-Type: application/json" \
  -d '{ "teamId": 7, "ranked": false, "mapIds": [3] }'
```

RawCopy Code

```
{ "ids": [1201] }
```

RawCopy Code

```
curl -L -o 1201.replay https://game.battlecode.au/api/v1/battles/1201/replay \
  -H "Authorization: Bearer $KEY"
```

Reads

* `GET /leaderboard` the ladder as the Leaderboard page shows it: rating history, members, institutions and prize eligibility.
* `GET /ratings` the same ladder, ratings and history only.
* `GET /teams` teams listed on Find a team.
* `GET /teams/:id` a team's public profile and recent battles.
* `GET /tournaments` and `GET /tournaments/:id` tournaments and brackets.
* `GET /maps` the maps in play, with their text.
* `GET /queue` live judge capacity and queue length."
