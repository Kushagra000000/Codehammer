#include "helper.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// v11: king safety (sprint-reach threat model, time-aware space, escape sprints),
// roles that follow the body, validated sonar, round-gated worker aggression.
// v13: no Hamiltonian patrols (the king roams for pearls), pocket raids with a
// split-reverse escape, value-and-risk portal scoring, shared portal maps, no
// centre bias.
// v14-v16: exploration (coverage sharing, dispersal), pearl routing, early portal
// exploring. v17: AGGR modes 2 (default) and 3 only, no straight-ahead bias, short
// dragons do not split in 1-wide corridors.
// v20: scoring in segments, one goal per turn, fight lookahead (scoring.hpp).
// v21: survival search over our own next moves (survival.hpp); traps priced by
// whether we could split out; long workers may use a reserved escape slot.
// v21 (continued): the ceil(L/4) free-steps rule throughout (free_moves); moves of
// several free steps; routing with time-aware paths, arrival in turns, who gets
// there first, and a survival check where each route ends (route.hpp); bodies
// remembered for as long as they will be there; allies' reach (in sight or by
// status packet); a price on landing next to tiles we cannot see; routes as far as
// a few turns of free steps; rushes past our window over remembered ground; bodies
// predicted where they run out of sight; 12-bit ids compared as such.
// v22: the judge's points budget (100M a turn; over it the dragon dies, "no valid
// action"). The judge's clock counts points, so each search stops at its phase's
// deadline (Config::TURN_POINTS); a per-turn neighbour table (nbr_) and a one-pass
// plan_options make the same searches several times cheaper. Also the queen (queen.hpp)
// and the centre rush (scoring.hpp).
// v23: per-map settings: the centre rush only on the maps in Config::CENTRE_MAPS. Survival
// searches tell walled-in moves from contested ones. The enemy queen is killed on sight.
// Split escapes keep the most length that can live (endgame.hpp choose_escape: the queen
// survives Schooltime's box and Slithery Fight's coil), workers keep off our boxed-in
// queen's last free sides, and good pearl beds (learnt respawn rate) are farmed. The queen
// stays out of fights (valuable from round 0, priced as 30 segments, keeps 4 while seeding).
// v24: portals. A dragon shut in a portal room crosses out, pearls in portal pockets it would
// die in are not objectives, a portal on the goal's way is either crossed or routed around,
// and a blind crossing is priced by round (8% early, 20% late): open early, and late only
// towards a far side known to be worth it.
// v25: dispersion. Early on, a tile to explore that a known teammate reaches first is a
// weaker objective (ally_claims), and at most two of us rush the centre. Pearls we know are
// coming are objectives in time to arrive as they spawn (spawn_forecast_cost).
// v26: a portal we have never looked through is a way on for the survival search (not a
// wall), so a dragon (the queen above all) is no longer "certainly dead" beside one.
namespace Config
{
    // Growers start retaining at SPLIT_CUTOFF (v13: 300, was 250): the roaming king
    // catches up quickly, so the swarm keeps splitting longer.
    constexpr int SPLIT_CUTOFF = 300, FINAL_GROWTH_CUTOFF = 425;
    constexpr int TARGET_SWARM_SIZE = 64, MAX_SWARM_SIZE = 64;
    constexpr int GROWER_NUMERATOR = 1, GROWER_DENOMINATOR = 10; // v14: 1 in 10 (was 8)
    constexpr int GROWER_SELECTION = 0;                          // 0 stable hash, 1 ID modulo
    constexpr int MAX_PRE_GROWTH_LENGTH = 3;
    constexpr int HEAD_TRADE_MAX_LENGTH = 3, SPRINT_TRADE_MAX_LENGTH = 4;
    constexpr bool KING_ENABLED = true;
    constexpr int FEED_MIN_GROWER_LENGTH = 10;
    constexpr int ENEMY_TARGET_MIN_LENGTH = 5;
    constexpr int KING_SELECTION = 0, KING_ID = -1, ROOT_POLICY = 0, BACKUP_KING_POLICY = 0;
    constexpr int FEED_START_ROUND = 380, FEEDER_MAX_LENGTH = 4;
    constexpr int FEEDING_RADIUS = 3, KING_PROTECTION_RADIUS = 3;
    constexpr int SACRIFICE_METHOD = 1; // verified own-body collision; preserves full death yield
    constexpr int SONAR_STRATEGY = 4, SONAR_FREQUENCY = 1;

    // --- v11: worker aggression ---
    // No trades, attack sprints or hunting before this round: early game is for eating and splitting.
    constexpr int AGGRESSION_START_ROUND = 50;
    constexpr int MIN_UNITS_FOR_TRADE = 6;
    // From this round any non-retaining dragon may trade into a long enemy (their tiebreak dragon).
    constexpr int ENDGAME_HUNT_ROUND = 470, ENDGAME_HUNT_MIN_LENGTH = 10;
    constexpr int MAX_ATTACK_STEPS = 6; // total steps including the one onto the enemy head
    constexpr int HUNT_RADIUS = 15, REPORT_HUNT_RADIUS = 12;
#ifndef AGGR_MODE
#define AGGR_MODE 2
#endif
    static_assert(AGGR_MODE == 2 || AGGR_MODE == 3, "AGGR_MODE is 2 (aggressive) or 3 (survival)");
    // 2 (default): elimination first. Trade into anything at least as long; hunt
    // visible enemies 2+ longer and long reported ones nearby.
    // 3: trades must not cost us swarm size. Equal trades only at length <= 3;
    // longer workers trade only into clearly longer enemies; walking towards
    // enemies only for long targets and only while the swarm is healthy.
    constexpr int FAVOURABLE_MARGIN = 2;
    constexpr int HUNT_MIN_TARGET_LENGTH = 8, HUNT_MIN_UNITS = 40, HUNT_MARGIN = 4;

    // --- v11: roles ---
    // Only a grower escape produces a child this long; worker splits are capped below it,
    // so a newborn this long knows it carries a grower's body.
    constexpr int HEIR_MIN_BIRTH_LENGTH = 10;
    constexpr int TOP_TWO_MARGIN = 3, LENGTH_CACHE_ROUNDS = 10;

    // --- v11: king safety ---
    constexpr int VALUABLE_LENGTH = 16;
    // A hidden body is assumed as long as the enemies we have seen whole (running
    // average), capped at this reach.
    constexpr int HIDDEN_BODY_REACH = 10;
    constexpr int ENEMY_VISION_RADIUS = 3; // an enemy can only target a head it can see
    constexpr int MAX_ESCAPE_STEPS = 6;

    // --- v11: pearl collection ---
    // v13 objective radius (path steps + source cost) by role. Retaining dragons (the
    // king, growers) see the whole map: they head for the closest thing worth eating
    // anywhere they know of. Workers look further than before because unexplored
    // tiles are objectives too. Feeders keep the old short radius and do not explore.
    constexpr int KEEPER_OBJECTIVE_RADIUS = 200, WORKER_OBJECTIVE_RADIUS = 30, FEEDER_OBJECTIVE_RADIUS = 12;
    // A never-seen tile counts as an objective this many steps further away than a
    // pearl would (what replaced the centre bias and the old exploration bonus).
    constexpr int WORKER_EXPLORE_COST = 8, KEEPER_EXPLORE_COST = 12;
    // A remembered pearl we can no longer see costs age/DIV extra steps, and is
    // forgotten after MAX_AGE rounds.
    constexpr int REMEMBERED_PEARL_AGE_DIV = 10, REMEMBERED_PEARL_MAX_AGE = 50, KEEPER_REMEMBERED_PEARL_MAX_AGE = 100;
    // A tile about to spawn counts once it is this close to spawning.
    // (v25: SPAWN_SOON_ROUNDS = 4 / KEEPER_SPAWN_SOON_ROUNDS = 10 are gone: every known spawn is
    // an objective, see spawn_forecast_cost.)
    // An unseen tile whose remembered countdown ran out may hold a pearl; it is a
    // weak objective that fades, so it never outweighs a pearl we know about.
    constexpr int EXPIRED_TIMER_BASE_COST = 5, EXPIRED_TIMER_AGE_DIV = 8, EXPIRED_TIMER_MAX_AGE = 40;
    constexpr int KEEPER_EXPIRED_TIMER_MAX_AGE = 200, EXPIRED_TIMER_AGE_CAP = 120;

    // --- v13: pocket raids ---
    // A worker may enter a dead end it cannot turn around in when it holds at least
    // this many pearls: it eats them, then splits (the rear L-2 reverse out, the 2 in
    // front are the sacrifice). Net gain: pearls - 2.
    constexpr int POCKET_MIN_PEARLS = 3;
    constexpr int POCKET_PEARL_MEMORY = 10; // remembered pearls this fresh count too
    // Any other move into such a trap (a closed region with no loop, e.g. a 1-wide
    // dead end of any depth) is unsafe, and its entrance is ignored by the objective
    // map for a while.
    constexpr int TRAP_MARK_ROUNDS = 10;
    // v25: dispersion (main.cpp ally_claims): until CLAIM_END_ROUND a tile to explore or an
    // unwatched timer a known teammate reaches first costs CLAIMED_EXTRA more (a never-seen
    // tile costs 8); teammates' sent positions count for CLAIM_SIGNAL_AGE rounds. The centre
    // rush pulls only a dragon with fewer than CENTRE_MAX_RUSHERS known teammates closer to
    // the centre.
    constexpr int CLAIM_END_ROUND = 200, CLAIMED_EXTRA = 10, CLAIM_SIGNAL_AGE = 4;
    constexpr int CENTRE_MAX_RUSHERS = 2;
    // v25: anticipated pearls (main.cpp spawn_forecast_cost).
    constexpr int FORECAST_CLUSTER_RADIUS = 3, FORECAST_CLUSTER_WINDOW = 6, FORECAST_CLUSTER_DIV = 3, FORECAST_CLUSTER_CAP = 8;
    constexpr int FORECAST_MAX_WAIT = 2;
    // v23: shut in a portal room (main.cpp update_portal_room): a crossing out is priced at
    // P_ROOM_EXIT (exit tiles were taken 2-25% of the time in the simulator on official maps)
    // and is worth ROOM_EXIT_VALUE.
    constexpr int PORTAL_ROOM_MAX_CELLS = 64, ROOM_EXIT_VALUE = 1500;
    constexpr int PORTAL_POCKET_MAX_CELLS = 8; // v23: portal_pocket_worth
    constexpr int PORTAL_REFUSE_ROUNDS = 15;   // v23: refuse_route_portals
    constexpr double P_ROOM_EXIT = 0.1;
    // v23: farming. A bed is good when its mean respawn gap, estimated from the countdowns
    // seen on it (bed_gap; GOOD_BED_MIN_SAMPLES rounds of them), is at most GOOD_BED_GAP.
    // A good bed was an objective 8 rounds before it spawns (others: 4; v25: any known spawn is), and once
    // its timer has run out unseen for up to GOOD_BED_EXPIRED_MAX_AGE rounds at base cost
    // GOOD_BED_EXPIRED_COST (others: 5, for 40); walking over one again costs no VISIT_COST.
    constexpr int GOOD_BED_MIN_SAMPLES = 3, GOOD_BED_GAP = 12;
    constexpr int GOOD_BED_EXPIRED_COST = 2, GOOD_BED_EXPIRED_MAX_AGE = 150; // (v25: GOOD_BED_SOON_ROUNDS gone)

    // --- v13: portals ---
    // Nothing is assumed about a portal. Its value is what we know of the far side:
    // the objective pull runs through known portals, plus how rich the area around
    // the exit is compared with where we are (pearls, timers about to fire, timers
    // that fired unwatched, unexplored tiles). Its risk depends on the role.
    constexpr int PORTAL_REGION_RADIUS = 4; // 9x9 area around the exit / our head
    // v16: early on, for workers, a portal whose exit we have never seen is an
    // exploration objective (the tiles in front of it, at this cost: a never-seen tile
    // is 8) and an ordinary move, so they walk to it from afar and go through. Only
    // until PORTAL_EXPLORE_END_ROUND: an exit we cannot see holds a body about as often
    // as the map is full (in local tests ~45% full by round 50, and about half of all
    // blind crossings after that died, ours and the baseline's).
    // v26.1.0: early portal pull for workers (scoring.hpp: centre_pull, the eat goal, portal_value).
    // false: V25's (180 for a crossing, explore goal only; objective cost 5).
    constexpr bool EARLY_PORTAL_PULL = true;
    constexpr int UNKNOWN_PORTAL_EXPLORE_COST = EARLY_PORTAL_PULL ? 2 : 5; // v26: was 5 (a never-seen tile is 8)
    constexpr int PORTAL_EXPLORE_END_ROUND = 100;                          // v24: was 80
    // v24: portals are open early, and late only the good ones are taken.
    //  - A blind crossing (far tile out of sight) dies with probability blind_portal_p():
    //    P_BLIND_EARLY up to round PORTAL_RISK_EARLY_ROUND, rising linearly to P_BLIND_LATE at
    //    PORTAL_RISK_LATE_ROUND (official maps in the simulator: the far tile held a dragon
    //    2-14% of rounds 0-49, 8-19% of rounds 50-149, 12-25% later). Half that when we saw
    //    the far tile free in the last KNOWN_FREE_MEMORY rounds. (Was 30% at any time.)
    //  - Until PORTAL_EXPLORE_END_ROUND the far side is valued as before (PORTAL_REGION_PER_PEARL,
    //    unexplored tiles included). After it, only pearls and timers we know of count, at
    //    PORTAL_GOOD_PER_PEARL per expected pearl (up to PORTAL_GOOD_CAP), and only for a far
    //    tile with a way on: a late crossing must lead somewhere worth its risk.
    constexpr double P_BLIND_EARLY = 0.08, P_BLIND_LATE = 0.20;
    constexpr int PORTAL_RISK_EARLY_ROUND = 40, PORTAL_RISK_LATE_ROUND = 150;
    constexpr int PORTAL_GOOD_PER_PEARL = 300, PORTAL_GOOD_CAP = 2500;
    // Taking the same portal pair again soon, with nothing to eat that way.
    constexpr int PORTAL_LOOP_ROUNDS = 12;
    // v26: the queen, the king and the growers take no portal into the unknown (a pair never
    // seen through, or a far tile out of sight) while another move keeps them alive (main.cpp
    // apply_portal_keepout): such a portal costs them a death then.
    constexpr bool PORTAL_KEEPOUT = true;
    // --- v14: exploration and contested pearls ---
    // Workers head for a destination in a block the team has not explored yet (see
    // explore.hpp); coverage maps are shared by sonar (magic 0x9) and at splits.
#ifdef NO_DISPERSE
    constexpr bool DISPERSE_ENABLED = false;
#else
    constexpr bool DISPERSE_ENABLED = true;
#endif
#ifdef NO_EXPLORE_SHARING
    constexpr bool EXPLORE_SHARING = false;
#else
    constexpr bool EXPLORE_SHARING = true;
#endif
    // Small maps are explored within 50 rounds anyway, and on any map the first
    // swarm does best eating where it is: destinations only on big maps, from here.
    // (v25: destinations on every map from round 0 lost 3-9 to v24, with 24% less length at
    // round 50: kept as it was.)
    constexpr int DISPERSE_MIN_AREA = 1600, DISPERSE_START_ROUND = 20;
    constexpr int TARGET_TIMEOUT_MIN = 30;  // give up on a destination after max(this, W + H) rounds
    constexpr int EXPLORED_BLOCK_EXTRA = 8; // never-seen tiles in a covered block cost this much more
    // An enemy in sight that could reach a pearl no later than we would walking:
    // sprint to it (at most this many steps).
    constexpr int PEARL_SPRINT_MAX_STEPS = 4;
    // --- v15: pearl routing (route.hpp) ---
    // Plan a route through the nearest known pearls (up to ROUTE_MAX_PEARLS within
    // RADIUS steps, legs within HORIZON steps in total). A pearl reached t steps from
    // now is worth DISCOUNT^t, so clusters win over lone pearls slightly closer.
    constexpr bool ROUTING_ENABLED = true;
    constexpr int KEEPER_ROUTE_RADIUS = 20, WORKER_ROUTE_RADIUS = 14;
    constexpr int KEEPER_ROUTE_HORIZON = 28, WORKER_ROUTE_HORIZON = 16;
    // v21: ...or, for dragons with many free steps, this many turns of them (a king of 80
    // covers 20 steps in one turn), up to ROUTE_MAX_RADIUS / ROUTE_MAX_HORIZON steps.
    constexpr int KEEPER_ROUTE_RADIUS_TURNS = 4, KEEPER_ROUTE_HORIZON_TURNS = 6;
    constexpr int WORKER_ROUTE_RADIUS_TURNS = 3, WORKER_ROUTE_HORIZON_TURNS = 4;
    constexpr int ROUTE_MAX_RADIUS = 100, ROUTE_MAX_HORIZON = 140;
    constexpr int ROUTE_MAX_PEARLS = 14, ROUTE_BEAM = 16, ROUTE_REPLAYS = 8;
    constexpr double KEEPER_ROUTE_DISCOUNT = 0.92, WORKER_ROUTE_DISCOUNT = 0.85;
    constexpr double ROUTE_KEEP_BONUS = 0.15; // for sticking to last turn's target
    // Crowds: each other dragon's segment within 2 tiles of a pearl divides its worth
    // by (1 + this); an enemy head within ROUTE_ENEMY_NEAR multiplies a keeper's by the factor.
    constexpr double ROUTE_KEEPER_CROWD = 0.25, ROUTE_WORKER_CROWD = 0.08;
    constexpr int ROUTE_ENEMY_NEAR = 4;
    constexpr double ROUTE_KEEPER_ENEMY_FACTOR = 0.4;
    // Never trade into an enemy this much shorter than us (or more).
    constexpr int TRADE_MAX_SIZE_GAP = 4;
    // v17: a dragon this long or shorter does not split with its head in a 1-wide
    // corridor (split escapes still happen).
    constexpr int CORRIDOR_SPLIT_MAX_LENGTH = 5;

    // Portal pairs are shared by sonar (magic 0xB) every PERIOD rounds, until GATHER_ROUND,
    // on one of the four directions (the status packet gives way). false: never sent.
    constexpr bool PORTAL_SHARING = true;
    constexpr int PORTAL_SHARE_PERIOD = 3;

    // --- v12: late game ---
    constexpr int SPLIT_STOP_ROUND = 380; // no worker splits after this
    constexpr int GATHER_ROUND = 380;     // every non-retaining dragon heads for the king...
    constexpr int SACRIFICE_ROUND = 400;  // ...and from here turns itself into the king's pearls
    constexpr int GATHER_DISTANCE = 7;    // before SACRIFICE_ROUND, wait this far from the king
    constexpr int KING_MIN_LENGTH = 12;   // before GATHER_ROUND, shorter dragons never count as king
    // v22: the queen (queen.hpp). She wins every king election from QUEEN_CROWN_ROUND (the
    // swarm gathers on her) and walks her cycle from QUEEN_SEED_END_ROUND to
    // QUEEN_CYCLE_END_ROUND. Before QUEEN_SEED_END_ROUND she plays as a founder and splits
    // to grow the swarm. The cycle's block: at most QUEEN_BLOCK_MAX a side and
    // QUEEN_BLOCK_AREA tiles (more if our length needs it), at least our length +
    // QUEEN_CYCLE_SLACK tiles; a bigger one is looked for every QUEEN_REBUILD_ROUNDS once
    // we are within that slack.
    constexpr int QUEEN_CROWN_ROUND = 380, QUEEN_CYCLE_END_ROUND = 400;
    constexpr int QUEEN_SEED_END_ROUND = 75;
    constexpr int QUEEN_BLOCK_MAX = 16, QUEEN_BLOCK_AREA = 144, QUEEN_CYCLE_SLACK = 12, QUEEN_REBUILD_ROUNDS = 10;
    constexpr int QUEEN_RETRY_ROUNDS = 3, QUEEN_CYCLE_MIN_FREE = 4, QUEEN_GROWTH_MARGIN = 2;
    constexpr int QUEEN_RETURN_RADIUS = 40;
    // v23: any of us (but our queen) who sees the enemy queen kills her if it can reach her
    // head this turn (queen.hpp, checked before anything else); otherwise QUEEN_HUNT_PULL
    // per step closer to her. KILL_ENEMY_QUEEN = false turns both off.
    constexpr bool KILL_ENEMY_QUEEN = true;
    constexpr int QUEEN_HUNT_PULL = 1000;
    constexpr int QUEEN_EXIT_BLOCK_COST = 4000; // v23: a worker's move onto one of our boxed-in queen's last free sides
    constexpr int QUEEN_SIDE_COST = 600;        // ...onto any free side of her head
    // v23: the queen stays out of fights. Her death costs at least QUEEN_DEATH_LENGTH segments
    // (every risk is priced against it), she never takes a fight goal or the centre rush,
    // and each step away from the nearest enemy head is worth QUEEN_ENEMY_DISTANCE_STEP.
    constexpr int QUEEN_DEATH_LENGTH = 30, QUEEN_ENEMY_DISTANCE_STEP = 150;
    // v23: while seeding she keeps this much: 2-3 long she could neither escape-split nor
    // outlast a crowd.
    constexpr int QUEEN_SEED_KEEP = 4;
    constexpr int LATE_KING_MIN_LENGTH = 3; // from GATHER_ROUND a king is always elected
    // Kingless dragons meet at the map centre (v20: per step, in thousandths of a segment).
    constexpr int RENDEZVOUS_PULL = 200, RENDEZVOUS_KING_PULL = 53;
    // Small-team mode (from SACRIFICE_ROUND with at most this many of us alive).
    constexpr int SMALL_TEAM_UNITS = 6, SMALL_TEAM_FEED_RADIUS = 6;
    constexpr int SMALL_TEAM_KING_PULL = 105; // a lonely king of a small team drifts to the centre
    constexpr int SMALL_TEAM_KING_TTL = 40;
    constexpr int KING_INFO_TTL = 15; // rounds a relayed king report stays valid
    // A second retainer ("vice") as insurance. Off: any dragon that knows of a clearly
    // longer ally feeds it. Set VICE_PERCENT to e.g. 60 to keep a vice at >= 60% of the king.
    constexpr int VICE_MIN_LENGTH = 20, VICE_PERCENT = 101;
    constexpr int BEACON_START_ROUND = SPLIT_CUTOFF, BEACON_DOUBLE_ROUND = 380;
    constexpr int FEED_NEAR_RADIUS = 4; // die this close to a visible king
    // A trapped feeder farther than this from the king split-escapes instead of dying.
    constexpr int FEEDER_ESCAPE_KING_RADIUS = 6;
    // Escape by splitting: the tail becomes a new head carrying the body. Growers
    // this long also do it to get out of tight space (anyone does it rather than die).
    constexpr int SPLIT_ESCAPE_MIN_LENGTH = 12;
    // v26.2.0: a worker's split takes 2 off its tail: the child is 2 long and the worker keeps
    // the rest, L = (L-2) + 2 = (L-4) + 2 + 2 ... over the next turns (a long worker, e.g. one
    // that starts long on Slithery Fight, used to keep 2 and give the rest). Split escapes
    // (endgame.hpp) and the queen's seeding keep their sizes. false: the old worker sizes.
    constexpr bool SPLIT_TWO_FROM_TAIL = true;
    // Workers leave this many unit slots free, so a trapped long dragon can always
    // split-escape (a split at the unit limit is illegal).
    constexpr int ESCAPE_UNIT_RESERVE = 2;
    // v21: a worker this long may also take the slot kept for long dragons (only the
    // king's last slot stays free). At the unit cap, boxed-in workers used to die whole:
    // ~140 deaths of 10+ dragons per 200 rounds on a big map. Build with
    // -DLONG_WORKER_ESCAPE=1000 to switch it off.
#ifndef LONG_WORKER_ESCAPE
#define LONG_WORKER_ESCAPE 8
#endif
    constexpr int LONG_WORKER_ESCAPE_LENGTH = LONG_WORKER_ESCAPE;
    constexpr int KNOWN_FREE_MEMORY = 4, OCCUPIED_MEMORY = 2;
    // v21: a body seen on a tile is remembered for as long as it will really be there
    // (memory_.busy_until): a segment we can place (its head in sight) until its dragon
    // has moved past it (an ally at half its free steps a turn, anyone else one step a
    // turn; +1 in case it eats); a segment of an ally whose length we know but whose head
    // we cannot see, half its length at that pace (at most UNPLACED_BODY_MEMORY_MAX);
    // anything else OCCUPIED_MEMORY rounds as before. Never longer than BODY_MEMORY_MAX.
    constexpr int BODY_MEMORY_MAX = 30, UNPLACED_BODY_MEMORY_MAX = 8;
    constexpr int CROWD_RADIUS = 2;

    // --- v20: scoring in segments (scoring.hpp) ---
    // Every move value is in thousandths of a segment: eating a pearl is +1000.
    // What a unit is worth beyond its segments (it eats, splits, blocks). Mode 2 values
    // their units above ours (eliminations), mode 3 ours above theirs (survival). So an
    // even trade gains a segment in mode 2 and loses one in mode 3.
#if AGGR_MODE == 2
    constexpr double UNIT_VALUE_OURS = 2.0, UNIT_VALUE_THEIRS = 3.0;
#else
    constexpr double UNIT_VALUE_OURS = 3.0, UNIT_VALUE_THEIRS = 2.0;
#endif
    // The king's and growers' segments count this much more: they decide the tiebreak.
    constexpr double KEEPER_LENGTH_WEIGHT = 1.5;
    // From ENDGAME_HUNT_ROUND, killing their long dragon (their tiebreak) is worth this much more.
    constexpr double ENDGAME_KILL_BONUS = 3.0;
    // Food. A pearl next to a grower is the grower's: a worker taking it gains this much,
    // a feeder must not; the king and growers yield it to a longer one.
    constexpr int PEARL_VALUE = 1000;
    // v23: a split-escape child's first turns (main.cpp escape_child_holding): no split for
    // this many turns, and on the first a move that leaves our tail in place costs this.
    constexpr int ESCAPE_CHILD_HOLD_TURNS = 3, ESCAPE_CHILD_STALL_COST = 5000;
    constexpr int RESERVED_PEARL_WORKER = 280, RESERVED_PEARL_KEEPER = 530, RESERVED_PEARL_FEEDER = -1400;
    constexpr int POCKET_NET_PEARL = 700; // per pearl above the 2 a pocket raid's split costs
    // Chances of dying (each times what our death would cost).
    constexpr double P_NO_EXIT = 0.8;                                                               // no free tile next to where we end
    constexpr double P_ONE_EXIT = 0.01;                                                             // one
    constexpr double P_SPACE = 0.3;                                                                 // room short of our body + 3, scaled by the shortfall
    constexpr double P_DEAD_END = 0.8;                                                              // a closed region with no loop (times the cost of splitting out)
    constexpr double P_ADJACENT_ENEMY = 0.02;                                                       // an enemy head next to it, outside a fight
    constexpr double P_HARD_THREAT = 0.5;                                                           // valuable: inside an enemy's sprint reach (its proven length)
    constexpr double P_SOFT_THREAT = 0.03;                                                          // valuable: inside it if its hidden body is typical
    constexpr double P_NO_ROBUST_EXIT = 0.013, P_ONE_ROBUST_EXIT = 0.004, P_CROWD_SEGMENT = 0.0002; // valuable: crowds
    constexpr double P_KNOWN_SPACE_CELL = 0.002, P_KNOWN_SPACE_CAP = 0.08;                          // valuable: tail through vouched tiles
    // Portals: an exit we cannot see holds a body about as often as the map is full
    // (local tests: 30-50% of blind crossings died after round 50). Early on (the
    // portal window) workers cross unknown portals as ordinary moves.
    // (v24: a blind crossing's chance is blind_portal_p(), by round; it was P_BLIND_PORTAL = 0.3,
    // or P_SEEN_FREE_PORTAL = 0.1 when we had just seen the far tile free.)
    constexpr double P_OCCUPIED_PORTAL = 0.8, P_WINDOW_PORTAL = 0.05;
    constexpr int PORTAL_EXPLORE_VALUE = EARLY_PORTAL_PULL ? 800 : 180;  // crossing an unknown portal in the window (v26: any goal; was 180, explore only)
    constexpr int PORTAL_REGION_PER_PEARL = 48, PORTAL_REGION_CAP = 360; // richer far side, per expected pearl
    constexpr int PORTAL_LOOP = 240;
    // Goal pulls, per step of progress.
    constexpr int STEP_EAT = 180, ROUTE_TIE = 80;          // along the pearl route (its checked first step: ties)
    constexpr int STEP_EXPLORE = 180, STEP_DISPERSE = 120; // to the nearest objective; to the dispersal destination
    constexpr int STEP_HUNT = 180, LATE_STEP_HUNT = 340;   // to the enemy target (from FEED_START_ROUND)
    // v22: the centre rush; v23: by map. On the official maps where top teams take the middle (listed
    // below by size: the bot knows the map's width and height from its first turn), every
    // dragon heads for the map centre for the first CENTRE_RUSH_END_ROUND rounds:
    // CENTRE_PULL per step of path closer (kelp and portals as we know them), until within
    // CENTRE_RADIUS steps (short side / CENTRE_RADIUS_DIV, at least 2) so we do not pile up
    // on one tile. A size not in the list (Autarky 54x18, the larger maps, any map we have
    // never seen) gets no centre rush. CENTRE_MAP_TABLE_ENABLED = false turns it off on
    // every map.
    constexpr bool CENTRE_MAP_TABLE_ENABLED = true;
    struct CentreMap
    {
        int width, height;
    };
    constexpr CentreMap CENTRE_MAPS[] = {
        {11, 11}, // Arena
        {16, 8},  // Small
        {16, 16}, // Colosseum, Default Small
        {24, 12}, // Stripes
        {32, 16}, // Devil, Portals, Prisoners Dilemma, Tower Defense
        {40, 15}, // Weakhold
        {25, 25}, // Trophy
    };
    constexpr int CENTRE_RUSH_END_ROUND = 75;
    constexpr int CENTRE_PULL = 300, CENTRE_RADIUS_DIV = 4;
    // Habits with no physical meaning: the old weights / 2.5 (a pearl used to be 2500).
    constexpr int VISIT_COST = 11, KEEPER_VISIT_COST = 2;                    // per earlier visit of the tile
    constexpr int EXIT_VALUE = 22, ROOM_CELL_VALUE = 3;                      // per free neighbour; per tile of room (to body + 5)
    constexpr int ALLY_CONGESTION = 120;                                     // per ally head next to the tile
    constexpr int BLOCKED_SPAWN = 140;                                       // our body on a tile when it spawns wastes the pearl
    constexpr int ENEMY_DISTANCE_STEP = 12, KEEPER_ENEMY_DISTANCE_STEP = 22; // per step away from visible enemies
    constexpr int SPRINT_STEP_COST = 20;                                     // attack sprints: per extra step (ties only)
    constexpr int CONTEST_VALUE = 100;                                       // per pearl a sprint takes from under an enemy's nose
    // --- v21: survival search (survival.hpp) ---
    // From where a move ends, search our own next moves for a way to keep going for
    // our length + DFS_EXTRA_DEPTH moves (at most DFS_MAX_DEPTH). At most
    // DFS_NODE_BUDGET positions per candidate and DFS_TURN_BUDGET per turn. A body
    // remembered out of sight is a wall for DFS_REMEMBERED_STEPS moves.
    constexpr int DFS_EXTRA_DEPTH = 2, DFS_MAX_DEPTH = 20;
    constexpr int DFS_NODE_BUDGET = 4000, DFS_TURN_BUDGET = 40000;
    constexpr int DFS_REMEMBERED_STEPS = 3;
    constexpr int DFS_OTHER_MIN_WAIT = 2; // another dragon's segment is a wall at least for our first move
    // A complete search that falls short: this chance, times the share of the target
    // missed, that we must split out (or die, when too short to split).
    constexpr double P_DFS_SHORT = 0.9;
    // v21: the king and growers search all the way: a way on for their whole length + 2
    // moves (at most KING_DFS_MAX_DEPTH), since their own newest segments stay put for
    // that long. 20 moves proved nothing for a king of 200 walking into a ring of its
    // own body. Bigger budgets; another dragon's body we cannot place (its head out of
    // sight) is assumed gone after DEEP_UNPLACED_BODY moves rather than never.
    constexpr int KING_DFS_MAX_DEPTH = 300, KING_DFS_NODE_BUDGET = 20000, KING_DFS_TURN_BUDGET = 250000;
    constexpr int DEEP_UNPLACED_BODY = 40;
    // A dragon that does not know all of its body (an heir born long from a split) has the
    // rest of it attached where its known body ends: in deep searches, while some of it
    // is still there, a tile it has never seen within that many steps of that end (at
    // most UNKNOWN_BODY_RADIUS) may be its own body.
    constexpr int UNKNOWN_BODY_RADIUS = 10;
    // --- v21: the king's plan (survival.hpp) ---
    // The king and growers weigh each of their best PLAN_CANDIDATES moves by what the
    // next turns can bring after it: for turns 2..PLAN_TURNS, the PLAN_BRANCH best moves
    // of up to our free steps (at most PLAN_MAX_STEPS) by food on the way and progress
    // along the route, each played out exactly against our own body. A plan counts only
    // if, where it ends, the deep survival search finds a way on for our whole length.
    // The food the best such plan reaches in its later turns, discounted PLAN_DISCOUNT a
    // turn, is added to the move (progress is the move's own score's business).
    constexpr int PLAN_TURNS = 3, PLAN_BRANCH = 4, PLAN_MAX_STEPS = 24, PLAN_CANDIDATES = 8;
    constexpr double PLAN_DISCOUNT = 0.85;
    constexpr int PLAN_DFS_TURN_BUDGET = 150000;
    // --- v20: fights ---
    // An enemy head this many steps away or closer (at most FIGHT_MAX_ENEMIES of them)
    // makes the turn a fight: each move is played out against its replies.
    constexpr int FIGHT_RADIUS = 4, FIGHT_MAX_ENEMIES = 2;
    constexpr bool FIGHT_BY_PATH = true;        // v26.1.0: an enemy is near by path, round walls (false: straight-line distance)
    constexpr double FIGHT_WORST_WEIGHT = 0.7;  // its replies: this much of the worst for us, the rest the average
    constexpr double FIGHT_BASE_PULL = 0.5;     // the goal we would otherwise have still pulls this much
    constexpr double P_RAM_PROFITABLE = 0.5;    // it steps onto our head when the trade pays for it
    constexpr int FIGHT_SPRINT_REACH = 3;       // its replies include sprints into our head this long
    constexpr double P_TRAPPED_KILL = 0.6;      // it dies when left less room than its length
    constexpr int FIGHT_PRESSURE_PER_CELL = 20; // per tile of room taken from it (to its length + 3)
    // --- v21: free moves (game update: ceil(L/4) steps a turn cost nothing) ---
    // Besides single steps, every turn we also weigh multi-step moves of up to our free
    // steps (at most FREE_MOVE_MAX_STEPS, all through tiles we can see, no portals):
    // the FREE_MOVE_EVALS most promising end tiles are evaluated in full like a step.
    constexpr int FREE_MOVE_MAX_STEPS = 6, FREE_MOVE_EVALS = 6;
    // Valuable dragons: a move of several steps can end at the edge of our window, next
    // to a head we cannot see (a single step never ends closer than 2 to its edge).
    // Each tile within UNSEEN_RADIUS steps of where we end that we cannot see now is
    // this chance, times the enemy heads we see per tile (running average), that one
    // there rams us; at most P_UNSEEN_CAP in all.
    constexpr int UNSEEN_RADIUS = 3;
    constexpr double P_UNSEEN_ATTACK = 0.5, P_UNSEEN_CAP = 0.3;
    // Any dragon this long: a head we cannot see (ally or enemy) may also box us in
    // there before our next move, whatever the survival search said: per unseen tile,
    // P_UNSEEN_BOX times the heads we see per tile, times what a trap costs us. (In
    // tests, long dragons boxed in right after a "safe" move: 1.2% of multi-step moves
    // against 0.1% of single steps; the walls were nearly all allies.)
    constexpr int UNSEEN_BOX_MIN_LENGTH = 10;
    constexpr double P_UNSEEN_BOX = 0.05;
    // --- v21: routing (route.hpp) ---
    // Arrival times: in turns at our free speed; the discount is per turn.
    // A pearl an enemy reaches in fewer turns than we do is worth this much.
    constexpr double ROUTE_CONTEST_LOST = 0.4;
    constexpr int ETA_RADIUS = 30; // steps searched from each visible head for who gets where first
    // The route must leave us room: from where it ends, the survival search
    // (survival.hpp) runs with its own budget, so routing never starves the moves.
    constexpr int ROUTE_DFS_TURN_BUDGET = 24000, KING_ROUTE_DFS_TURN_BUDGET = 80000;

    // v22: the judge meters each turn in CPU points (100M; a turn over it delivers
    // nothing and the dragon dies, "no valid action"), and its clock counts those points
    // (1 ns = 1 point), so the bot reads its own spending off the clock. Each search
    // stops at its phase's deadline, counted from the turn's start: the route's checks
    // by ROUTE_STOP, the moves' survival searches by MOVES_STOP, the king's plans by
    // PLAN_STOP, anything else by SEARCH_STOP. What follows (choosing, the output: 2.5M
    // plus 4,000 a byte) fits in the rest. Node budgets still spread a phase's points
    // over its searches; the deadlines make sure the turn is never over.
    constexpr long long TURN_POINTS = 100'000'000;
#ifdef BOT_UNLIMITED
    // (tests only: no deadlines)
    constexpr long long ROUTE_STOP = 1LL << 60, MOVES_STOP = 1LL << 60, PLAN_STOP = 1LL << 60, SEARCH_STOP = 1LL << 60;
#else
    // Measured (metered replays; the sandbox agrees): the plan overshoots its stop by under
    // 1M, and the rest of the turn after it (choosing, the 2.5M write) costs under 4M, so a
    // turn peaks near PLAN_STOP + 4M = 95M. SEARCH_STOP (searches after the plan, such as a
    // split escape) stays just above PLAN_STOP for the same reason.
    constexpr long long ROUTE_STOP = 40'000'000, MOVES_STOP = 66'000'000, PLAN_STOP = 91'000'000,
                        SEARCH_STOP = 92'000'000;
#endif
    constexpr long long SEARCH_MIN_POINTS = 2'000'000; // one survival search's share: at least this
    // Native builds (local tests) have no meter: their clock's nanoseconds count as
    // this many points (measured against the metered build).
    constexpr long long NATIVE_POINTS_PER_NS = 24;
    // Rush (route.hpp): follow the checked route path for all our free steps, through
    // tiles we cannot see now but have seen free (an edge we know, no body remembered on
    // it). Each such tile seen k rounds ago is taken by now with chance k x the measured
    // fill rate, but never more than the share of tiles we see taken (nor
    // P_BLIND_TILE_CAP); running into it is death. A rush riskier than RUSH_MAX_RISK
    // (valuable dragons: RUSH_MAX_RISK_VALUABLE) is not safe.
    constexpr double P_BLIND_TILE_CAP = 0.5, RUSH_MAX_RISK = 0.08, RUSH_MAX_RISK_VALUABLE = 0.02;
    constexpr int ROUTE_REPLAY_SLACK = 8; // a replay may take this many steps more than planned

    // --- v26.3.0: from Leo's v27.2.0 (V25-based), the parts on by default there, each behind a switch ---
    // Late army (his idea 2): KEEP_ARMY_PERMIL / 1000 of the workers (a stable hash of the id) do not
    // feed from GATHER_ROUND until ARMY_UNTIL: they harvest, hunt and explore as usual, skip the late
    // rendezvous, and may keep splitting until ARMY_SPLIT_END. The rest feed the king as before.
    constexpr bool KEEP_ARMY = true;
    constexpr int KEEP_ARMY_PERMIL = 500;
    constexpr int ARMY_UNTIL = 440;
    constexpr int ARMY_SPLIT_END = 470;
    // The queen and portals (his safety A): from QUEEN_NO_PORTAL_FROM, while the water she can reach
    // without a portal (known kelp blocks, bodies ignored) is QUEEN_PORTAL_ROOM tiles or more, a move
    // through any portal is unsafe for her and costs QUEEN_PORTAL_COST. (Our keep-out, PORTAL_KEEPOUT,
    // covers portals into the unknown all game.)
    constexpr bool QUEEN_NO_PORTAL = true;
    constexpr int QUEEN_PORTAL_COST = 40000;
    constexpr int QUEEN_PORTAL_ROOM = 64;
    constexpr int QUEEN_NO_PORTAL_FROM = 100;
    // The queen waits (his v25.1 and safety B): certainly dead, she detaches 2 and keeps her place
    // (endgame.hpp play_split_escape; was: her rear L-2 escape and she dies). QUEEN_STALL: she also
    // waits when every move is hopeless or ends in a certain enemy reach (or crosses a portal, as
    // above) while her own tile is out of reach and no enemy head is next to her.
    constexpr bool QUEEN_WAIT_SPLIT = true;
    constexpr bool QUEEN_STALL = true;
    // Ally box (his v27.1 idea 1): a move that takes every free side left to a teammate's head costs
    // ALLY_BOX_P of that teammate's death; all but one, ALLY_SIDE_COST (scoring.hpp ally_box_cost).
    constexpr bool ALLY_BOX = true;
    constexpr double ALLY_BOX_P = 0.7;
    constexpr int ALLY_SIDE_COST = 300;
    // Early trades (his v27.2): before AGGRESSION_START_ROUND a worker (not the queen, not valuable) may
    // trade into an enemy as long as it or longer while we have EARLY_TRADE_MIN_UNITS units.
    constexpr bool EARLY_TRADE = true;
    constexpr int EARLY_TRADE_MIN_UNITS = 3;
}

namespace
{

    using unswbc::Direction;
    using unswbc::EdgeType;
    using unswbc::Position;

    constexpr int INF = std::numeric_limits<int>::max() / 4;

    // v21 movement rule (game update). In one turn a dragon of length L (its length at
    // the start of the turn) may take ceil(L/4) steps for free. Every step after those
    // costs a segment (its tail loses one more), and is only allowed while it is longer
    // than 2. Lengths 1-4: 1 free step (the old rule), 5-8: 2, 9-12: 3, 20: 5.
    constexpr int free_moves(int length) { return std::max(1, (length + 3) / 4); }
    constexpr int paid_steps(int length, int steps) { return std::max(0, steps - free_moves(length)); }
    // The most steps it can take in one turn (eating nothing on the way).
    constexpr int max_turn_steps(int length) { return free_moves(length) + std::max(0, length - 2); }
    // Turns to take `steps` steps at full free speed.
    constexpr int turns_for(int steps, int length)
    {
        return steps <= 0 ? 0 : (steps + free_moves(length) - 1) / free_moves(length);
    }

    int direction_index(Direction direction)
    {
        switch (direction.value)
        {
        case Direction::NORTH:
            return 0;
        case Direction::EAST:
            return 1;
        case Direction::SOUTH:
            return 2;
        default:
            return 3;
        }
    }

    struct RememberedEdge
    {
        bool known = false;
        EdgeType type = EdgeType::EMPTY;
        int portal_id = -1;
    };

    struct CellMemory
    {
        bool seen = false;
        int last_seen = -1;
        int visits = 0;
        bool pearl = false;
        int pearl_seen = -1;
        int pearl_time = -1;
        int pearl_sample_round = -1;
        int cd_sum = 0, cd_n = 0; // v23: pearl countdowns seen here (a bed's respawn rate, see bed_gap)
        bool other_seen = false;  // another dragon's segment was here when last seen
        int busy_until = -1;      // v21: ...and is expected to be gone from this round on
        std::array<RememberedEdge, 4> edges{};
    };

    struct EdgeKey
    {
        bool horizontal = false;
        int x = 0;
        int y = 0;
        constexpr bool operator==(EdgeKey const &) const = default;
    };

    struct Step
    {
        bool passable = false;
        bool destination_known = false;
        bool portal = false;
        Position destination{};
    };

    struct SeenHead
    {
        int id = -1;
        unswbc::Team team = unswbc::Team::A;
        Position position{};
        Direction facing = Direction::NORTH;
        int visible_length = 1;
        int lower_length = 1;  // max(traced body, visible parts): a guaranteed minimum
        bool truncated = true; // the body may continue outside our window
        bool queen = false;    // v22: dragon 0 or 1 (raw id)
        int estimated_length() const { return lower_length + (truncated ? 2 : 0); }
    };

    struct TeamSignal
    {
        int id = -1;
        Position position{};
        int length = 0;
        int round = 0;
        bool grower = false;
    };

    // What a time-aware space search found besides its size.
    struct SpaceStats
    {
        int cells = 0;        // tiles reached
        int edges = 0;        // moves possible between tiles reached (a tree has cells - 1)
        int pearls = 0;       // pearls in the region (visible, or seen very recently)
        int blockers = 0;     // times another dragon bounded the region (it may move)
        bool unknown = false; // touches never-seen tiles or a portal we cannot follow
        bool capped = false;  // the search stopped at its cap: plenty of room
        // Neither big nor bounded by anything that moves: a dead end made of kelp
        // (and our own body) that we can see all of.
        bool closed() const { return !capped && !unknown && blockers == 0; }
        // A closed region with no loop in it (a 1-wide dead end, however deep, or a
        // tree of them): the head always ends at a leaf with its body behind it.
        bool dead_end() const { return closed() && edges < cells; }
        // Any closed region we can never turn around in: a dead end, or one too small.
        bool trap(int length) const { return dead_end() || (closed() && cells < length + 2); }
    };

    struct Candidate
    {
        std::vector<Direction> steps;
        Position destination{};
        bool destination_known = true;
        bool portal = false;
        bool unknown_portal = false;
        bool head_trade = false;
        bool direct_pearl = false;
        bool safe = false;
        bool critical_ok = false;  // no reach threat, an exit, and time-aware room for the body
        bool pocket_plan = false;  // v13: a dead end worth raiding (eat, then split-reverse out)
        bool queen_cycle = false;  // v22: the queen's step(s) along her cycle
        bool walled = false;       // v23: the survival search proves it a dead end even with no other head moving in
        bool via_portal = false;   // v26: its only way on found is a portal we have never looked through
        bool pearl_sprint = false; // v14: a sprint to a contested pearl
        bool free_move = false;    // v21: several steps within our free steps
        bool rush = false;         // v21: ...along the route, past what we can see
        bool route_portal = false; // v23: a portal crossing our goal's path goes through
        int plan_value = 0;        // v21: what the king's best safe plan after this move adds (-1: none found)
        // v21 survival search from where the move ends (survival.hpp): moves survived,
        // the target, whether the search was complete (-1: not searched).
        int dfs_depth = -1, dfs_target = 0;
        bool dfs_complete = false;
        bool dfs_ok() const { return dfs_depth >= 0 && dfs_depth >= dfs_target; }
        bool dfs_dead() const { return dfs_depth >= 0 && dfs_complete && dfs_depth * 4 < dfs_target * 3; }
        SpaceStats region; // of the time-aware space search from the destination
        int enemy_threats = 0;
        int ally_threats = 0;
        int hard_threats = 0;
        int soft_threats = 0;
        int exits = 0;
        int space = 0;
        int known_space = -1;
        int new_length = 0;
        int score = -INF;
    };

    // The body after a hypothetical move: the head path p1..pk followed by the old body.
    struct PostMove
    {
        Position head{};
        int length = 0;
        int steps = 0;
        int eaten = 0;
        std::vector<Position> path;
    };

    struct BodyTrace
    {
        std::vector<Position> segments; // head first
        bool complete = false;          // the last segment is provably the tail
    };

    class ReplayDerivedBot
    {
    public:
        ReplayDerivedBot(unswbc::Controller &controller, unswbc::Game &game_state)
            : controller_(controller), game_(game_state), width_(game_state.width),
              height_(game_state.height),
              memory_(static_cast<std::size_t>(width_ * height_)),
              enemy_threats_(memory_.size(), 0), ally_threats_(memory_.size(), 0),
              hard_threat_(memory_.size(), 0), soft_threat_(memory_.size(), 0),
              own_idx_(memory_.size(), -1), other_occ_(memory_.size(), 0),
              objective_dist_(memory_.size(), INF), trap_mark_(memory_.size(), 0),
              stamp_(memory_.size(), 0), best_(memory_.size(), INF), done_(memory_.size(), 0),
              parent_(memory_.size(), -1), parent_dir_(memory_.size(), 0) {}

        // v21: dragon ids as our packets carry them (12 bits). Ids pass 4096 in long games
        // with many splits (both teams share the counter); every id we store or compare
        // with one from a packet is kept in this form, or feeders lose their king.
        static int pid(int id) { return id & 4095; }
        int my_id() const { return pid(controller_.get_id()); }

        // v22: the turn's clock, in points (see Config::TURN_POINTS).
        static long long clock_now()
        {
            long long const ns = std::chrono::steady_clock::now().time_since_epoch().count();
#if defined(__wasm__) || defined(__wasm32__)
            return ns; // the judge's clock counts points
#else
            return ns * Config::NATIVE_POINTS_PER_NS;
#endif
        }
        void start_turn_clock(long long t0)
        {
            turn_t0_ = t0;
            search_deadline_ = t0 + Config::SEARCH_STOP;
            phase_bit_ = 8;
            cut_flags_ = 0;
            nbr_valid_ = false; // edges may be learned before the table is refreshed
        }

#ifdef BOT_PROFILE
        // Points spent per phase (the judge's clock counts points, 1 ns = 1 point).
        std::array<long long, 20> prof_{};
        long long prof_last_ = 0;
        static long long prof_now() { return clock_now(); }
        std::array<long long, 8> sub_{};
        mutable std::array<long long, 8> subc_{};
        struct SubTimer
        {
            long long &acc;
            long long t0;
            SubTimer(long long &a) : acc(a), t0(prof_now()) {}
            ~SubTimer() { acc += prof_now() - t0; }
        };
        void prof_begin()
        {
            prof_.fill(0);
            subc_.fill(0);
            prof_last_ = prof_now();
        }
        void prof(int slot)
        {
            long long const n = prof_now();
            prof_[static_cast<std::size_t>(slot)] += n - prof_last_;
            prof_last_ = n;
        }
        void prof_dump()
        {
            std::string s;
            for (long long v : prof_)
                s += std::to_string(v / 1000) + " ";
            s += "| ";
            for (long long v : subc_)
                s += std::to_string(v / 1000) + " ";
            s += "| " + std::to_string(surv_turn_nodes_) + " " + std::to_string(surv_route_nodes_) + " " + std::to_string(surv_plan_nodes_) +
                 " " + std::to_string(cut_flags_);
            controller_.output_log("PROF", s, king_fresh() && king_.id == my_id() ? "K" : "-");
        }
#define PROF(n) prof(n)
#define SUBPROF(k) SubTimer subtimer_##k(const_cast<long long &>(subc_[k]))
#else
#define PROF(n)
#define SUBPROF(k)
#endif
        void play_turn()
        {
            if (first_round_ < 0)
            {
                // Works whether the first round is numbered 0 or 1. A restarted or
                // grower-escape process is born long, so it keeps the grower role.
                first_round_ = game_.get_round_num();
                founder_ = first_round_ <= 1 &&
                           controller_.get_length() == unswbc::Constants::INITIAL_LENGTH;
                heir_ = controller_.get_length() >= Config::HEIR_MIN_BIRTH_LENGTH;
                birth_length_ = controller_.get_length();
                initialized_ = true;
            }
            receive_status();
            remember_vision();
            refresh_neighbours();      // v22: the edges are known for this turn
            note_unexplored_portals(); // v26
            prepare_centre();          // v22: the centre rush on small maps (scoring.hpp)
#ifdef NBR_CHECK
            nbr_check("start");
#endif
            update_body_model();
            observe_heads();
            remember_bodies(); // v21: how long each body we see will stay
            compute_roles();
            update_king_info();
            build_threat_maps();
            update_coordination();
            PROF(1);
#ifdef BOT_DEBUG
            if (valuable_now_)
            {
                std::string b;
                for (Position const &q : own_trail_)
                    b += std::to_string(q.x) + "," + std::to_string(q.y) + ";";
                controller_.output_log("BODY", b);
            }
            controller_.output_log("ROLE", retaining_now_, valuable_now_, heir_, known_body_, controller_.get_length(),
                                   feeder_target_ ? feeder_target_->id : -1, king_fresh() ? king_.id : -1, king_.length);
            if (feeding_role() && game_.get_round_num() >= Config::SACRIFICE_ROUND)
            {
                char const *state = !king_fresh() ? "noking" : !king_head_estimate_ ? "nohead"
                                                           : !feeder_target_        ? "notarget"
                                                           : seen_head(king_.id)    ? "near"
                                                                                    : "far";
                int d = king_head_estimate_ ? distance(controller_.get_position(), *king_head_estimate_) : -1;
                controller_.output_log("FSTATE", state, d / 5 * 5, good_sacrifice_spot() ? 1 : 0,
                                       game_.get_round_num() - king_.obs, king_.length,
                                       "aim", feeder_target_ ? feeder_target_->position.x : -1, feeder_target_ ? feeder_target_->position.y : -1,
                                       "units", controller_.get_unit_count());
            }
            if (king_.id == my_id())
                controller_.output_log("IAMKING", controller_.get_length(), "units", controller_.get_unit_count(),
                                       "fill", fill_rate_x1e4_, "occ", occupancy_x1e4_);
            if (valuable_now_)
                controller_.output_log("BODYK", known_body_, controller_.get_length(), game_.get_round_num() - first_round_,
                                       birth_length_, dbg_resets_, dbg_truncs_, dbg_mem_links_);
#endif

            if (play_queen_kill()) // v23: the enemy queen within reach: kill her, first of all
                return;
            if (play_clear_remnant())
                return;
            if (play_feeder_strategy())
                return;

            PROF(2);
            CellMemory &current = cell(controller_.get_position());
            ++current.visits;

            update_explore_target();
            prepare_survival(); // v21: when other dragons' bodies move on (survival.hpp)
            PROF(3);
            prepare_etas(); // v21: who reaches which tile first (route.hpp)
            PROF(4);
            build_objective_map();
            update_portal_room(); // v23
            PROF(5);
            set_deadline(Config::ROUTE_STOP); // v22: each phase's searches stop at its deadline
            plan_route();
            set_deadline(Config::MOVES_STOP);
            PROF(6);
            choose_goal(); // v20: one goal for this turn (scoring.hpp)
            PROF(7);
#ifdef BOT_DEBUG
            controller_.output_log("GOAL", goal_name(goal_), goal_name(base_goal_));
            controller_.output_log("FARM", dbg_good_beds_, dbg_good_expired_);
            if (stuck_in_room_)
                controller_.output_log("ROOM_STUCK");
            dbg_good_beds_ = dbg_good_expired_ = 0;
            if (route_first_ >= 0 && valuable_now_)
            {
                Position const tp = position_of(route_cells_[static_cast<std::size_t>(route_first_)]);
                controller_.output_log("ROUTE", route_planned_, "to", tp.x, tp.y, "rest", route_remaining_now_,
                                       "cands", static_cast<int>(route_cells_.size()), "end_dfs", route_end_depth_);
            }
#endif
            std::vector<Candidate> candidates = single_step_candidates(false);
            if (candidates.empty())
                candidates = single_step_candidates(true);
            PROF(8);
            add_tactical_sprints(candidates);
            PROF(9);
            add_pearl_sprints(candidates);
            PROF(10);
            add_free_moves(candidates); // v21 (route.hpp)
            PROF(11);
            add_route_rush(candidates); // v21 (route.hpp)
            PROF(12);
            add_escape_sprints(candidates);
            PROF(13);
#ifdef NBR_CHECK
            nbr_check("mid");
#endif
            set_deadline(Config::PLAN_STOP);
            plan_king_moves(candidates); // v21 (survival.hpp)
            set_deadline(Config::SEARCH_STOP);
            PROF(14);
            apply_known_space_penalties(candidates);
            apply_portal_keepout(candidates);  // v26
            apply_queen_no_portal(candidates); // v26.3.0 (Leo's queen safety A)
            PROF(15);
            if (play_split_escape(candidates))
                return;
            PROF(16);

            if (should_split() && split_child_ok() && split_parent_ok() && !corridor_blocks_split() && !escape_child_holding())
            {
                int const child_size = worker_child_size();
                controller_.do_split(child_size);
                send_status(controller_.get_position(), controller_.get_length() - child_size);
                send_explore_handover(child_size);
                return;
            }

            if (escape_born_ && game_.get_round_num() == first_round_)
                for (Candidate &c : candidates)
                    if (!move_frees_tail(c))
                        c.score -= Config::ESCAPE_CHILD_STALL_COST;

            if (candidates.empty())
            {
                // Reverse into our own neck rather than kill an adjacent ally head.
                // The neck remains behind the facing direction even across portals.
                controller_.output_log("TRAPPED_SELF");
                controller_.make_move(controller_.get_dir().get_opposite());
                return;
            }

            bool const conservative = retaining_length() || controller_.get_length() >= 4;
            bool const has_safe = std::any_of(candidates.begin(), candidates.end(),
                                              [](Candidate const &move)
                                              { return move.safe; });
            Candidate const *best = nullptr;
            for (Candidate const &move : candidates)
            {
                // Approved trades are deliberate; they may override the safety filter.
                if (conservative && has_safe && !move.safe && !move.head_trade)
                    continue;
                if (!best || move.score > best->score)
                    best = &move;
            }
            if (!best)
                best = &*std::max_element(
                    candidates.begin(), candidates.end(),
                    [](Candidate const &a, Candidate const &b)
                    { return a.score < b.score; });
            // v22: the queen walks her cycle (queen.hpp).
            Candidate queen_move;
            if (queen_cycling())
                if (std::optional<Candidate> q = queen_cycle_choice(candidates))
                {
                    queen_move = *q;
                    best = &queen_move;
                }
#ifdef BOT_DEBUG
            if (is_queen())
                controller_.output_log("QUEEN", queen_cycling() ? "cycle" : queen_seeding() ? "seed"
                                                                                            : "free",
                                       static_cast<int>(qcycle_.size()), best->queen_cycle ? "on" : "off",
                                       static_cast<int>(best->steps.size()), retaining_now_, king_.id, king_.queen);
#endif

#ifdef BOT_DEBUG
#ifndef BOT_DEBUG_ALL_CAND
            if (valuable_now_)
#endif
                for (Candidate const &c : candidates)
                {
                    std::string path;
                    for (Direction d : c.steps)
                        path.push_back(static_cast<char>(d.value));
                    std::string nb;
                    for (Direction d : Direction::get_direction_list())
                    {
                        Step const st = step_from(c.destination, d);
                        if (!st.passable)
                        {
                            nb += 'K';
                            continue;
                        }
                        if (!st.destination_known)
                        {
                            nb += 'U';
                            continue;
                        }
                        int const n = index(st.destination);
                        if (other_occ_[static_cast<std::size_t>(n)])
                            nb += 'O';
                        else if (own_idx_[static_cast<std::size_t>(n)] >= 0)
                            nb += 'b';
                        else if (own_idx_[static_cast<std::size_t>(n)] == -2)
                            nb += 'q';
                        else
                            nb += 'F';
                    }
                    controller_.output_log("CAND", path, "nb", nb, "dest", c.destination.x, c.destination.y, "sc", c.score,
                                           "sp", c.space, "ksp", c.known_space, "ex", c.exits, "hard", c.hard_threats, "soft", c.soft_threats,
                                           "ok", c.critical_ok, "safe", c.safe,
                                           "reg", c.region.cells, c.region.edges, c.region.pearls, c.region.blockers,
                                           c.region.unknown ? "U" : "-", c.region.capped ? "C" : "-",
                                           c.region.trap(c.new_length) ? "TRAP" : "-", c.pocket_plan ? "POCKET" : "-",
                                           "dfs", c.dfs_depth, c.dfs_target, c.dfs_complete ? "C" : "-", "plan", c.plan_value);
                }
            if (best->pocket_plan)
                controller_.output_log("POCKET", best->region.pearls, "len", controller_.get_length(), "space", best->space);
#endif
            refuse_route_portals(candidates, *best);
            if (best->steps.size() == 1)
                controller_.make_move(best->steps.front());
            else
                controller_.make_moves(best->steps);
            if (best->steps.size() > 1)
                controller_.output_log(best->head_trade     ? "ATTACK_SPRINT"
                                       : best->pearl_sprint ? "PEARL_SPRINT"
                                       : best->rush         ? "RUSH"
                                       : best->free_move    ? "FREE_MOVE"
                                                            : "ESCAPE_SPRINT",
                                       static_cast<int>(best->steps.size()));
            note_portals_taken(best->steps);
            record_pending_path(best->steps);

            int projected_length = best->new_length;
            if (projected_length <= 0)
            {
                projected_length = controller_.get_length();
                if (best->direct_pearl)
                    ++projected_length;
                projected_length -= paid_steps(controller_.get_length(), static_cast<int>(best->steps.size()));
            }
            send_status(best->destination_known ? best->destination
                                                : controller_.get_position(),
                        std::max(2, projected_length));
        }

    private:
        // Growers retain length from round 250; small workers later feed or attack.
        static constexpr int SPECIALISE_ROUND = Config::SPLIT_CUTOFF;
        static constexpr int FINAL_GROWTH_ROUND = Config::FINAL_GROWTH_CUTOFF;
        static constexpr std::uint64_t STATUS_MAGIC = 0xC8ULL;
        static constexpr std::uint64_t SONAR_KEY = 0x6A09E667F3BCC909ULL;

        unswbc::Controller &controller_;
        unswbc::Game &game_;
        int width_;
        int height_;
        std::vector<CellMemory> memory_;
        std::map<int, std::vector<EdgeKey>> portal_edges_;

        // v22: the turn's clock (points since the turn started) and the current search
        // deadline (an absolute clock value).
        long long turn_t0_ = 0;
        mutable long long search_deadline_ = 0;
        long long spent() const { return clock_now() - turn_t0_; }
        mutable int cut_flags_ = 0; // (profiling) which phases ran out: 1 route, 2 moves, 4 plan, 8 other, 16 a search's share
        mutable int phase_bit_ = 8;
        bool out_of_time() const
        {
            bool const late = clock_now() > search_deadline_;
            if (late)
                cut_flags_ |= phase_bit_;
            return late;
        }
        void set_deadline(long long stop) const
        {
            search_deadline_ = turn_t0_ + std::min(stop, Config::SEARCH_STOP);
            phase_bit_ = stop == Config::ROUTE_STOP ? 1 : stop == Config::MOVES_STOP ? 2
                                                      : stop == Config::PLAN_STOP    ? 4
                                                                                     : 8;
        }

        // v22: step_from for every tile and direction, kept from turn to turn and
        // refreshed once the turn's edges are learned (only around tiles whose edges
        // changed; all of it when a portal end is learned): nbr_[4c + d] is the tile
        // stepped onto, NBR_WALL for kelp, NBR_BLIND for a portal whose other end we do
        // not know; nbr_portal_ marks portal crossings. The searches read it directly.
        static constexpr int NBR_WALL = -1, NBR_BLIND = -2;
        std::vector<int> nbr_;
        std::vector<std::uint8_t> nbr_portal_;
        std::vector<int> nbr_dirty_;
        bool nbr_all_dirty_ = true;
        bool nbr_valid_ = false;
        int nb(int c, int d) const { return nbr_[static_cast<std::size_t>(4 * c + d)]; }
        std::vector<SeenHead> heads_;
        std::vector<TeamSignal> signals_;
        std::vector<int> enemy_threats_;
        std::vector<int> ally_threats_;

        // v11: reach-based threats for valuable dragons. Hard: reachable given the
        // enemy's guaranteed length. Soft: reachable if its hidden body is long.
        std::vector<int> hard_threat_;
        std::vector<int> soft_threat_;

        // v11: own body model. own_idx_ holds the segment index (0 = head), -2 for a
        // visible own segment whose index is unknown, -1 otherwise.
        std::vector<int> own_idx_;
        std::vector<char> other_occ_;
        std::vector<int> touched_;
        int known_body_ = 0;
        std::vector<Position> pending_path_;
        bool pending_valid_ = false;

        // Per-turn path distance (in steps, plus source costs) to the nearest pearl objective.
        std::vector<int> objective_dist_;
        // v13: tiles leading into a trap we decided not to enter, and until which round
        // the objective map treats them as walls (so its pearls stop pulling us there).
        mutable std::vector<int> trap_mark_;

        // Running average (x16) of enemy lengths seen whole; sizes hidden bodies.
        int typical_enemy_x16_ = 4 * 16;
        int enemy_density_x1e4_ = 100; // v21: enemy heads per visible tile (x10000), running average
        int ally_density_x1e4_ = 100;  // v21: the same for ally heads
        int fill_rate_x1e4_ = 300;     // v21: free tiles taken by another dragon per round (x10000), running average
        int occupancy_x1e4_ = 2000;    // v21: tiles in view holding another dragon (x10000), running average

        // v11: roles
        int first_round_ = -1;
        int birth_length_ = 0;
        bool heir_ = false;
        bool retaining_now_ = false;
        bool valuable_now_ = false;

        // Scratch for searches (generation-stamped so nothing is cleared per call).
        mutable std::vector<int> stamp_;
        mutable std::vector<int> best_;
        mutable std::vector<int> done_;
        mutable std::vector<int> parent_;
        mutable std::vector<int> parent_dir_;
        mutable int generation_ = 0;

        // NEW: added symmetry deduction
        enum class SymmetryType
        {
            UNKNOWN,
            HORIZONTAL,
            VERTICAL,
            ROTATIONAL
        };
        SymmetryType confirmed_symmetry_ = SymmetryType::UNKNOWN;
        bool sym_h_ = true, sym_v_ = true, sym_r_ = true;

        Position mirror(Position p, SymmetryType type) const
        {
            if (type == SymmetryType::HORIZONTAL)
                return {wrap(width_ - 1 - p.x, width_), p.y};
            if (type == SymmetryType::VERTICAL)
                return {p.x, wrap(height_ - 1 - p.y, height_)};
            if (type == SymmetryType::ROTATIONAL)
                return {wrap(width_ - 1 - p.x, width_), wrap(height_ - 1 - p.y, height_)};
            return p;
        }

        Direction mirror_dir(Direction d, SymmetryType type) const
        {
            if (type == SymmetryType::HORIZONTAL)
            {
                if (d == Direction::EAST)
                    return Direction::WEST;
                if (d == Direction::WEST)
                    return Direction::EAST;
                return d;
            }
            if (type == SymmetryType::VERTICAL)
            {
                if (d == Direction::NORTH)
                    return Direction::SOUTH;
                if (d == Direction::SOUTH)
                    return Direction::NORTH;
                return d;
            }
            if (type == SymmetryType::ROTATIONAL)
            {
                return d.get_opposite();
            }
            return d;
        }

#include "coordination.hpp"
#include "endgame.hpp"
#include "explore.hpp"
#include "route.hpp"
#include "survival.hpp"
#include "queen.hpp"
#include "scoring.hpp"

        int wrap(int value, int size) const
        {
            value %= size;
            return value < 0 ? value + size : value;
        }

        int index(Position position) const
        {
            return position.y * width_ + position.x;
        }

        Position position_of(int cell_index) const
        {
            return Position(cell_index % width_, cell_index / width_);
        }

        CellMemory &cell(Position position)
        {
            return memory_[static_cast<std::size_t>(index(position))];
        }

        CellMemory const &cell(Position position) const
        {
            return memory_[static_cast<std::size_t>(index(position))];
        }

        int toroidal_axis_distance(int a, int b, int size) const
        {
            int const direct = std::abs(a - b);
            return std::min(direct, size - direct);
        }

        int distance(Position a, Position b) const
        {
            return toroidal_axis_distance(a.x, b.x, width_) +
                   toroidal_axis_distance(a.y, b.y, height_);
        }

        int chebyshev_distance(Position a, Position b) const
        {
            return std::max(toroidal_axis_distance(a.x, b.x, width_),
                            toroidal_axis_distance(a.y, b.y, height_));
        }

        static std::uint64_t mix(std::uint64_t value)
        {
            value += 0x9E3779B97F4A7C15ULL;
            value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
            value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
            return value ^ (value >> 31U);
        }

        bool selected_grower() const
        {
            // A mixed ID produces a stable role without creating map-wide stripes
            // from consecutive split-child IDs. Workers stay workers so they keep
            // splitting; the role only moves with the body (heir_) on a grower escape.
            return founder_ || heir_ || controller_.get_unit_count() <= 2 ||
                   (Config::GROWER_SELECTION == 0 ? mix(static_cast<std::uint64_t>(controller_.get_id()) ^ 0xD1B54A32D192ED03ULL) : static_cast<std::uint64_t>(controller_.get_id())) % Config::GROWER_DENOMINATOR < Config::GROWER_NUMERATOR;
        }

        // Latest (length, round) heard from each ally.
        mutable std::map<int, std::pair<int, int>> ally_length_cache_;

        void remember_ally_length(int id, int length, int round)
        {
            if (id < 2 && !queen_crowned())
                return; // v22: the queen is not a king candidate before her crown

            auto found = ally_length_cache_.find(id);
            if (found == ally_length_cache_.end() || found->second.second <= round)
                ally_length_cache_[id] = {length, round};
        }

        bool is_top_two_grower() const
        {
            // Demote only when two allies are clearly longer, using fresh reports.
            // Latest lengths (not maxima) so a dead or shrunken ally cannot outrank us.
            int const now = game_.get_round_num();
            int const mine = controller_.get_length();
            int stronger = 0;
            for (auto it = ally_length_cache_.begin(); it != ally_length_cache_.end();)
            {
                if (now - it->second.second > Config::LENGTH_CACHE_ROUNDS)
                {
                    it = ally_length_cache_.erase(it);
                    continue;
                }
                if (it->first != my_id() &&
                    it->second.first >= mine + Config::TOP_TWO_MARGIN)
                    ++stronger;
                ++it;
            }
            return stronger < 2;
        }

        // Election: the longest dragon we know of (ties to the lower id). With no
        // news of any king we stand ourselves; beacons spread every candidate and the
        // team converges on one. No margin, so near-equals always resolve.
        bool elected_king() const
        {
            int const L = controller_.get_length();
            if (queen_crowned())
            {
                // v22: the queen is the king while she lives.
                if (is_queen())
                    return true;
                if (king_fresh() && king_.queen)
                    return false;
            }
            if (L < Config::LATE_KING_MIN_LENGTH)
                return false;
            if (!king_fresh() || king_.id == my_id())
                return true;
            return king_beats(L, my_id(), king_.length, king_.id);
        }

        bool is_king_or_vice() const
        {
            int const L = controller_.get_length();
            if (L < Config::KING_MIN_LENGTH)
                return false;
            int const now = game_.get_round_num();
            int stronger = 0, top = L;
            bool king_counted = false;
            for (auto it = ally_length_cache_.begin(); it != ally_length_cache_.end();)
            {
                if (now - it->second.second > Config::LENGTH_CACHE_ROUNDS)
                {
                    it = ally_length_cache_.erase(it);
                    continue;
                }
                if (it->first != my_id() && it->second.first >= L + Config::TOP_TWO_MARGIN)
                {
                    ++stronger;
                    top = std::max(top, it->second.first);
                    king_counted = king_counted || it->first == king_.id;
                }
                ++it;
            }
            if (king_fresh() && king_.id != my_id() && king_.length >= L + Config::TOP_TWO_MARGIN)
            {
                if (!king_counted)
                    ++stronger;
                top = std::max(top, king_.length);
            }
            if (stronger == 0)
                return true;
            if (stronger == 1)
                return L >= Config::VICE_MIN_LENGTH && L * 100 >= top * Config::VICE_PERCENT;
            return false;
        }

        bool compute_retaining() const
        {
            int const round = game_.get_round_num();
            // v22: the queen keeps her length (and is the king from QUEEN_CROWN_ROUND);
            // only while seeding the swarm alone does she split like a founder.
            if (is_queen())
                return !queen_seeding();

            // From round 400 exactly one dragon retains: the elected king. Everyone
            // else becomes food.
            if (round >= Config::SACRIFICE_ROUND)
                return elected_king();

            if (round >= Config::FEED_START_ROUND && controller_.get_length() <= Config::FEEDER_MAX_LENGTH)
            {
                for (auto const &s : signals_)
                    if (s.grower && s.id != my_id() &&
                        s.length >= Config::FEED_MIN_GROWER_LENGTH && round - s.round <= 2 &&
                        distance(s.position, controller_.get_position()) <= 8)
                        return false;
            }
            if (round < SPECIALISE_ROUND)
                return false;
            // Hash-picked growers, plus whoever is the longest dragon we know of: a
            // worker that grew long at the unit cap becomes the king instead of a
            // length-3 grower picked at random.
            return selected_grower() || is_king_or_vice();
        }

        void compute_roles()
        {
            retaining_now_ = compute_retaining();
            // v23: the queen always, from round 0: her length is the first tiebreak, so she gets
            // the king's caution (reach threats, crowds, unseen heads, no blind portals, no hunts
            // or trades) even while she seeds the swarm.
            valuable_now_ = retaining_now_ || controller_.get_length() >= Config::VALUABLE_LENGTH || is_queen();
        }

        // Cached once per turn, after sonar and vision are read.
        bool retaining_length() const
        {
            return retaining_now_;
        }

        // v26.3.0 (Leo's late army): a worker that keeps working through the late game instead of feeding.
        bool army_worker() const
        {
            return Config::KEEP_ARMY && !is_queen() && !retaining_length() &&
                   game_.get_round_num() >= Config::GATHER_ROUND && game_.get_round_num() < Config::ARMY_UNTIL &&
                   mix(static_cast<std::uint64_t>(controller_.get_id()) ^ 0x8CB92BA72F3D8DD7ULL) % 1000 <
                       static_cast<std::uint64_t>(Config::KEEP_ARMY_PERMIL);
        }

        int worker_child_size() const
        {
            // The parent keeps at least two (v23: the queen QUEEN_SEED_KEEP); the child never
            // reaches heir length.
            if (Config::SPLIT_TWO_FROM_TAIL && !is_queen())
                return 2; // v26.2.0
            int const keep = is_queen() ? Config::QUEEN_SEED_KEEP : unswbc::Constants::MIN_SIZE;
            return std::min(controller_.get_length() - keep, Config::HEIR_MIN_BIRTH_LENGTH - 1);
        }

        int unit_cap() const
        {
            return std::min({Config::TARGET_SWARM_SIZE, Config::MAX_SWARM_SIZE, game_.get_unit_limit()});
        }

        bool should_split() const
        {
            if (is_queen() && !queen_seeding())
                return false; // v22: the queen never splits (unless seeding the swarm alone)
            if (is_queen() && controller_.get_length() < Config::QUEEN_SEED_KEEP + 2)
                return false;                                                                         // v23: she keeps QUEEN_SEED_KEEP and gives at least 2
            int const split_stop = army_worker() ? Config::ARMY_SPLIT_END : Config::SPLIT_STOP_ROUND; // v26.3.0 (army)
            if (game_.get_round_num() >= split_stop || retaining_length() || controller_.get_length() <= Config::MAX_PRE_GROWTH_LENGTH)
                return false;
            if (controller_.get_unit_count() >= unit_cap() - Config::ESCAPE_UNIT_RESERVE)
                return false;
            return controller_.can_split(worker_child_size());
        }

        // v16: workers explore through portals they have never seen through, early on.
        // v26: an end of a portal pair we have not seen both ends of, on a tile in view: a
        // worker exploring heads for it, and the centre rush gives way (scoring.hpp centre_pull).
        bool unexplored_portal_in_view_ = false;
        void note_unexplored_portals()
        {
            unexplored_portal_in_view_ = false;
            for (unswbc::Tile const &tile : controller_.get_tiles())
                for (Direction d : Direction::get_direction_list())
                {
                    unswbc::Edge const &edge = tile.get_edge(d);
                    if (!edge.is_portal())
                        continue;
                    auto const found = portal_edges_.find(edge.get_portal_id());
                    if (found == portal_edges_.end() || found->second.size() < 2)
                    {
                        unexplored_portal_in_view_ = true;
                        return;
                    }
                }
        }

        // v26: the queen, the king and the growers keep out of portals unless it is that or die.
        bool keeps_out_of_portals() const
        {
            return Config::PORTAL_KEEPOUT && (is_queen() || retaining_length());
        }

        // A move that takes a portal into the unknown: a pair nobody has seen both ends of,
        // or one whose far tile is out of sight. A portal onto a tile in view (a box of beds
        // beside the start, say) is a step like any other.
        bool crosses_portal(Candidate const &c) const
        {
            if (c.via_portal)
                return true;
            if (c.portal)
                return c.unknown_portal || !controller_.get_tile(c.destination);
            Position at = controller_.get_position();
            for (Direction d : c.steps)
            {
                Step const st = step_from(at, d);
                if (!st.passable)
                    return false;
                if (st.portal && (!st.destination_known || !controller_.get_tile(st.destination)))
                    return true;
                if (!st.destination_known)
                    return false;
                at = st.destination;
            }
            return false;
        }

        // v26.3.0 (Leo's queen safety A): from QUEEN_NO_PORTAL_FROM, with QUEEN_PORTAL_ROOM tiles of water
        // she can reach without a portal, every portal move is a last resort for the queen.
        void apply_queen_no_portal(std::vector<Candidate> &candidates) const
        {
            if (!Config::QUEEN_NO_PORTAL || !is_queen() || game_.get_round_num() < Config::QUEEN_NO_PORTAL_FROM ||
                queen_room_without_portals() < Config::QUEEN_PORTAL_ROOM)
                return;
            for (Candidate &c : candidates)
                if (path_crosses_portal(c.steps))
                {
                    c.safe = false;
                    c.score -= Config::QUEEN_PORTAL_COST;
                }
        }

        // Tiles reachable from our head by ordinary steps (no portal; known kelp blocks, bodies
        // ignored, unknown edges open), counted up to QUEEN_PORTAL_ROOM.
        int queen_room_without_portals() const
        {
            std::vector<int> seen{index(controller_.get_position())};
            std::vector<char> mark(memory_.size(), 0);
            mark[static_cast<std::size_t>(seen[0])] = 1;
            for (std::size_t k = 0; k < seen.size() && static_cast<int>(seen.size()) < Config::QUEEN_PORTAL_ROOM; ++k)
                for (int d = 0; d < 4; ++d)
                {
                    std::size_t const e = static_cast<std::size_t>(4 * seen[k] + d);
                    int const n = nbr_[e];
                    if (n < 0 || nbr_portal_[e] || mark[static_cast<std::size_t>(n)])
                        continue;
                    mark[static_cast<std::size_t>(n)] = 1;
                    seen.push_back(n);
                }
            return static_cast<int>(seen.size());
        }

        // Does this path (from our head) take a portal on any step?
        bool path_crosses_portal(std::vector<Direction> const &steps) const
        {
            Position p = controller_.get_position();
            for (Direction d : steps)
            {
                Step const st = step_from(p, d);
                if (st.portal)
                    return true;
                if (!st.passable || !st.destination_known)
                    return false;
                p = st.destination;
            }
            return false;
        }

        static bool lives_on(Candidate const &c)
        {
            return c.score > -INF && !c.head_trade && c.destination_known && !c.walled && c.hard_threats == 0 &&
                   (c.dfs_ok() || (!c.dfs_dead() && c.exits > 0));
        }

        void apply_portal_keepout(std::vector<Candidate> &candidates) const
        {
            if (!keeps_out_of_portals())
                return;
            bool other = false;
            for (Candidate const &c : candidates)
                if (lives_on(c) && !crosses_portal(c))
                    other = true;
            if (!other)
                return; // a portal may be the way out
            int const cost = death_cost();
            for (Candidate &c : candidates)
                if (c.score > -INF && crosses_portal(c))
                {
#ifdef BOT_DEBUG
                    controller_.output_log("PORTAL_KEEPOUT", c.destination.x, c.destination.y, c.score);
#endif
                    c.score -= cost;
                }
        }

        bool portal_exploring() const
        {
            return !retaining_length() && !valuable_now_ && game_.get_round_num() < Config::PORTAL_EXPLORE_END_ROUND;
        }

        // v17: a short dragon (length <= CORRIDOR_SPLIT_MAX_LENGTH) does not split while
        // it is in a 1-wide corridor: its head or its tail (where the child is born, and
        // becomes a head) on a tile with at most two sides free of kelp. Both halves would
        // be 2-3 long, and one would start in a passage it cannot turn around in.
        // Longer dragons split anywhere; split escapes (endgame.hpp) are not affected.
        bool corridor_tile(Position p) const
        {
            int open = 0;
            for (Direction d : Direction::get_direction_list())
                if (step_from(p, d).passable)
                    ++open;
            return open <= 2;
        }

        bool corridor_blocks_split() const
        {
            if (controller_.get_length() > Config::CORRIDOR_SPLIT_MAX_LENGTH)
                return false;
            if (corridor_tile(controller_.get_position()))
                return true;
            return body_complete() && !own_trail_.empty() && corridor_tile(own_trail_.back());
        }

        // v23: and so do we. The child moves right after the split, from our tail: a free
        // side of our head beside that tail it may take. Without another free side we
        // would be boxed in (the queen died this way on Slithery Fight's start).
        bool split_parent_ok() const
        {
            if (!body_complete() || own_trail_.empty())
                return true;
            Position const here = controller_.get_position();
            Position const child_head = own_trail_.back();
            int far = 0, near = 0;
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(here, d);
                if (!s.passable || !s.destination_known)
                    continue;
                std::size_t const c = static_cast<std::size_t>(index(s.destination));
                if (other_occ_[c] || own_idx_[c] != -1)
                    continue;
                bool next_to_child = false;
                for (Direction e : Direction::get_direction_list())
                {
                    Step const t = step_from(child_head, e);
                    if (t.passable && t.destination_known && t.destination == s.destination)
                        next_to_child = true;
                }
                ++(next_to_child ? near : far);
            }
            return far >= 1 || near >= 2;
        }

        // The child (born at our tail) needs somewhere to go at all.
        bool split_child_ok() const
        {
            if (!body_complete())
                return true;
            int const child = worker_child_size();
            int exits = 0;
            child_space(child, child + 2, exits);
            return exits > 0;
        }

        // v13.1: split-escape slots are tiered. Normal worker splits stop at cap - 2
        // (ESCAPE_UNIT_RESERVE); an ordinary worker may escape only below that, growers
        // and any other dragon of 16+ may take the next slot, and only the king the
        // last one. At the cap nobody can split out, and a boxed-in king dies.
        bool escape_slot_available() const
        {
            // Before growers start there is no king yet: the longest dragon we know of
            // (12+, nobody known 3 longer) is the future one and gets its tier.
            bool const king = retaining_length() ? king_.id == my_id()
                                                 : controller_.get_length() >= Config::KING_MIN_LENGTH && is_king_or_vice();
            // v21: long workers too take the slot kept for long dragons.
            int const reserve = king ? 0 : (valuable_now_ || controller_.get_length() >= Config::LONG_WORKER_ESCAPE_LENGTH) ? 1
                                                                                                                            : Config::ESCAPE_UNIT_RESERVE;
            return controller_.get_unit_count() < unit_cap() - reserve;
        }

        // A dead end worth raiding: only workers (never the king or growers), only
        // while the swarm builds, and only if a split will be legal at the far end.
        bool pocket_plan_allowed() const
        {
            return !retaining_length() && !is_queen() && game_.get_round_num() < Config::GATHER_ROUND &&
                   escape_slot_available() && body_complete();
        }

        // Portal loops: the last portal pair we went through, and when.
        int last_portal_id_ = -1;
        int last_portal_round_ = -1000;

        void note_portals_taken(std::vector<Direction> const &steps)
        {
            Position current = controller_.get_position();
            for (Direction d : steps)
            {
                RememberedEdge const &e = cell(current).edges[static_cast<std::size_t>(direction_index(d))];
                Step const s = step_from(current, d);
                if (e.known && e.type == EdgeType::PORTAL)
                {
                    last_portal_id_ = e.portal_id;
                    last_portal_round_ = game_.get_round_num();
#ifdef BOT_DEBUG
                    controller_.output_log("PORTAL", e.portal_id, s.destination_known ? "known" : "unknown");
#endif
                }
                if (!s.passable || !s.destination_known)
                    return;
                current = s.destination;
            }
        }

        EdgeKey canonical_edge(Position position, Direction direction) const
        {
            switch (direction.value)
            {
            case Direction::NORTH:
                return {true, position.x, position.y};
            case Direction::SOUTH:
                return {true, position.x, wrap(position.y + 1, height_)};
            case Direction::WEST:
                return {false, position.x, position.y};
            default:
                return {false, wrap(position.x + 1, width_), position.y};
            }
        }

        void remember_portal(int portal_id, EdgeKey endpoint)
        {
            std::vector<EdgeKey> &endpoints = portal_edges_[portal_id];
            if (std::find(endpoints.begin(), endpoints.end(), endpoint) == endpoints.end())
            {
                endpoints.push_back(endpoint);
                nbr_all_dirty_ = true; // v22: a portal's far end may be known now
                nbr_valid_ = false;
            }
        }

        std::optional<Position> portal_exit(Position position, Direction direction,
                                            int portal_id) const
        {
            auto const found = portal_edges_.find(portal_id);
            if (found == portal_edges_.end() || found->second.size() != 2)
                return std::nullopt;

            EdgeKey const entry = canonical_edge(position, direction);
            EdgeKey const *other = nullptr;
            for (EdgeKey const &endpoint : found->second)
            {
                if (!(endpoint == entry))
                {
                    other = &endpoint;
                    break;
                }
            }
            if (!other)
                return std::nullopt;

            if (other->horizontal)
            {
                if (direction.value == Direction::NORTH)
                    return Position(other->x, wrap(other->y - 1, height_));
                if (direction.value == Direction::SOUTH)
                    return Position(other->x, other->y);
                return std::nullopt;
            }
            if (direction.value == Direction::WEST)
                return Position(wrap(other->x - 1, width_), other->y);
            if (direction.value == Direction::EAST)
                return Position(other->x, other->y);
            return std::nullopt;
        }

        // Memory holds every visible edge (remember_vision runs first), so no
        // per-step vision lookup is needed. An edge belongs to both tiles it separates:
        // a tile we never saw does not know its own edges, but its neighbour may (e.g.
        // kelp along the border of what we have seen), so searches that start or pass
        // through unseen tiles do not walk through known walls.
        // v21: do we know what is on this edge (seen from either side)?
        bool edge_known(Position position, Direction direction) const
        {
            return cell(position).edges[static_cast<std::size_t>(direction_index(direction))].known ||
                   cell(position.add_dir(direction)).edges[static_cast<std::size_t>(direction_index(direction.get_opposite()))].known;
        }

        Step step_from(Position position, Direction direction) const
        {
            if (!nbr_valid_)
                return step_from_raw(position, direction);
            int const n = nb(index(position), direction_index(direction));
            if (n == NBR_WALL)
                return {};
            if (n == NBR_BLIND)
                return {true, false, true, position};
            return {true, true, nbr_portal_[static_cast<std::size_t>(4 * index(position) + direction_index(direction))] != 0,
                    position_of(n)};
        }

        void refresh_neighbours()
        {
            std::size_t const cells = memory_.size();
            auto build = [&](int c)
            {
                Position const p = position_of(c);
                for (int d = 0; d < 4; ++d)
                {
                    Step const st = step_from_raw(p, Direction::get_direction_list()[static_cast<std::size_t>(d)]);
                    std::size_t const k = static_cast<std::size_t>(4 * c + d);
                    nbr_[k] = !st.passable ? NBR_WALL : !st.destination_known ? NBR_BLIND
                                                                              : index(st.destination);
                    nbr_portal_[k] = st.portal ? 1 : 0;
                }
            };
            if (nbr_all_dirty_ || nbr_.size() != 4 * cells)
            {
                nbr_.assign(4 * cells, NBR_WALL);
                nbr_portal_.assign(4 * cells, 0);
                for (std::size_t c = 0; c < cells; ++c)
                    build(static_cast<int>(c));
            }
            else
                for (int c : nbr_dirty_)
                {
                    // A step reads the edge on its own side, or the far side's if that is
                    // unknown: the tile and its four neighbours.
                    build(c);
                    Position const p = position_of(c);
                    for (Direction d : Direction::get_direction_list())
                        build(index(p.add_dir(d)));
                }
            nbr_dirty_.clear();
            nbr_all_dirty_ = false;
            nbr_valid_ = true;
        }

#ifdef NBR_CHECK
        void nbr_check(char const *tag)
        {
            int bad = 0;
            for (std::size_t c = 0; c < memory_.size(); ++c)
                for (int d = 0; d < 4; ++d)
                {
                    Position const p = position_of(static_cast<int>(c));
                    Step const a = step_from_raw(p, Direction::get_direction_list()[static_cast<std::size_t>(d)]);
                    Step const b = step_from(p, Direction::get_direction_list()[static_cast<std::size_t>(d)]);
                    if (a.passable != b.passable || a.destination_known != b.destination_known || a.portal != b.portal ||
                        (a.passable && !(a.destination == b.destination)))
                        ++bad;
                }
            if (bad)
                controller_.output_log("NBRBAD", tag, bad);
        }
#endif
        Step step_from_raw(Position position, Direction direction) const
        {
            RememberedEdge edge = cell(position).edges[static_cast<std::size_t>(direction_index(direction))];
            if (!edge.known)
                edge = cell(position.add_dir(direction)).edges[static_cast<std::size_t>(direction_index(direction.get_opposite()))];
            if (edge.known && edge.type == EdgeType::KELP)
                return {};
            if (edge.known && edge.type == EdgeType::PORTAL)
            {
                std::optional<Position> const exit = portal_exit(position, direction, edge.portal_id);
                if (!exit)
                    return {true, false, true, position};
                return {true, true, true, *exit};
            }
            return {true, true, false, position.add_dir(direction)};
        }

        // NEW: remember_vision incorporates symmetry deduction
        void remember_vision()
        {
            int const round = game_.get_round_num();
            int free_before = 0, filled = 0; // v21: tiles free when seen last round, and now taken
            int tiles = 0, occupied = 0;     // v21: tiles in view, and those holding another dragon
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                Position pos = tile.get_position();
                CellMemory &remembered = cell(pos);
                ++tiles;
                if (tile.get_dragon() && tile.get_dragon()->get_id() != controller_.get_id())
                    ++occupied;
                if (remembered.seen && remembered.last_seen == round - 1 && !remembered.other_seen)
                {
                    ++free_before;
                    if (tile.get_dragon() && tile.get_dragon()->get_id() != controller_.get_id())
                        ++filled;
                }
                if (!remembered.seen)
                    note_first_sighting(pos);
                remembered.seen = true;
                remembered.last_seen = round;
                remembered.pearl = tile.has_pearl();
                remembered.pearl_seen = tile.has_pearl() ? round : -1;
                remembered.pearl_time = tile.get_pearl_time();
                remembered.pearl_sample_round = round;
                if (remembered.pearl_time >= 1)
                {
                    // v23: one sample a round; halved now and then so the estimate keeps up.
                    remembered.cd_sum += remembered.pearl_time;
                    if (++remembered.cd_n >= 64)
                    {
                        remembered.cd_sum /= 2;
                        remembered.cd_n /= 2;
                    }
                }
                remembered.other_seen = tile.get_dragon() && tile.get_dragon()->get_id() != controller_.get_id();
                remembered.busy_until = remembered.other_seen ? round + Config::OCCUPIED_MEMORY + 1 : -1;

                for (Direction direction : Direction::get_direction_list())
                {
                    unswbc::Edge const &edge = tile.get_edge(direction);
                    RememberedEdge &known = remembered.edges[static_cast<std::size_t>(direction_index(direction))];
                    RememberedEdge const fresh{true, edge.get_edge_type(), edge.get_portal_id()};
                    if (!known.known || known.type != fresh.type || known.portal_id != fresh.portal_id)
                    {
                        nbr_dirty_.push_back(index(pos)); // v22
                        nbr_valid_ = false;
                    }
                    known = fresh;
                    if (edge.is_portal())
                        remember_portal(edge.get_portal_id(),
                                        canonical_edge(pos, direction));
                }
                // Symmetry deduction remains disabled (see v10).
            }
            // v21: how fast free tiles around us fill up (per tile per round, running
            // average): what a tile we cannot see now risks for each round since we saw it.
            if (free_before >= 10)
                fill_rate_x1e4_ += (filled * 10000 / free_before - fill_rate_x1e4_) / 16;
            if (tiles > 0)
                occupancy_x1e4_ += (occupied * 10000 / tiles - occupancy_x1e4_) / 16;
        }

        // ---------------------------------------------------------------
        // v11: body model
        // ---------------------------------------------------------------

        // Follows a dragon's body from its head through visible segments. Each body
        // segment's facing points at the next segment towards the head.
        BodyTrace trace_body(Position head, int id) const
        {
            BodyTrace trace;
            trace.segments.push_back(head);
            Position current = head;
            for (int guard = 0; guard < width_ * height_; ++guard)
            {
                bool found = false;
                Position next{};
                for (Direction d : Direction::get_direction_list())
                {
                    Step const s = step_from(current, d);
                    if (!s.passable || !s.destination_known)
                        continue;
                    unswbc::DragonPart const *part = visible_part(s.destination);
                    if (!part || pid(part->get_id()) != pid(id) || part->is_head())
                        continue;
                    Step const back = step_from(s.destination, part->get_dir());
                    if (back.passable && back.destination_known && back.destination == current)
                    {
                        next = s.destination;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    break;
                trace.segments.push_back(next);
                current = next;
            }
            // Only a segment whose every neighbour is visible can be proven to be the tail.
            trace.complete = true;
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(current, d);
                if (!s.passable)
                    continue;
                if (!s.destination_known || !controller_.get_tile(s.destination))
                {
                    trace.complete = false;
                    break;
                }
            }
            return trace;
        }

        void update_body_model()
        {
            apply_body_msgs();
            Position const here = controller_.get_position();
            std::size_t const length = static_cast<std::size_t>(controller_.get_length());

            // Every tile the head entered, in order, including sprint steps. The body
            // is always the last `length` of them.
            if (pending_valid_ && !pending_path_.empty() && pending_path_.back() == here)
            {
                for (Position const p : pending_path_)
                    if (own_trail_.empty() || !(own_trail_.front() == p))
                        own_trail_.push_front(p);
            }
            else if (own_trail_.empty() || !(own_trail_.front() == here))
            {
                own_trail_.push_front(here);
            }
            pending_path_.clear();
            pending_valid_ = false;

            // Vision is authoritative where it reaches.
            BodyTrace const trace = trace_body(here, controller_.get_id());
            std::size_t const common = std::min(trace.segments.size(), own_trail_.size());
            bool mismatch = false;
            for (std::size_t i = 0; i < common; ++i)
                if (!(own_trail_[i] == trace.segments[i]))
                {
                    mismatch = true;
                    break;
                }
            if (mismatch)
            {
                own_trail_.assign(trace.segments.begin(), trace.segments.end());
                ++dbg_resets_;
            }
            else
                for (std::size_t i = own_trail_.size(); i < trace.segments.size(); ++i)
                    own_trail_.push_back(trace.segments[i]);
            verify_trail();
            extend_trail_from_memory(static_cast<int>(length));
            while (own_trail_.size() > length)
                own_trail_.pop_back();
            known_body_ = static_cast<int>(own_trail_.size());

            for (int c : touched_)
            {
                own_idx_[static_cast<std::size_t>(c)] = -1;
                other_occ_[static_cast<std::size_t>(c)] = 0;
            }
            touched_.clear();
            for (int i = 0; i < known_body_; ++i)
            {
                int const c = index(own_trail_[static_cast<std::size_t>(i)]);
                if (own_idx_[static_cast<std::size_t>(c)] == -1)
                {
                    own_idx_[static_cast<std::size_t>(c)] = i;
                    touched_.push_back(c);
                }
            }
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                unswbc::DragonPart const *part = tile.get_dragon();
                if (!part)
                    continue;
                int const c = index(tile.get_position());
                if (part->get_id() == controller_.get_id())
                {
                    if (own_idx_[static_cast<std::size_t>(c)] == -1)
                    {
                        own_idx_[static_cast<std::size_t>(c)] = -2;
                        touched_.push_back(c);
                    }
                }
                else
                {
                    other_occ_[static_cast<std::size_t>(c)] = 1;
                    touched_.push_back(c);
                }
            }
            remember_own_body(static_cast<int>(length));
        }

        // v13.1: memory of our own body. A body cell keeps its segment (and that
        // segment's facing) until the tail leaves it, so a segment we saw earlier is
        // still there unless enough moves have passed. A king born from a split escape
        // starts knowing only the part of its body it can see, and used to treat the
        // rest as empty space: it curled into its own tail and boxed itself in.
        int dbg_resets_ = 0, dbg_truncs_ = 0, dbg_mem_links_ = 0; // debug counters
        std::vector<int> own_seen_;                               // round we last saw our (non-head) segment here, -1: none
        std::vector<int> own_seen_idx_;                           // its index then (a lower bound: 1 if unknown)
        std::vector<unsigned char> own_seen_dir_;                 // its facing then

        // v13.1 body handover. At a split escape the new dragon (the child) starts at our
        // tail and sees only the part of its body near it; the rest is our head side,
        // which we know. We send it on every sonar ray that will hit the child this turn
        // (the child moves later this round and reads them first).
        // Magic 0xA: 59 team | 58-50 child length | 49-41 child index h0 of the first
        // tile | 40-35 x | 34-29 y of that tile | 28-5 twelve 2-bit facings of child
        // segments h0, h0+1, ... (each faces the one before it) | 4-0 checksum.
        std::vector<std::uint64_t> body_msgs_; // received on our first turn
        bool escape_born_ = false;             // v23: our parent split-escaped (its handover reached us)

        // v23: a split escape may keep a head part that lives only if our tail leaves the
        // tile beside it (endgame.hpp choose_escape). So on our first turns we do not split
        // (that keeps our tail where it is), and on the first we do not stall on a pearl.
        bool escape_child_holding() const
        {
            return escape_born_ && game_.get_round_num() - first_round_ < Config::ESCAPE_CHILD_HOLD_TURNS;
        }
        bool move_frees_tail(Candidate const &c) const
        {
            int const L = controller_.get_length();
            int const steps = static_cast<int>(c.steps.size());
            int eaten = 0;
            Position p = controller_.get_position();
            for (Direction d : c.steps)
            {
                Step const s = step_from(p, d);
                if (!s.passable || !s.destination_known)
                    break;
                p = s.destination;
                if (unswbc::Tile const *t = controller_.get_tile(p))
                    if (t->has_pearl())
                        ++eaten;
            }
            return steps - eaten + paid_steps(L, steps) > 0;
        }

        int dir_between(Position a, Position b) const
        {
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(a, d);
                if (s.passable && s.destination_known && s.destination == b)
                    return direction_index(d);
            }
            return -1;
        }

        // Child tiles from our segment s0 downwards (towards our head).
        std::uint64_t body_chunk_message(int child, int s0) const
        {
            int const L = controller_.get_length();
            int const keep = L - child; // v23: the escape may keep more than 2
            if (s0 < keep || s0 + 1 >= known_body_)
                return 0;
            std::uint64_t m = 0xAULL << 60U;
            m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
            m |= std::uint64_t(child & 511) << 50U;
            m |= std::uint64_t((L - 1 - s0) & 511) << 41U;
            Position const p0 = own_trail_[static_cast<std::size_t>(s0)];
            m |= std::uint64_t(p0.x & 63) << 35U;
            m |= std::uint64_t(p0.y & 63) << 29U;
            for (int i = 0; i < 12 && s0 - i >= keep; ++i)
            {
                int const j = s0 - i;
                // The child's segment on our segment j faces our segment j + 1.
                int const d = dir_between(own_trail_[static_cast<std::size_t>(j)], own_trail_[static_cast<std::size_t>(j + 1)]);
                if (d < 0)
                    return 0;
                m |= std::uint64_t(d) << (5U + 2U * static_cast<unsigned>(i));
            }
            m |= packet_checksum(m, 5);
            return m;
        }

        // Will this turn's sonar ray in direction d stop on the child (our segments
        // keep+, after a split that leaves us the first `keep`)?
        bool ray_hits_child(Direction d, int keep = 2) const
        {
            Position p = controller_.get_position();
            if (d == controller_.get_dir().get_opposite())
            {
                // That ray leaves our tail, which after the split is segment keep - 1.
                if (known_body_ < keep)
                    return false;
                p = own_trail_[static_cast<std::size_t>(keep - 1)];
            }
            for (int n = 0; n < width_ + height_; ++n)
            {
                Step const s = step_from(p, d);
                if (!s.passable || !s.destination_known || !controller_.get_tile(s.destination))
                    return false;
                std::size_t const c = static_cast<std::size_t>(index(s.destination));
                int const oi = own_idx_[c];
                if ((oi >= 0 && oi < keep) || other_occ_[c])
                    return false;
                if (oi >= keep || oi == -2)
                    return true;
                p = s.destination;
            }
            return false;
        }

        void send_body_handover(int child)
        {
            int const L = controller_.get_length();
            // The child traces its body from its head (our tail) while it stays in view;
            // start just inside that (2 tiles of overlap) and go towards our head.
            int s_top = known_body_ - 2;
            if (body_complete())
            {
                Position const tail = own_trail_.back();
                int j = L - 2;
                while (j >= L - child && chebyshev_distance(own_trail_[static_cast<std::size_t>(j)], tail) <= 3)
                    --j;
                s_top = std::min(s_top, j + 2);
            }
            int chunk = 0, sent = 0;
            for (Direction d : Direction::get_direction_list())
            {
                if (!ray_hits_child(d, L - child))
                    continue;
                std::uint64_t const m = body_chunk_message(child, s_top - 12 * chunk);
                if (!m)
                    break;
                controller_.send_sonar(d, m); // replaces the status packet on that ray
                ++chunk;
                ++sent;
            }
#ifdef BOT_DEBUG
            controller_.output_log("BODYSEND", sent, "from", s_top, "known", known_body_, L);
#else
            (void)sent;
#endif
        }

        // On our first turn: take the body tiles our parent sent, if they fit what we see.
        void apply_body_msgs()
        {
            if (body_msgs_.empty())
                return;
            ensure_own_memory();
            int const now = game_.get_round_num();
            int const L = controller_.get_length();
            for (std::uint64_t const m : body_msgs_)
            {
                if ((m & 31ULL) != packet_checksum(m & ~31ULL, 5) || static_cast<int>((m >> 50U) & 511ULL) != L)
                    continue;
                int const h0 = static_cast<int>((m >> 41U) & 511ULL);
                Position const first(static_cast<int>((m >> 35U) & 63ULL), static_cast<int>((m >> 29U) & 63ULL));
                if (h0 < 1 || h0 >= L || first.x >= width_ || first.y >= height_)
                    continue;
                std::vector<std::array<int, 3>> tiles; // cell, child index, facing
                bool ok = true;
                Position prev = first;
                for (int i = 0; i < 12 && h0 + i < L; ++i)
                {
                    int const f = static_cast<int>((m >> (5U + 2U * static_cast<unsigned>(i))) & 3ULL);
                    Direction const fd = Direction::get_direction_list()[static_cast<std::size_t>(f)];
                    Position c = first;
                    if (i > 0)
                    {
                        Step const s = step_from(prev, fd.get_opposite());
                        if (!s.passable || !s.destination_known)
                            break;
                        Step const back = step_from(s.destination, fd);
                        if (!back.passable || !back.destination_known || !(back.destination == prev))
                            break;
                        c = s.destination;
                    }
                    if (unswbc::Tile const *t = controller_.get_tile(c))
                    {
                        unswbc::DragonPart const *part = t->get_dragon();
                        if (!part || part->get_id() != controller_.get_id() || part->is_head() ||
                            direction_index(part->get_dir()) != f)
                        {
                            ok = false;
                            break;
                        }
                    }
                    tiles.push_back({index(c), h0 + i, f});
                    prev = c;
                }
                if (!ok)
                    continue;
                escape_born_ = true;
                for (auto const &[c, h, f] : tiles)
                {
                    own_seen_[static_cast<std::size_t>(c)] = now;
                    own_seen_idx_[static_cast<std::size_t>(c)] = h;
                    own_seen_dir_[static_cast<std::size_t>(c)] = static_cast<unsigned char>(f);
                }
#ifdef BOT_DEBUG
                controller_.output_log("PBODY", h0, static_cast<int>(tiles.size()));
#endif
            }
            body_msgs_.clear();
        }

        void ensure_own_memory()
        {
            if (own_seen_.empty())
            {
                own_seen_.assign(memory_.size(), -1);
                own_seen_idx_.assign(memory_.size(), 1);
                own_seen_dir_.assign(memory_.size(), 0);
            }
        }

        // Every trail cell we can see must hold our segment, facing the cell before
        // it. Anything after the first contradiction is unreliable: drop it (the
        // unknown remainder is then treated as blocking, not as free).
        void verify_trail()
        {
            for (std::size_t i = 1; i < own_trail_.size(); ++i)
            {
                Position const q = own_trail_[i];
                if (!controller_.get_tile(q))
                    continue;
                unswbc::DragonPart const *part = visible_part(q);
                bool ok = part && part->get_id() == controller_.get_id() && !part->is_head();
                if (ok)
                {
                    Step const s = step_from(q, part->get_dir());
                    if (s.passable && s.destination_known && !(s.destination == own_trail_[i - 1]))
                        ok = false;
                }
                if (!ok)
                {
                    own_trail_.resize(i);
                    ++dbg_truncs_;
                    return;
                }
            }
        }

        // Continue the trail past what we can see through segments we saw earlier,
        // following their facings back towards the head. Only an unambiguous link
        // that is consistent with its age is taken.
        void extend_trail_from_memory(int length)
        {
            ensure_own_memory();
            int const now = game_.get_round_num();
            ++generation_;
            int const gen = generation_;
            for (Position const &p : own_trail_)
                stamp_[static_cast<std::size_t>(index(p))] = gen;
            while (!own_trail_.empty() && static_cast<int>(own_trail_.size()) < length)
            {
                Position const p = own_trail_.back();
                int const k = static_cast<int>(own_trail_.size()); // index the next cell would get
                int found = -1, count = 0;
                for (Direction d : Direction::get_direction_list())
                {
                    Step const s = step_from(p, d);
                    if (!s.passable || !s.destination_known || controller_.get_tile(s.destination))
                        continue;
                    std::size_t const c = static_cast<std::size_t>(index(s.destination));
                    if (own_seen_[c] < 0 || stamp_[c] == gen || own_seen_idx_[c] + (now - own_seen_[c]) > k)
                        continue;
                    Step const back = step_from(s.destination, Direction::get_direction_list()[own_seen_dir_[c]]);
                    if (!back.passable || !back.destination_known || !(back.destination == p))
                        continue;
                    ++count;
                    found = static_cast<int>(c);
                }
                if (count != 1)
                    return;
                stamp_[static_cast<std::size_t>(found)] = gen;
                own_trail_.push_back(position_of(found));
                ++dbg_mem_links_;
            }
        }

        // Record what we see of our body, forget sightings that cannot still be body,
        // and while the body is not fully known, treat the remaining out-of-sight
        // sightings as occupied (index unknown: frees last).
        void remember_own_body(int length)
        {
            ensure_own_memory();
            int const now = game_.get_round_num();
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                std::size_t const c = static_cast<std::size_t>(index(tile.get_position()));
                unswbc::DragonPart const *part = tile.get_dragon();
                if (part && part->get_id() == controller_.get_id() && !part->is_head())
                {
                    // Best lower bound on its index: exact from the trail, else what an
                    // earlier sighting (or our parent's handover) implies.
                    own_seen_idx_[c] = own_idx_[c] >= 1    ? own_idx_[c]
                                       : own_seen_[c] >= 0 ? own_seen_idx_[c] + (now - own_seen_[c])
                                                           : 1;
                    own_seen_[c] = now;
                    own_seen_dir_[c] = static_cast<unsigned char>(direction_index(part->get_dir()));
                }
                else
                    own_seen_[c] = -1;
            }
            bool const complete = known_body_ >= length;
            for (std::size_t c = 0; c < own_seen_.size(); ++c)
            {
                if (own_seen_[c] < 0 || own_seen_[c] == now || own_idx_[c] != -1)
                    continue;
                if (complete || own_seen_idx_[c] + (now - own_seen_[c]) >= length)
                {
                    own_seen_[c] = -1; // not in our (fully known) body, or the tail has passed it
                    continue;
                }
                own_idx_[c] = -2;
                touched_.push_back(static_cast<int>(c));
            }
        }

        bool body_complete() const
        {
            return known_body_ == controller_.get_length();
        }

        void record_pending_path(std::vector<Direction> const &steps)
        {
            pending_path_.clear();
            pending_valid_ = true;
            Position current = controller_.get_position();
            for (Direction d : steps)
            {
                Step const s = step_from(current, d);
                if (!s.passable || !s.destination_known)
                {
                    pending_valid_ = false;
                    pending_path_.clear();
                    return;
                }
                current = s.destination;
                pending_path_.push_back(current);
            }
        }

        // Segment index of a cell after the move in pm (-1 when free).
        int post_index(PostMove const &pm, int c) const
        {
            int const k = static_cast<int>(pm.path.size());
            for (int m = k; m >= 1; --m)
                if (index(pm.path[static_cast<std::size_t>(m - 1)]) == c)
                {
                    int const i = k - m;
                    return i < pm.length ? i : -1;
                }
            int i = own_idx_[static_cast<std::size_t>(c)];
            if (i == -1)
                return -1;
            if (i == -2)
                i = known_body_; // earliest index it could have: frees last
            i += pm.steps;
            return i < pm.length ? i : -1;
        }

        // Replays a multi-step move exactly as the engine applies it: pay check,
        // collision against the body as the previous step left it, pearl growth,
        // and the extra tail segment per paid step (v21: the first free_moves(L) steps
        // of a turn are free).
        // v21 `remembered_ok` (rushes): a step may also land on a tile we cannot see now
        // but have seen, through an edge we know, with no body remembered there; those
        // tiles go to `blind`, and a pearl remembered on one counts as eaten (its tail
        // staying put is the cautious case for running into ourselves).
        bool simulate_path(std::vector<Direction> const &steps, PostMove &pm, bool attack_final,
                           bool remembered_ok = false, std::vector<int> *blind = nullptr) const
        {
            pm = PostMove{};
            Position current = controller_.get_position();
            int const L = controller_.get_length();
            int const free = free_moves(L);
            int eaten = 0;
            for (std::size_t j = 0; j < steps.size(); ++j)
            {
                int const s = static_cast<int>(j);
                int const length_now = L + eaten - std::max(0, s - free); // after the steps before this one
                if (s >= free && length_now <= 2)
                    return false; // a paid step needs a dragon longer than 2
                Step const step = step_from(current, steps[j]);
                if (!step.passable || !step.destination_known)
                    return false;
                Position const q = step.destination;
                int const c = index(q);
                bool const last = j + 1 == steps.size();
                bool const unseen = !controller_.get_tile(q);
                if (unseen && s >= 1)
                {
                    if (!remembered_ok)
                        return false; // sprints stay inside what we can see
                    CellMemory const &m = memory_[static_cast<std::size_t>(c)];
                    if (step.portal || !m.seen || body_remembered(m) || !edge_known(current, steps[j]))
                        return false;
                    if (blind)
                        blind->push_back(c);
                }
                if (other_occ_[static_cast<std::size_t>(c)])
                {
                    if (!attack_final || !last)
                        return false;
                    unswbc::DragonPart const *part = visible_part(q);
                    if (!part || !part->is_head() || part->get_team() == controller_.get_team())
                        return false;
                    pm.path.push_back(q);
                    pm.head = q;
                    pm.steps = static_cast<int>(steps.size());
                    pm.eaten = eaten;
                    pm.length = L + eaten - paid_steps(L, pm.steps);
                    return true;
                }
                if (attack_final && last)
                    return false; // the target head is gone
                // Own body as the previous step left it.
                for (int m = 1; m <= s; ++m)
                    if (pm.path[static_cast<std::size_t>(m - 1)] == q && s - m < length_now)
                        return false;
                int oi = own_idx_[static_cast<std::size_t>(c)];
                if (oi == -2)
                    oi = known_body_;
                if (oi >= 0 && oi + s < length_now)
                {
                    bool revisited = false;
                    for (int m = 1; m <= s; ++m)
                        if (pm.path[static_cast<std::size_t>(m - 1)] == q)
                            revisited = true;
                    if (!revisited)
                        return false;
                }
                bool already = false;
                for (Position const &p : pm.path)
                    if (p == q)
                        already = true;
                if (!already && (visible_pearl(q) || (remembered_ok && unseen && memory_[static_cast<std::size_t>(c)].pearl)))
                    ++eaten;
                pm.path.push_back(q);
                current = q;
            }
            pm.head = current;
            pm.steps = static_cast<int>(steps.size());
            pm.eaten = eaten;
            pm.length = L + eaten - paid_steps(L, pm.steps);
            return true;
        }

        // Cells reachable from the post-move head when our own segments free up as
        // the tail moves. Segment i may be entered from move L-i+1 on. Waiting for a
        // segment is only allowed when there is already room to wander meanwhile.
        template <class IndexFn>
        int timed_space_generic(Position head, int length, IndexFn const &body_index, int cap, bool known_only,
                                SpaceStats *stats = nullptr) const
        {
            int const now = game_.get_round_num();
            ++generation_;
            int const gen = generation_;
            // Item: (arrival, cell << 7 | eaten << 1 | waited).
            //  - waited: reached through a wait. Only unwaited cells count as room to
            //    stall in, otherwise the estimate snowballs through the body it waits for.
            //  - eaten: pearls on the path. Each one stalls the tail a turn, so our
            //    segments free that much later.
            using Item = std::pair<int, int>;
            std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
            int const start = index(head);
            stamp_[static_cast<std::size_t>(start)] = gen;
            best_[static_cast<std::size_t>(start)] = 0;
            pq.push({0, start << 7});
            int settled = 0;
            int stall_room = 0;
            while (!pq.empty() && settled < cap)
            {
                auto const [t, packed] = pq.top();
                pq.pop();
                int const c = packed >> 7;
                int const eaten = (packed >> 1) & 63;
                bool const waited = (packed & 1) != 0;
                if (done_[static_cast<std::size_t>(c)] == gen || best_[static_cast<std::size_t>(c)] != t)
                    continue;
                done_[static_cast<std::size_t>(c)] = gen;
                ++settled;
                if (!waited)
                    ++stall_room;
                if (stats)
                {
                    CellMemory const &here = memory_[static_cast<std::size_t>(c)];
                    if (!here.seen)
                        stats->unknown = true;
                    else if (here.pearl && (here.last_seen == now || now - here.pearl_seen <= Config::POCKET_PEARL_MEMORY))
                        ++stats->pearls;
                }
                Position const p = position_of(c);
                for (Direction d : Direction::get_direction_list())
                {
                    Step const s = step_from(p, d);
                    if (!s.passable)
                        continue;
                    if (!s.destination_known)
                    {
                        if (stats)
                            stats->unknown = true; // a portal we cannot follow
                        continue;
                    }
                    int const n = index(s.destination);
                    if (done_[static_cast<std::size_t>(n)] == gen)
                    {
                        if (stats && n != c)
                            ++stats->edges; // counted once, from the later of its two ends
                        continue;
                    }
                    if (other_occ_[static_cast<std::size_t>(n)])
                    {
                        if (stats)
                            ++stats->blockers;
                        continue;
                    }
                    int arrive = t + 1;
                    bool wait_here = waited;
                    int const oi = body_index(n);
                    if (oi == 0 || oi == -3) // -3: static part of us
                        continue;
                    CellMemory const &m = memory_[static_cast<std::size_t>(n)];
                    if (oi < 0 && m.last_seen != now)
                    {
                        // Out of sight: a body seen there just now is probably still there.
                        if (body_remembered(m))
                        {
                            if (stats)
                                ++stats->blockers;
                            continue;
                        }
                        if (known_only && !(m.seen && !m.other_seen && now - m.last_seen <= Config::KNOWN_FREE_MEMORY))
                            continue;
                    }
                    if (oi > 0)
                    {
                        int const free_at = length - oi + 1 + eaten;
                        if (free_at > arrive)
                        {
                            if (free_at - arrive > stall_room / 2)
                                continue;
                            arrive = free_at;
                            wait_here = true;
                        }
                    }
                    int const next_eaten = std::min(63, eaten + (m.pearl && m.last_seen == now ? 1 : 0));
                    if (stamp_[static_cast<std::size_t>(n)] != gen || arrive < best_[static_cast<std::size_t>(n)])
                    {
                        stamp_[static_cast<std::size_t>(n)] = gen;
                        best_[static_cast<std::size_t>(n)] = arrive;
                        pq.push({arrive, (n << 7) | (next_eaten << 1) | (wait_here ? 1 : 0)});
                    }
                }
            }
            if (stats)
            {
                stats->cells = settled;
                stats->capped = settled >= cap;
            }
            return settled;
        }

        int timed_space(PostMove const &pm, int cap, bool known_only = false, SpaceStats *stats = nullptr) const
        {
            return timed_space_generic(pm.head, pm.length, [&](int c)
                                       { return post_index(pm, c); }, cap, known_only, stats);
        }

        // Free neighbours of the post-move head at the next move (every segment still blocks).
        int post_exits(PostMove const &pm) const
        {
            int exits = 0;
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(pm.head, d);
                if (!s.passable)
                    continue;
                if (!s.destination_known)
                {
                    ++exits;
                    continue;
                }
                int const n = index(s.destination);
                if (other_occ_[static_cast<std::size_t>(n)] || post_index(pm, n) >= 0)
                    continue;
                ++exits;
            }
            return exits;
        }

        // Crowd risk for a valuable dragon standing at pm.head after the move.
        // Segments of our body we cannot place (out of sight, never seen since birth).
        // The space search may count up to that many of their tiles as free, so every
        // room requirement grows by it (v13.1).
        int unknown_body() const
        {
            return std::max(0, controller_.get_length() - known_body_);
        }

        int space_cap(int length) const
        {
            return std::max(64, length + 20 + unknown_body());
        }

        // Valuable dragons: prefer moves that keep the tail reachable through tiles we can
        // vouch for. Relative to the best option, so it still ranks moves when no option
        // reaches the full body length (long kings rarely see their whole route).
        // v20: each tile short of the best is a small chance of being boxed in.
        void apply_known_space_penalties(std::vector<Candidate> &candidates) const
        {
            if (!valuable_now_)
                return;
            int best = -1;
            for (Candidate const &c : candidates)
                if (!c.head_trade && c.destination_known && c.known_space >= 0)
                    best = std::max(best, std::min(c.known_space, c.new_length + 2));
            if (best < 0)
                return;
            int const D = death_cost();
            for (Candidate &c : candidates)
            {
                if (c.head_trade || !c.destination_known || c.known_space < 0)
                    continue;
                int const deficit = std::max(0, best - c.known_space);
                c.score -= risk(std::min(Config::P_KNOWN_SPACE_CAP, deficit * Config::P_KNOWN_SPACE_CELL), D);
            }
        }

        // ---------------------------------------------------------------

        void observe_heads()
        {
            heads_.clear();
            int visible_tiles = 0;
            int enemy_heads = 0;
            int ally_heads = 0;
            std::unordered_map<int, int> visible_lengths;
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                unswbc::DragonPart const *part = tile.get_dragon();
                if (part)
                    ++visible_lengths[part->get_id()];
            }
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                unswbc::DragonPart const *part = tile.get_dragon();
                if (!part || !part->is_head() || part->get_id() == controller_.get_id())
                    continue;
                SeenHead head{pid(part->get_id()), part->get_team(), part->get_position(),
                              part->get_dir(), visible_lengths[part->get_id()]};
                head.queen = part->get_id() < 2; // v22
                BodyTrace const trace = trace_body(part->get_position(), part->get_id());
                int const traced = static_cast<int>(trace.segments.size());
                head.lower_length = std::max({2, head.visible_length, traced});
                head.truncated = !trace.complete || head.visible_length > traced;
                if (!head.truncated && head.team != controller_.get_team())
                    typical_enemy_x16_ += (head.lower_length * 16 - typical_enemy_x16_) / 8;
                if (head.team != controller_.get_team())
                    ++enemy_heads;
                else
                    ++ally_heads;
                heads_.push_back(head);
            }
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                (void)tile;
                ++visible_tiles;
            }
            if (visible_tiles > 0)
            {
                enemy_density_x1e4_ += (enemy_heads * 10000 / visible_tiles - enemy_density_x1e4_) / 8;
                ally_density_x1e4_ += (ally_heads * 10000 / visible_tiles - ally_density_x1e4_) / 8;
            }
        }

        SeenHead const *seen_head(int id) const
        {
            for (SeenHead const &h : heads_)
                if (h.id == pid(id))
                    return &h;
            return nullptr;
        }

        void build_threat_maps()
        {
            std::fill(enemy_threats_.begin(), enemy_threats_.end(), 0);
            std::fill(ally_threats_.begin(), ally_threats_.end(), 0);
            for (SeenHead const &head : heads_)
            {
                bool const ally = head.team == controller_.get_team();
                std::vector<int> &threats = ally ? ally_threats_ : enemy_threats_;
                // v21: an enemy head reaches every tile within its free steps at no
                // cost (an ally's next step is still what crowds us).
                int const reach = ally ? 1 : free_moves(head.estimated_length());
                for (int c : tiles_within(head.position, head.facing, reach))
                    ++threats[static_cast<std::size_t>(c)];
            }
            build_reach_threats();
        }

        // v21: tiles a head can step onto within `reach` steps through tiles free now
        // (not back into its neck, unless `any_first`: we do not know where it faces),
        // each once.
        std::vector<int> tiles_within(Position from, Direction facing, int reach, bool any_first = false) const
        {
            std::vector<int> out;
            std::unordered_set<int> seen{index(from)};
            std::vector<std::pair<int, int>> frontier{{index(from), 0}};
            for (std::size_t q = 0; q < frontier.size(); ++q)
            {
                auto const [c, d] = frontier[q];
                if (d >= reach)
                    continue;
                for (Direction dir : Direction::get_direction_list())
                {
                    if (d == 0 && !any_first && dir == facing.get_opposite())
                        continue;
                    Step const s = step_from(position_of(c), dir);
                    if (!s.passable || !s.destination_known)
                        continue;
                    int const n = index(s.destination);
                    if (!seen.insert(n).second)
                        continue;
                    out.push_back(n);
                    // Bodies stop it (it may still step onto a head: a head-on).
                    if (!visible_occupied(s.destination))
                        frontier.push_back({n, d + 1});
                }
            }
            return out;
        }

        // Every enemy moves exactly once between our consecutive turns, and a dragon
        // of length L can take max_turn_steps(L) steps in that move (v21: ceil(L/4)
        // free, then one segment each while longer than 2), with a collision check on
        // every step. It can only aim at a head inside its own 7x7 window.
        void build_reach_threats()
        {
            std::fill(hard_threat_.begin(), hard_threat_.end(), 0);
            std::fill(soft_threat_.begin(), soft_threat_.end(), 0);
            if (!valuable_now_)
                return;
            int const tail_cell = body_complete() && known_body_ > 0
                                      ? index(own_trail_.back())
                                      : -1;
            for (SeenHead const &head : heads_)
            {
                if (head.team == controller_.get_team())
                    continue;
                int const hard_reach = max_turn_steps(head.lower_length);
                // v21: a body running out of sight: as long as typical, or as its own
                // estimate (what we see + 2), whichever reaches further.
                int const typical_reach = std::min(Config::HIDDEN_BODY_REACH,
                                                   max_turn_steps(std::max((typical_enemy_x16_ + 15) / 16, head.estimated_length())));
                int const soft_reach = head.truncated ? std::max(hard_reach, typical_reach) : hard_reach;
                if (soft_reach <= 0)
                    continue;
                ++generation_;
                int const gen = generation_;
                std::deque<Position> queue;
                int const origin = index(head.position);
                stamp_[static_cast<std::size_t>(origin)] = gen;
                best_[static_cast<std::size_t>(origin)] = 0;
                queue.push_back(head.position);
                while (!queue.empty())
                {
                    Position const p = queue.front();
                    queue.pop_front();
                    int const d = best_[static_cast<std::size_t>(index(p))];
                    if (d >= soft_reach)
                        continue;
                    for (Direction dir : Direction::get_direction_list())
                    {
                        if (d == 0 && dir == head.facing.get_opposite())
                            continue;
                        Step const s = step_from(p, dir);
                        if (!s.passable || !s.destination_known)
                            continue;
                        int const n = index(s.destination);
                        if (stamp_[static_cast<std::size_t>(n)] == gen)
                            continue;
                        stamp_[static_cast<std::size_t>(n)] = gen;
                        best_[static_cast<std::size_t>(n)] = d + 1;
                        if (chebyshev_distance(s.destination, head.position) <= Config::ENEMY_VISION_RADIUS)
                        {
                            if (d + 1 <= hard_reach)
                                ++hard_threat_[static_cast<std::size_t>(n)];
                            else
                                ++soft_threat_[static_cast<std::size_t>(n)];
                        }
                        // Bodies stop the sprint (its own body is treated as passable).
                        bool blocked = false;
                        if (own_idx_[static_cast<std::size_t>(n)] != -1 && n != tail_cell)
                            blocked = true;
                        if (other_occ_[static_cast<std::size_t>(n)])
                        {
                            unswbc::DragonPart const *part = visible_part(s.destination);
                            if (!part || pid(part->get_id()) != head.id)
                                blocked = true;
                        }
                        if (!blocked)
                            queue.push_back(s.destination);
                    }
                }
            }
        }

        unswbc::DragonPart const *visible_part(Position position) const
        {
            unswbc::Tile const *tile = controller_.get_tile(position);
            return tile ? tile->get_dragon() : nullptr;
        }

        bool visible_occupied(Position position) const
        {
            std::size_t const c = static_cast<std::size_t>(index(position));
            return other_occ_[c] || own_idx_[c] != -1;
        }

        bool visible_pearl(Position position) const
        {
            unswbc::Tile const *tile = controller_.get_tile(position);
            return tile && tile->has_pearl();
        }

        int open_exits(Position position) const
        {
            int exits = 0;
            for (Direction direction : Direction::get_direction_list())
            {
                Step const step = step_from(position, direction);
                if (!step.passable)
                    continue;
                if (step.destination_known && visible_occupied(step.destination))
                    continue;
                ++exits;
            }
            return exits;
        }

        bool blocks_pearl_path(int c) const
        {
            std::size_t const i = static_cast<std::size_t>(c);
            if (other_occ_[i] || (own_idx_[i] != -1 && own_idx_[i] != 0))
                return true;
            return body_remembered(memory_[i]);
        }

        // v21: out of sight, a body seen there is expected to be there still.
        bool body_remembered(CellMemory const &m) const
        {
            int const now = game_.get_round_num();
            return m.last_seen != now && m.other_seen && now < m.busy_until; // (v21: also predicted, never seen)
        }

        // Rounds until the body on tile `c` (seen now, or remembered) is gone: 0 if none.
        int body_rounds_left(int c) const
        {
            CellMemory const &m = memory_[static_cast<std::size_t>(c)];
            return m.other_seen ? std::max(0, m.busy_until - game_.get_round_num()) : 0;
        }

        // A dragon's length as well as we know it: what we see of it, or for an ally, its
        // last status report (if recent).
        int known_length(SeenHead const &h) const
        {
            int len = h.estimated_length();
            if (h.team == controller_.get_team())
            {
                auto const found = ally_length_cache_.find(h.id);
                if (found != ally_length_cache_.end() && game_.get_round_num() - found->second.second <= 10)
                    len = std::max(len, found->second.first);
            }
            return len;
        }

        // v21: for every other dragon's segment in sight, the round it should be gone by:
        // segment i of a dragon of length len has moved on after len - i of its steps
        // (+1 in case it eats). Segments we cannot place (head out of sight) of an ally
        // whose length we know: half that length. Others keep the default set when seen
        // (OCCUPIED_MEMORY + 1).
        void remember_bodies()
        {
            int const now = game_.get_round_num();
            std::unordered_set<int> placed;
            for (SeenHead const &h : heads_)
            {
                int const len = known_length(h);
                // Our allies take several free steps on most turns: half their free steps
                // a turn on average. Anyone else: one step a turn.
                int const pace = h.team == controller_.get_team() ? std::max(1, free_moves(len) / 2) : 1;
                BodyTrace const trace = trace_body(h.position, h.id);
                for (std::size_t i = 0; i < trace.segments.size(); ++i)
                {
                    int const c = index(trace.segments[i]);
                    placed.insert(c);
                    int const rounds = (len - static_cast<int>(i) + pace - 1) / pace + 1;
                    memory_[static_cast<std::size_t>(c)].busy_until =
                        now + std::min(Config::BODY_MEMORY_MAX, std::max(1, rounds));
                }
            }
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                unswbc::DragonPart const *part = tile.get_dragon();
                if (!part || part->get_id() == controller_.get_id() || part->get_team() != controller_.get_team())
                    continue;
                int const c = index(tile.get_position());
                if (placed.count(c))
                    continue;
                auto const found = ally_length_cache_.find(pid(part->get_id()));
                if (found == ally_length_cache_.end() || now - found->second.second > 10)
                    continue;
                int const len = found->second.first;
                int const rounds = std::clamp(len / 2 / std::max(1, free_moves(len) / 2), Config::OCCUPIED_MEMORY + 1,
                                              Config::UNPLACED_BODY_MEMORY_MAX);
                memory_[static_cast<std::size_t>(c)].busy_until = now + rounds;
            }
            predict_hidden_segments();
        }

        // v21: a body running out of our window goes on out of sight. A segment's facing
        // points to the next segment towards its head, so when that tile is out of sight
        // we know it holds the same dragon; and when no visible segment follows one
        // towards its tail and only one tile out of sight could, that one almost surely
        // does. Those tiles are remembered as taken (as long as the segment next to them),
        // so rushes, the survival search and routing treat them as bodies.
        void predict_hidden_segments()
        {
            int const now = game_.get_round_num();
            auto predict = [&](Position q, int busy_until)
            {
                CellMemory &m = cell(q);
                if (m.last_seen == now)
                    return;
                int const before = m.other_seen && m.busy_until > now ? m.busy_until : now;
                m.other_seen = true;
                m.busy_until = std::max(before, busy_until);
            };
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                unswbc::DragonPart const *part = tile.get_dragon();
                if (!part || part->get_id() == controller_.get_id())
                    continue;
                Position const pos = tile.get_position();
                int const busy = memory_[static_cast<std::size_t>(index(pos))].busy_until;
                // Towards its head.
                Step const ahead = step_from(pos, part->get_dir());
                if (!part->is_head() && ahead.passable && ahead.destination_known && !controller_.get_tile(ahead.destination))
                    predict(ahead.destination, busy);
                // Towards its tail: is it followed by a visible segment?
                bool followed = false;
                std::vector<Position> hidden;
                for (Direction d : Direction::get_direction_list())
                {
                    Step const s = step_from(pos, d);
                    if (!s.passable || !s.destination_known)
                        continue;
                    if (!part->is_head() && s.destination == ahead.destination)
                        continue;
                    if (!controller_.get_tile(s.destination))
                    {
                        hidden.push_back(s.destination);
                        continue;
                    }
                    unswbc::DragonPart const *next = visible_part(s.destination);
                    if (next && next->get_id() == part->get_id() && !next->is_head())
                    {
                        Step const back = step_from(s.destination, next->get_dir());
                        if (back.passable && back.destination_known && back.destination == pos)
                            followed = true;
                    }
                }
                if (!followed && hidden.size() == 1)
                    predict(hidden.front(), busy - 1);
            }
        }

        // An ally that reaches the pearl first will take it; chasing it too wastes a
        // worker. Ties go to the lower id (one rule both of us follow). Growers never
        // yield. v21: in turns, each at its own free speed, by time-aware paths
        // (prepare_etas, route.hpp) for us and the allies we can see.
        bool pearl_taken_by_ally(Position p) const
        {
            if (retaining_length())
                return false;
            std::size_t const c = static_cast<std::size_t>(index(p));
            int const L = controller_.get_length();
            int const mine_steps = c < route_head_dist_.size() && route_head_dist_[c] < INF
                                       ? route_head_dist_[c]
                                       : distance(controller_.get_position(), p);
            int const mine = turns_for(mine_steps, L);
            int const me = my_id();
            for (AllyEta const &a : ally_etas_)
            {
                if (a.steps[c] >= INF)
                    continue; // it cannot get there soon
                int const t = turns_for(a.steps[c], a.length);
                if (t < mine || (t == mine && a.id < me))
                    return true;
            }
            int const now = game_.get_round_num();
            for (TeamSignal const &sig : signals_)
            {
                if (sig.id == me || now - sig.round > 1 || seen_head(sig.id))
                    continue;
                int const t = turns_for(distance(sig.position, p), std::max(sig.length, 1));
                if (t < mine || (t == mine && sig.id < me))
                    return true;
            }
            return false;
        }

        // v23: a bed's mean respawn gap from the countdowns seen on it. The countdown runs
        // g, g-1, ..., 1 after each spawn, so its mean over time is (g+1)/2 for a fixed gap
        // g (about that for a random one); -1 until GOOD_BED_MIN_SAMPLES rounds of it seen.
        static int bed_gap(CellMemory const &m)
        {
            if (m.cd_n < Config::GOOD_BED_MIN_SAMPLES)
                return -1;
            return std::max(1, (2 * m.cd_sum - m.cd_n) / m.cd_n);
        }
        // A good bed refills often: worth coming back to (farming).
        static bool good_bed(CellMemory const &m)
        {
            int const g = bed_gap(m);
            return g >= 1 && g <= Config::GOOD_BED_GAP;
        }

        // One multi-source search per turn from every objective outwards (path
        // distance, through known portals). Objectives: visible pearls (cost 0),
        // remembered pearls (fading with age), tiles about to spawn, unseen tiles whose
        // countdown ran out, and never-seen tiles (exploring). The radius depends on
        // the role: the king and growers see the whole map, so they always head for the
        // nearest thing worth eating instead of circling; feeders do not explore.
        int objective_radius_ = Config::WORKER_OBJECTIVE_RADIUS;

        // v25: dispersion. Early on, a tile left to explore (or a timer that ran out while
        // nobody watched) is ours only if no teammate we know of is closer to it: the visible
        // ones by their path steps (ally_etas_), the others by the last position they sent
        // (at most CLAIM_SIGNAL_AGE rounds old). A tile a teammate reaches first costs
        // CLAIMED_EXTRA more in the objective map, so dragons near each other head for
        // different parts of the map instead of the same one. (Pearls were already left to
        // whoever reaches them first: pearl_taken_by_ally.)
        struct ClaimAlly
        {
            Position at{};
            int id = -1;
            std::vector<int> const *steps = nullptr;
            int age = 0;
        };
        std::vector<ClaimAlly> claim_allies_;
        void gather_claim_allies()
        {
            claim_allies_.clear();
            int const now = game_.get_round_num();
            for (AllyEta const &a : ally_etas_)
                for (SeenHead const &h : heads_)
                    if (h.id == a.id && h.team == controller_.get_team())
                        claim_allies_.push_back(ClaimAlly{h.position, a.id, &a.steps, 0});
            for (TeamSignal const &s : signals_)
                if (s.id != my_id() && !seen_head(s.id) && now - s.round <= Config::CLAIM_SIGNAL_AGE)
                    claim_allies_.push_back(ClaimAlly{s.position, s.id, nullptr, now - s.round});
        }
        bool ally_claims(int c) const
        {
            if (claim_allies_.empty())
                return false;
            std::size_t const ci = static_cast<std::size_t>(c);
            Position const p = position_of(c);
            int const mine = ci < route_head_dist_.size() && route_head_dist_[ci] < INF ? route_head_dist_[ci]
                                                                                        : distance(controller_.get_position(), p);
            int const me = my_id();
            for (ClaimAlly const &a : claim_allies_)
            {
                int const theirs = a.steps && (*a.steps)[ci] < INF ? (*a.steps)[ci] : distance(a.at, p) + a.age;
                if (theirs < mine || (theirs == mine && a.id < me))
                    return true;
            }
            return false;
        }

        // v25: anticipated pearls. Every tile in view shows when its next pearl comes, and we
        // remember it (CellMemory pearl_time, pearl_sample_round). Before, a bed was an
        // objective only within SPAWN_SOON_ROUNDS (4; the king 10) of spawning, at cost
        // rounds left + 1 plus the walk. Now a known future spawn becomes an objective when it
        // is time to set off for it: when the rounds left are at most our turns to get there
        // plus FORECAST_MAX_WAIT, so we arrive as it spawns rather than early (dragons that
        // went early circled on the beds and blocked them: they ate less of a burst than
        // before). Its source cost is that wait, max(0, rounds left + 1 - our turns there);
        // the search adds the walk. Spawns bunched in place and time (a burst: Prisoners
        // Dilemma's two middle columns all spawn around round 480) cost less: one round per
        // FORECAST_CLUSTER_DIV other spawns within FORECAST_CLUSTER_RADIUS tiles and
        // FORECAST_CLUSTER_WINDOW rounds of it, at most FORECAST_CLUSTER_CAP. The objective
        // radius (30 steps for workers, 200 for the king) bounds how far anyone goes.
        int spawn_forecast_cost(int c, int remaining) const
        {
            Position const p = position_of(c);
            std::size_t const ci = static_cast<std::size_t>(c);
            int const steps = ci < route_head_dist_.size() && route_head_dist_[ci] < INF ? route_head_dist_[ci]
                                                                                         : distance(controller_.get_position(), p);
            int const wait = std::max(0, remaining + 1 - turns_for(steps, controller_.get_length()));
            if (wait > Config::FORECAST_MAX_WAIT)
                return INF; // too early to set off: we would wait there, on the beds (blocking them)
            if (wait == 0)
                return 0;
            int const now = game_.get_round_num();
            int const at = now + remaining;
            int const r = Config::FORECAST_CLUSTER_RADIUS;
            int bunch = 0;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                {
                    if (dx == 0 && dy == 0)
                        continue;
                    CellMemory const &o = cell(Position(wrap(p.x + dx, width_), wrap(p.y + dy, height_)));
                    if (!o.seen || o.pearl || o.pearl_time < 0 || o.pearl_sample_round < 0)
                        continue;
                    if (std::abs(o.pearl_sample_round + o.pearl_time - at) <= Config::FORECAST_CLUSTER_WINDOW)
                        ++bunch;
                }
            return std::max(0, wait - std::min(Config::FORECAST_CLUSTER_CAP, bunch / Config::FORECAST_CLUSTER_DIV));
        }

        // v24: the chance a blind portal crossing lands on a body (Config::P_BLIND_EARLY).
        double blind_portal_p() const
        {
            int const r = game_.get_round_num();
            if (r <= Config::PORTAL_RISK_EARLY_ROUND)
                return Config::P_BLIND_EARLY;
            if (r >= Config::PORTAL_RISK_LATE_ROUND)
                return Config::P_BLIND_LATE;
            return Config::P_BLIND_EARLY + (Config::P_BLIND_LATE - Config::P_BLIND_EARLY) *
                                               (r - Config::PORTAL_RISK_EARLY_ROUND) /
                                               (Config::PORTAL_RISK_LATE_ROUND - Config::PORTAL_RISK_EARLY_ROUND);
        }
        std::vector<int> objective_src_round_; // v23: the round a tile was last an objective

        // v23: a pearl in a portal pocket. On Portals most bed blocks are 1-tile cells whose only
        // side out is a portal: a dragon goes in, eats, and cannot turn round (its neck is in
        // the portal), so it dies there unless it is long enough to split out. The pull towards
        // such a pearl walked small workers up to the portal, the crossing was refused (blind),
        // and they paced in front of it for dozens of rounds. Such a pearl is an objective only
        // when a pocket raid is allowed and the pocket holds POCKET_MIN_PEARLS, as for kelp
        // pockets. A pocket here: the tiles reachable from it without a portal are few (at most
        // PORTAL_POCKET_MAX_CELLS), all known, and too small to turn round in or a dead end.
        bool portal_pocket_worth(int start) const
        {
            if (!nbr_valid_)
                return true;
            int const cap = Config::PORTAL_POCKET_MAX_CELLS;
            std::array<int, Config::PORTAL_POCKET_MAX_CELLS> seen{};
            int n = 0, edges = 0, pearls = 0;
            bool portal = false;
            seen[static_cast<std::size_t>(n++)] = start;
            for (int q = 0; q < n; ++q)
            {
                int const c = seen[static_cast<std::size_t>(q)];
                CellMemory const &m = memory_[static_cast<std::size_t>(c)];
                if (m.pearl)
                    ++pearls;
                for (int d = 0; d < 4; ++d)
                {
                    int const nn = nb(c, d);
                    if (nn == NBR_WALL)
                        continue;
                    if (nbr_portal_[static_cast<std::size_t>(4 * c + d)])
                    {
                        portal = true;
                        continue;
                    }
                    if (nn == NBR_BLIND || nn == index(controller_.get_position()))
                        return true; // not known all round, or we are in it already
                    ++edges;         // (each inner edge is counted from both ends)
                    bool dup = false;
                    for (int i = 0; i < n; ++i)
                        if (seen[static_cast<std::size_t>(i)] == nn)
                            dup = true;
                    if (dup)
                        continue;
                    if (n >= cap)
                        return true; // bigger than a pocket
                    seen[static_cast<std::size_t>(n++)] = nn;
                }
            }
            if (!portal)
                return true;
            bool const dead_end = edges / 2 < n; // no loop in it
            bool const trap = dead_end || n < controller_.get_length() + 3;
            if (!trap)
                return true;
            return pocket_plan_allowed() && pearls >= Config::POCKET_MIN_PEARLS;
        }

        // v23: a portal on the way is taken or dropped. The searches behind the goals (pearl
        // route, objective map) treat a crossing as an ordinary step, but the move itself
        // prices a crossing whose far tile we cannot see as a 15-30% death. So the goal led
        // workers up to a portal, the crossing lost to the risk, they turned away, the goal
        // led them back: dozens of rounds pacing in front of portals (simulator, Portals and
        // Default). When the goal's best way on is a crossing and we choose another move, the
        // far tile is marked as a trap for PORTAL_REFUSE_ROUNDS: the searches route around
        // it, and the goal picks something else meanwhile.
        void refuse_route_portals(std::vector<Candidate> const &candidates, Candidate const &best)
        {
            int const now = game_.get_round_num();
            for (Candidate const &c : candidates)
            {
                if (!c.route_portal || &c == &best || c.steps.empty() || best.steps.empty() ||
                    c.steps.front() == best.steps.front())
                    continue;
                trap_mark_[static_cast<std::size_t>(index(c.destination))] = now + Config::PORTAL_REFUSE_ROUNDS;
#ifdef BOT_DEBUG
                controller_.output_log("PORTAL_REFUSED", c.destination.x, c.destination.y, c.score, best.score);
#endif
            }
        }

        // v23: shut in a portal room. Default's centre, Autarky's 3x3 boxes and Portals' cells
        // are kelp-sealed rooms whose only ways out are portals. Every exit is then a crossing
        // whose far tile we cannot see, priced at 30% death (v23), so a worker that
        // had eaten what was there circled the room for the rest of the game (100+ rounds in
        // the simulator). We are shut in when the tiles we can reach without a portal (at most
        // PORTAL_ROOM_MAX_CELLS, all seen) hold no objective at all and a portal leads out:
        // then a crossing is priced at P_ROOM_EXIT and is worth ROOM_EXIT_VALUE (portal_value).
        bool stuck_in_room_ = false;
        void update_portal_room()
        {
            stuck_in_room_ = false;
            if (is_queen() || !nbr_valid_)
                return; // (a room is a fine place for her)
            int const now = game_.get_round_num();
            std::size_t const cells = memory_.size();
            if (objective_src_round_.size() != cells)
                return;
            ++generation_;
            int const gen = generation_;
            int const start = index(controller_.get_position());
            std::vector<int> queue{start};
            stamp_[static_cast<std::size_t>(start)] = gen;
            bool exit = false;
            for (std::size_t q = 0; q < queue.size(); ++q)
            {
                int const c = queue[q];
                if (objective_src_round_[static_cast<std::size_t>(c)] == now)
                    return; // something to do here
                for (int d = 0; d < 4; ++d)
                {
                    int const n = nb(c, d);
                    if (n == NBR_WALL)
                        continue;
                    if (nbr_portal_[static_cast<std::size_t>(4 * c + d)])
                    {
                        exit = true;
                        continue;
                    }
                    if (n == NBR_BLIND)
                        return; // unexplored: not shut in
                    if (stamp_[static_cast<std::size_t>(n)] == gen)
                        continue;
                    stamp_[static_cast<std::size_t>(n)] = gen;
                    queue.push_back(n);
                    if (static_cast<int>(queue.size()) > Config::PORTAL_ROOM_MAX_CELLS)
                        return; // open water
                }
            }
            stuck_in_room_ = exit;
        }
        int dbg_good_beds_ = 0, dbg_good_expired_ = 0;
        void build_objective_map()
        {
            std::fill(objective_dist_.begin(), objective_dist_.end(), INF);
            if (objective_src_round_.size() != memory_.size())
                objective_src_round_.assign(memory_.size(), -1);
            int const now = game_.get_round_num();
            bool const keeper = retaining_length();
            bool const feeder = !keeper && now >= Config::GATHER_ROUND && feeder_target_.has_value();
            int const max_cost = keeper   ? Config::KEEPER_OBJECTIVE_RADIUS
                                 : feeder ? Config::FEEDER_OBJECTIVE_RADIUS
                                          : Config::WORKER_OBJECTIVE_RADIUS;
            objective_radius_ = max_cost;
            int const explore_cost = keeper ? Config::KEEPER_EXPLORE_COST : feeder ? INF
                                                                                   : Config::WORKER_EXPLORE_COST;
            int const pearl_max_age = keeper ? Config::KEEPER_REMEMBERED_PEARL_MAX_AGE : Config::REMEMBERED_PEARL_MAX_AGE;
            int const expired_max_age = keeper ? Config::KEEPER_EXPIRED_TIMER_MAX_AGE : Config::EXPIRED_TIMER_MAX_AGE;
            bool const late = now >= SPECIALISE_ROUND; // no pearl is reserved before this
            std::vector<std::vector<int>> buckets(static_cast<std::size_t>(max_cost + 1));
            bool sourcing = true; // v23: the objectives themselves, not the search spreading from them
            auto add = [&](int c, int cost)
            {
                if (cost > max_cost || cost >= objective_dist_[static_cast<std::size_t>(c)])
                    return;
                if (sourcing)
                    objective_src_round_[static_cast<std::size_t>(c)] = now; // v23 (update_portal_room)
                objective_dist_[static_cast<std::size_t>(c)] = cost;
                buckets[static_cast<std::size_t>(cost)].push_back(c);
            };
            bool const claiming = !keeper && !feeder && now < Config::CLAIM_END_ROUND; // v25
            if (claiming)
                gather_claim_allies();
            else
                claim_allies_.clear();
            int const cells = width_ * height_;
            for (int c = 0; c < cells; ++c)
            {
                CellMemory const &m = memory_[static_cast<std::size_t>(c)];
                if (blocks_pearl_path(c) || own_idx_[static_cast<std::size_t>(c)] == 0 ||
                    trap_mark_[static_cast<std::size_t>(c)] > now)
                    continue;
                if (!m.seen)
                {
                    // Somebody on the team has already covered this part of the map.
                    int cost = block_explored(block_of(position_of(c))) ? explore_cost + Config::EXPLORED_BLOCK_EXTRA : explore_cost;
                    if (claiming && ally_claims(c))
                        cost += Config::CLAIMED_EXTRA; // v25: a teammate is closer
                    add(c, cost);
                    continue;
                }
                Position const p = position_of(c);
                bool const visible = m.last_seen == now;
                if ((m.pearl || m.pearl_time >= 0) && !portal_pocket_worth(c))
                    continue; // v23: a portal pocket we would die in
                if (m.pearl)
                {
                    int const age = now - m.pearl_seen;
                    if (age > pearl_max_age || (late && reserved_food(p)) || pearl_taken_by_ally(p))
                        continue;
                    add(c, age / Config::REMEMBERED_PEARL_AGE_DIV);
                    continue;
                }
                if (m.pearl_time < 0 || m.pearl_sample_round < 0)
                    continue; // never spawns
                int const remaining = m.pearl_sample_round + m.pearl_time - now;
                bool const good = good_bed(m); // v23: farm beds that refill often
                if (remaining > 0)
                {
                    // v25: every spawn we know of is an objective, priced by when we could eat
                    // it (spawn_forecast_cost); before, only those within `soon` rounds.
                    if (!(late && reserved_food(p)) && !pearl_taken_by_ally(p))
                        if (int const fc = spawn_forecast_cost(c, remaining); fc <= max_cost)
                            add(c, fc);
                }
                else if (!visible && -remaining <= (good ? std::max(expired_max_age, Config::GOOD_BED_EXPIRED_MAX_AGE) : expired_max_age) &&
                         !(late && reserved_food(p)))
                {
                    // A good bed that ran out while nobody watched most likely holds a pearl.
                    add(c, (good ? Config::GOOD_BED_EXPIRED_COST : Config::EXPIRED_TIMER_BASE_COST) +
                               std::min(-remaining, Config::EXPIRED_TIMER_AGE_CAP) / Config::EXPIRED_TIMER_AGE_DIV +
                               (claiming && ally_claims(c) ? Config::CLAIMED_EXTRA : 0)); // v25
#ifdef BOT_DEBUG
                    dbg_good_expired_ += good ? 1 : 0;
#endif
                }
#ifdef BOT_DEBUG
                dbg_good_beds_ += good ? 1 : 0;
#endif
            }
            // Portals we have seen only one end of: exploring for workers, early on.
            if (!feeder && portal_exploring())
                for (auto const &[id, ends] : portal_edges_)
                {
                    if (ends.size() != 1)
                        continue;
                    for (auto const &[pp, pd] : edge_sides(ends[0]))
                    {
                        int const c = index(pp);
                        if (!blocks_pearl_path(c) && own_idx_[static_cast<std::size_t>(c)] != 0 &&
                            trap_mark_[static_cast<std::size_t>(c)] <= now)
                            add(c, Config::UNKNOWN_PORTAL_EXPLORE_COST);
                    }
                }
            sourcing = false;
            for (int d = 0; d < max_cost; ++d)
            {
                auto &bucket = buckets[static_cast<std::size_t>(d)];
                for (std::size_t k = 0; k < bucket.size(); ++k)
                {
                    int const c = bucket[k];
                    if (objective_dist_[static_cast<std::size_t>(c)] != d)
                        continue;
                    Position const p = position_of(c);
                    for (Direction dir : Direction::get_direction_list())
                    {
                        Step const st = step_from(p, dir);
                        if (!st.passable || !st.destination_known)
                            continue;
                        int const n = index(st.destination);
                        if (blocks_pearl_path(n) || trap_mark_[static_cast<std::size_t>(n)] > now)
                            continue;
                        add(n, d + 1);
                    }
                }
            }
        }

        std::pair<int, int> nearest_visible_enemy_delta(Position before,
                                                        Position after) const
        {
            int old_distance = INF;
            int new_distance = INF;
            for (SeenHead const &head : heads_)
            {
                if (head.team == controller_.get_team())
                    continue;
                old_distance = std::min(old_distance, distance(before, head.position));
                new_distance = std::min(new_distance, distance(after, head.position));
            }
            return {old_distance, new_distance};
        }

        // Keeping out of each other's way. v20: in thousandths of a segment (the old
        // weights / 2.5, as a pearl used to be worth 2500).
        int teammate_spacing_score(Position destination) const
        {
            int score = 0;
            for (SeenHead const &head : heads_)
            {
                if (head.team != controller_.get_team())
                    continue;
                // NEW: Do not repel from the grower we are trying to feed
                if (feeder_target_ && head.id == feeder_target_->id)
                    continue;

                int const gap = chebyshev_distance(destination, head.position);
                if (gap <= 1)
                    score -= 360;
                else if (gap == 2)
                    score -= 104;
                else if (gap >= 4)
                    score += 8;
                if (head.visible_length > controller_.get_length() && gap <= 3)
                    score -= 72;
            }
            for (TeamSignal const &signal : signals_)
            {
                if (signal.id == my_id() || seen_head(signal.id))
                    continue; // a visible head was already counted above

                // NEW: Do not repel from the grower we are trying to feed!
                if (feeder_target_ && signal.id == feeder_target_->id)
                    continue;

                int const gap = chebyshev_distance(destination, signal.position);
                if (gap <= 1)
                    score -= 280;
                else if (gap == 2)
                    score -= 72;
                if (signal.length > controller_.get_length() && gap <= 3)
                    score -= 96;
            }
            return score;
        }

        int deterministic_jitter(Direction direction) const
        {
            Position const position = controller_.get_position();
            std::uint64_t seed = static_cast<std::uint64_t>(controller_.get_id());
            seed = seed * 1000003ULL + static_cast<std::uint64_t>(game_.get_round_num());
            seed = seed * 1009ULL + static_cast<std::uint64_t>(position.x * 131 + position.y);
            seed = seed * 17ULL + static_cast<unsigned char>(direction.value);
            return static_cast<int>(mix(seed) % 101ULL);
        }

        // ---------------------------------------------------------------
        // v11: aggression
        // ---------------------------------------------------------------

        bool eligible_target(SeenHead const &h) const
        {
            int const round = game_.get_round_num();
            if (h.team == controller_.get_team() || retaining_length())
                return false;
            int const est = h.estimated_length();
            if (round < Config::AGGRESSION_START_ROUND) // v26.3.0 (Leo's early trade)
                return Config::EARLY_TRADE && !valuable_now_ && !is_queen() && est >= controller_.get_length() &&
                       controller_.get_unit_count() >= Config::EARLY_TRADE_MIN_UNITS;
            // v14: never trade into an enemy 4 or more shorter than us, in any mode or round.
            if (est <= controller_.get_length() - Config::TRADE_MAX_SIZE_GAP)
                return false;
            // Endgame: their long dragon decides the tiebreak; anything that is not
            // one of our top two may trade into it.
            if (round >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                return true;
            if (valuable_now_ || controller_.get_unit_count() < Config::MIN_UNITS_FOR_TRADE)
                return false;
            int const L = controller_.get_length();
#if AGGR_MODE == 2
            return L <= Config::HEAD_TRADE_MAX_LENGTH || est >= L;
#else
            return L <= Config::HEAD_TRADE_MAX_LENGTH || est >= L + Config::FAVOURABLE_MARGIN;
#endif
        }

        // Worth walking towards, not just trading with when adjacent.
        bool profitable_target(SeenHead const &h) const
        {
            int const est = h.estimated_length();
            if (game_.get_round_num() >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                return true;
#if AGGR_MODE == 2
            return est >= std::max(controller_.get_length() + 2, Config::ENEMY_TARGET_MIN_LENGTH);
#else
            return controller_.get_unit_count() >= Config::HUNT_MIN_UNITS &&
                   est >= std::max(controller_.get_length() + Config::HUNT_MARGIN, Config::HUNT_MIN_TARGET_LENGTH);
#endif
        }

        // Multi-step trades spend nothing extra (we die either way), but only chase
        // targets that are worth more than we are, or equals within the old 2-step reach.
        bool sprint_target(SeenHead const &h, int steps) const
        {
            int const est = h.estimated_length();
            if (game_.get_round_num() >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                return true;
#if AGGR_MODE == 2
            (void)steps;
            return true;
#else
            return est >= controller_.get_length() + Config::FAVOURABLE_MARGIN ||
                   (steps <= 2 && controller_.get_length() <= 4);
#endif
        }

        // ---------------------------------------------------------------

        // v20: gathers the facts about a single step (what is there, the room and risks
        // where it ends, whether it is a trade); move_value (scoring.hpp) prices them.
        Candidate evaluate_single(Direction direction) const
        {
            Candidate candidate;
            candidate.steps.push_back(direction);
            Position const origin = controller_.get_position();
            Step const step = step_from(origin, direction);
            if (!step.passable)
                return candidate;

            candidate.destination_known = step.destination_known;
            candidate.portal = step.portal;
            candidate.unknown_portal = step.portal && !step.destination_known;
            candidate.destination = step.destination_known ? step.destination : origin;
            bool const blind_portal = step.portal && step.destination_known &&
                                      !controller_.get_tile(step.destination);
            int const now = game_.get_round_num();
            // An exit we cannot see: a body seen there a moment ago is probably still there.
            bool exit_occupied = false;
            if (blind_portal)
            {
                CellMemory const &m = cell(step.destination);
                exit_occupied = body_remembered(m);
            }

            unswbc::DragonPart const *occupant =
                step.destination_known ? visible_part(step.destination) : nullptr;
            if (!occupant && step.destination_known && visible_occupied(step.destination))
                return candidate;
            int trade = 0;
            if (occupant)
            {
                bool const enemy_head = occupant->is_head() &&
                                        occupant->get_team() != controller_.get_team();
                SeenHead const *target = enemy_head ? seen_head(occupant->get_id()) : nullptr;
                if (!target || !eligible_target(*target))
                    return candidate;
                candidate.head_trade = true;
                trade = trade_value(*target);
            }

            candidate.direct_pearl = step.destination_known && visible_pearl(step.destination);
            candidate.new_length = controller_.get_length() + (candidate.direct_pearl ? 1 : 0);
            bool trap = false;
            PostMove pm;
            if (step.destination_known)
            {
                pm.head = step.destination;
                pm.path.push_back(step.destination);
                pm.steps = 1;
                pm.eaten = candidate.direct_pearl ? 1 : 0;
                pm.length = candidate.new_length;
                std::size_t const destination_index = static_cast<std::size_t>(index(step.destination));
                candidate.enemy_threats = enemy_threats_[destination_index];
                candidate.ally_threats = ally_threats_[destination_index];
                candidate.hard_threats = hard_threat_[destination_index];
                candidate.soft_threats = soft_threat_[destination_index];
                if (!candidate.head_trade)
                {
                    candidate.exits = post_exits(pm);
                    candidate.space = timed_space(pm, space_cap(pm.length), false, &candidate.region);
                    // v13: a closed region we cannot turn around in is a trap however
                    // deep it is. A worker may still raid it when it holds enough pearls
                    // to pay for the split that gets us out again; nobody else enters.
                    int const need = pm.length + unknown_body(); // room our whole body needs
                    trap = candidate.region.trap(need);
                    bool const tight = candidate.exits == 0 || candidate.space < need + 2 || trap;
                    candidate.pocket_plan = tight && !candidate.portal && pocket_plan_allowed() &&
                                            candidate.region.closed() &&
                                            candidate.region.pearls >= Config::POCKET_MIN_PEARLS &&
                                            (!valuable_now_ || candidate.hard_threats == 0);
                    // A dead end's pearls are not worth chasing either: stop the pull from
                    // walking us back to its entrance every turn.
                    if (!candidate.pocket_plan && candidate.region.dead_end())
                        trap_mark_[destination_index] = now + Config::TRAP_MARK_ROUNDS;
                    if (valuable_now_)
                        candidate.known_space = timed_space(pm, pm.length + 3, true);
                    // v21: can we keep going from there?
                    SurvivalResult const sr = survival(pm, direction);
                    candidate.dfs_depth = sr.depth;
                    candidate.dfs_target = sr.target;
                    candidate.dfs_complete = sr.complete;
                    candidate.walled = sr.walled;         // v23
                    candidate.via_portal = sr.via_portal; // v26
                }
            }

            candidate.score = move_value(candidate, pm, direction, step, blind_portal, exit_occupied, trade);
            // v23: the goal's way on goes through this portal (see refuse_route_portals).
            if (candidate.portal && step.destination_known && !candidate.head_trade && !pm.path.empty())
                candidate.route_portal = goal_progress(goal_ == Goal::FIGHT ? base_goal_ : goal_, candidate, pm) > 0;

            // v21: a survival search that reaches its target proves the room; one that
            // searched everything and fell well short proves a trap. Otherwise (cut
            // short by its budget) the room estimate decides, as before.
            bool const room_ok = candidate.dfs_ok() ||
                                 (!candidate.dfs_dead() && candidate.exits > 0 &&
                                  candidate.space >= pm.length + unknown_body() + 2 && !trap);
            candidate.critical_ok = candidate.destination_known && !candidate.unknown_portal &&
                                    !candidate.head_trade && room_ok && !exit_occupied &&
                                    (!valuable_now_ || (candidate.hard_threats == 0 && (!blind_portal || stuck_in_room_)));
            // v16: early on, for a worker, a portal it has never seen through is an
            // ordinary move: it is how the team learns where portals lead.
            bool const worker_portal = candidate.unknown_portal && (portal_exploring() || stuck_in_room_);
            // A pocket raid is deliberate: it passes the safety filter like a safe move.
            // v20: in a fight, an enemy head next to the tile is for the fight search to
            // judge (it plays out the head-on), not a reason to call the move unsafe.
            candidate.safe = (candidate.critical_ok || candidate.pocket_plan || worker_portal) &&
                             (candidate.enemy_threats == 0 || goal_ == Goal::FIGHT);
            return candidate;
        }

        // Expected pearls (x100) in the 9x9 area around a tile, from memory: pearls we
        // know of (fading with age), tiles about to spawn, tiles whose countdown ran out
        // while nobody watched, and a little for tiles never seen.
        int region_value(Position centre, int unseen_value = 3) const
        {
            int const now = game_.get_round_num();
            int const r = Config::PORTAL_REGION_RADIUS;
            int value = 0;
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx)
                {
                    CellMemory const &m = cell(Position(wrap(centre.x + dx, width_), wrap(centre.y + dy, height_)));
                    if (!m.seen)
                    {
                        value += unseen_value;
                        continue;
                    }
                    if (m.pearl)
                    {
                        int const age = now - m.pearl_seen;
                        if (age <= Config::REMEMBERED_PEARL_MAX_AGE)
                            value += 100 - age;
                        continue;
                    }
                    if (m.pearl_time < 0 || m.pearl_sample_round < 0)
                        continue;
                    int const remaining = m.pearl_sample_round + m.pearl_time - now;
                    if (remaining > 0)
                    {
                        if (remaining <= 10)
                            value += 70 - 3 * remaining;
                    }
                    else if (m.last_seen != now && -remaining <= 100)
                        value += (good_bed(m) ? 80 : 50) + remaining / 2; // v23
                }
            return value;
        }

        std::vector<Candidate> single_step_candidates(bool include_reverse) const
        {
            Direction const facing = controller_.get_dir();
            std::vector<Candidate> candidates;
            for (Direction direction : Direction::get_direction_list())
            {
                if (!include_reverse && direction == facing.get_opposite())
                    continue;
                Candidate move = evaluate_single(direction);
                if (move.score > -INF)
                    candidates.push_back(std::move(move));
            }
            return candidates;
        }

        // Multi-step head trades: any eligible enemy head within sprint reach through
        // free tiles. Never before AGGRESSION_START_ROUND.
        void add_tactical_sprints(std::vector<Candidate> &candidates) const
        {
            int const L = controller_.get_length();
            if (retaining_length() || L < 3 || game_.get_round_num() < Config::AGGRESSION_START_ROUND)
                return;
            int const max_steps = std::min(max_turn_steps(L), Config::MAX_ATTACK_STEPS);
            if (max_steps < 2)
                return;
            bool any = false;
            for (SeenHead const &h : heads_)
                if (eligible_target(h))
                    any = true;
            if (!any)
                return;

            Position const origin = controller_.get_position();
            Direction const facing = controller_.get_dir();
            ++generation_;
            int const gen = generation_;
            int const start = index(origin);
            stamp_[static_cast<std::size_t>(start)] = gen;
            best_[static_cast<std::size_t>(start)] = 0;
            parent_[static_cast<std::size_t>(start)] = -1;
            std::deque<int> queue{start};
            std::unordered_set<int> targeted;
            while (!queue.empty())
            {
                int const c = queue.front();
                queue.pop_front();
                int const depth = best_[static_cast<std::size_t>(c)];
                Position const p = position_of(c);
                for (Direction d : Direction::get_direction_list())
                {
                    if (depth == 0 && d == facing.get_opposite())
                        continue;
                    Step const s = step_from(p, d);
                    if (!s.passable || !s.destination_known || !controller_.get_tile(s.destination))
                        continue;
                    int const n = index(s.destination);
                    unswbc::DragonPart const *part = visible_part(s.destination);
                    if (part)
                    {
                        if (depth + 1 < 2 || !part->is_head() || targeted.count(part->get_id()))
                            continue;
                        SeenHead const *h = seen_head(part->get_id());
                        if (!h || !eligible_target(*h) || !sprint_target(*h, depth + 1))
                            continue;
                        std::vector<Direction> steps{d};
                        for (int at = c; parent_[static_cast<std::size_t>(at)] != -1; at = parent_[static_cast<std::size_t>(at)])
                            steps.push_back(Direction::get_direction_list()[static_cast<std::size_t>(parent_dir_[static_cast<std::size_t>(at)])]);
                        std::reverse(steps.begin(), steps.end());
                        PostMove pm;
                        if (!simulate_path(steps, pm, true))
                            continue;
                        targeted.insert(part->get_id());
                        Candidate attack;
                        attack.steps = steps;
                        attack.destination = s.destination;
                        attack.head_trade = true;
                        attack.new_length = pm.length;
                        // v20: the trade's value; a longer sprint only breaks ties (v21: per
                        // paid step).
                        attack.score = trade_value(*h) - Config::SPRINT_STEP_COST * paid_steps(L, static_cast<int>(steps.size())) +
                                       deterministic_jitter(steps.front()) / 5;
                        attack.safe = false;
                        candidates.push_back(std::move(attack));
                        continue;
                    }
                    if (depth + 1 >= max_steps || stamp_[static_cast<std::size_t>(n)] == gen ||
                        own_idx_[static_cast<std::size_t>(n)] != -1)
                        continue;
                    stamp_[static_cast<std::size_t>(n)] = gen;
                    best_[static_cast<std::size_t>(n)] = depth + 1;
                    parent_[static_cast<std::size_t>(n)] = c;
                    parent_dir_[static_cast<std::size_t>(n)] = direction_index(d);
                    queue.push_back(n);
                }
            }
        }

        // When no single step is critically safe for a valuable dragon, pay segments
        // to sprint out of reach or out of a pocket.
        void add_escape_sprints(std::vector<Candidate> &candidates) const
        {
            if (!valuable_now_)
                return;
            int const L = controller_.get_length();
            int const max_steps = std::min(max_turn_steps(L) - 1, Config::MAX_ESCAPE_STEPS);
            if (max_steps < 2)
                return;
            if (std::any_of(candidates.begin(), candidates.end(),
                            [](Candidate const &c)
                            { return c.critical_ok; }))
                return;
            // Growers also sprint out of tight space; other long dragons only out of
            // a certain kill zone.
            bool const hard_everywhere = std::all_of(candidates.begin(), candidates.end(),
                                                     [](Candidate const &c)
                                                     { return c.head_trade || c.hard_threats > 0; });
            bool const trapped = std::all_of(candidates.begin(), candidates.end(),
                                             [&](Candidate const &c)
                                             { return c.head_trade || c.space < controller_.get_length(); });
            if (!retaining_length() && !hard_everywhere && !trapped)
                return;

            // Breadth-first over step sequences, one sequence per reached tile.
            std::vector<std::vector<Direction>> frontier{{}};
            std::unordered_set<int> reached{index(controller_.get_position())};
            std::vector<Candidate> found;
            for (int depth = 1; depth <= max_steps && !frontier.empty(); ++depth)
            {
                std::vector<std::vector<Direction>> next;
                for (auto const &prefix : frontier)
                {
                    for (Direction d : Direction::get_direction_list())
                    {
                        if (prefix.empty() && d == controller_.get_dir().get_opposite())
                            continue;
                        std::vector<Direction> steps = prefix;
                        steps.push_back(d);
                        PostMove pm;
                        if (!simulate_path(steps, pm, false))
                            continue;
                        // Every step lands where we can see (the first too: not through
                        // a portal into a tile our unseen body may occupy).
                        if (!controller_.get_tile(pm.path.front()))
                            continue;
                        int const cell_index = index(pm.head);
                        if (!reached.insert(cell_index).second)
                            continue;
                        next.push_back(steps);
                        if (depth < 2)
                            continue;
                        Candidate c;
                        c.steps = steps;
                        c.destination = pm.head;
                        c.new_length = pm.length;
                        c.direct_pearl = pm.eaten > 0;
                        c.hard_threats = hard_threat_[static_cast<std::size_t>(cell_index)];
                        c.soft_threats = soft_threat_[static_cast<std::size_t>(cell_index)];
                        c.enemy_threats = enemy_threats_[static_cast<std::size_t>(cell_index)];
                        c.ally_threats = ally_threats_[static_cast<std::size_t>(cell_index)];
                        c.exits = post_exits(pm);
                        c.space = timed_space(pm, space_cap(pm.length), false, &c.region);
                        int const need = pm.length + unknown_body();
                        bool const trap = c.region.trap(need);
                        c.known_space = timed_space(pm, pm.length + 3, true);
                        SurvivalResult const sr = survival(pm, steps.back()); // v21
                        c.dfs_depth = sr.depth;
                        c.dfs_target = sr.target;
                        c.dfs_complete = sr.complete;
                        c.walled = sr.walled;         // v23
                        c.via_portal = sr.via_portal; // v26
                        // v20: segments eaten minus segments paid, then room and risks
                        // where it ends (and the fight from there).
                        int score = deterministic_jitter(steps.front()) / 5;
                        score += (pm.eaten - paid_steps(L, pm.steps)) * SEG;
                        score += position_value(c, pm, goal_ == Goal::FIGHT);
                        if (goal_ == Goal::FIGHT)
                            score += fight_value(pm, steps.back());
                        score += teammate_spacing_score(pm.head);
                        c.critical_ok = c.hard_threats == 0 &&
                                        (c.dfs_ok() || (!c.dfs_dead() && c.exits > 0 && c.space >= need + 2 && !trap));
                        c.safe = c.critical_ok;
                        c.score = score;
                        found.push_back(std::move(c));
                    }
                }
                frontier = std::move(next);
            }
            std::sort(found.begin(), found.end(), [](Candidate const &a, Candidate const &b)
                      { return a.score > b.score; });
            if (found.size() > 4)
                found.resize(4);
            for (Candidate &c : found)
                candidates.push_back(std::move(c));
        }

        // Sonar packets carry a keyed checksum; anything that fails it, or has
        // fields outside the map or a stale round, is ignored.
        static std::uint64_t packet_checksum(std::uint64_t body, int bits)
        {
            return mix(body ^ SONAR_KEY) & ((1ULL << static_cast<unsigned>(bits)) - 1ULL);
        }

        static int length_class(int length)
        {
            return length >= 20 ? 3 : length >= 10 ? 2
                                  : length >= 5    ? 1
                                                   : 0;
        }

        static int class_length(int cls)
        {
            static constexpr std::array<int, 4> lengths{3, 6, 12, 20};
            return lengths[static_cast<std::size_t>(cls & 3)];
        }

        void apply_symmetry_bits(std::uint64_t sym_bits)
        {
            if (confirmed_symmetry_ != SymmetryType::UNKNOWN || sym_bits == 0)
                return;
            if (sym_bits == 1)
                confirmed_symmetry_ = SymmetryType::HORIZONTAL;
            else if (sym_bits == 2)
                confirmed_symmetry_ = SymmetryType::VERTICAL;
            else if (sym_bits == 3)
                confirmed_symmetry_ = SymmetryType::ROTATIONAL;
        }

        std::uint64_t symmetry_bits() const
        {
            if (confirmed_symmetry_ == SymmetryType::HORIZONTAL)
                return 1;
            if (confirmed_symmetry_ == SymmetryType::VERTICAL)
                return 2;
            if (confirmed_symmetry_ == SymmetryType::ROTATIONAL)
                return 3;
            return 0;
        }

        void receive_status()
        {
            int const now = game_.get_round_num();
            std::erase_if(signals_, [&](auto const &s)
                          { return now - s.round > 2; });
            int const own_team = controller_.get_team().value == unswbc::Team::B ? 1 : 0;

            for (std::uint64_t message : controller_.get_sonar_messages())
            {
                std::uint64_t const magic = message >> 60U;
                int const encoded_team = static_cast<int>((message >> 59U) & 1ULL);
                if (encoded_team != own_team)
                    continue;

                if (magic == 0xEULL)
                {
                    receive_beacon(message);
                }
                else if (magic == 0xBULL)
                {
                    receive_portal(message);
                }
                else if (magic == 0x9ULL)
                {
                    receive_explore(message);
                }
                else if (magic == 0xAULL)
                {
                    // Body handover from the dragon we split from: only on our first turn.
                    if (game_.get_round_num() == first_round_)
                        body_msgs_.push_back(message);
                }
                else if (magic == 0xDULL)
                {
                    // STATUS: checksum in bits 0-4. A sonar is read at most one round
                    // after it is sent, so anything older is not ours.
                    if ((message & 31ULL) != packet_checksum(message & ~31ULL, 5))
                        continue;
                    TeamSignal signal;
                    signal.grower = ((message >> 58U) & 1ULL) != 0;
                    signal.position.x = static_cast<int>((message >> 52U) & 63ULL);
                    signal.position.y = static_cast<int>((message >> 46U) & 63ULL);
                    signal.length = static_cast<int>((message >> 37U) & 511ULL);
                    int const age = (now - static_cast<int>((message >> 34U) & 7ULL)) & 7;
                    signal.id = static_cast<int>((message >> 22U) & 4095ULL);
                    bool const has_enemy = ((message >> 21U) & 1ULL) != 0;
                    Position const enemy_pos(static_cast<int>((message >> 15U) & 63ULL),
                                             static_cast<int>((message >> 9U) & 63ULL));
                    if (age > 1 || signal.position.x >= width_ || signal.position.y >= height_ ||
                        signal.length < 2 ||
                        (has_enemy && (enemy_pos.x >= width_ || enemy_pos.y >= height_)))
                        continue;
                    signal.round = now - age;
                    apply_symmetry_bits((message >> 5U) & 3ULL);
                    if (signal.id != my_id())
                    {
                        remember_ally_length(signal.id, signal.length, signal.round);
                        if (signal.grower || signal.id == king_.id)
                            offer_king(signal.id, signal.length, signal.position, signal.round);
                        std::erase_if(signals_, [&](auto const &old)
                                      { return old.id == signal.id; });
                        signals_.push_back(signal);
                    }
                    if (has_enemy)
                    {
                        int const enemy_length = class_length(static_cast<int>((message >> 7U) & 3ULL));
                        if (!enemy_report_ || now > enemy_report_->round || enemy_length > enemy_report_->length)
                            enemy_report_ = TeamSignal{-1, enemy_pos, enemy_length, now - age, false};
                    }
                }
            }
        }

        void send_status(Position predicted_position, int predicted_length)
        {
            if (Config::SONAR_STRATEGY == 0 || game_.get_round_num() % Config::SONAR_FREQUENCY != 0)
                return;

            // Unified packet:
            // 63-60 magic 0xD | 59 team | 58 grower | 57-52 x | 51-46 y | 45-37 length (max 511)
            // 36-34 round&7 | 33-22 id | 21 enemy flag | 20-15 enemy x | 14-9 enemy y
            // 8-7 enemy length class | 6-5 symmetry | 4-0 checksum
            std::uint64_t m = 0xDULL << 60U;
            m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
            m |= std::uint64_t(game_.get_round_num() < SPECIALISE_ROUND ? selected_grower() : retaining_length()) << 58U;
            m |= std::uint64_t(predicted_position.x & 63) << 52U;
            m |= std::uint64_t(predicted_position.y & 63) << 46U;
            m |= std::uint64_t(std::min(511, predicted_length)) << 37U;
            m |= std::uint64_t(game_.get_round_num() & 7) << 34U;
            m |= std::uint64_t(controller_.get_id() & 4095) << 22U;
            if (enemy_report_ && game_.get_round_num() - enemy_report_->round <= 2)
            {
                m |= 1ULL << 21U;
                m |= std::uint64_t(enemy_report_->position.x & 63) << 15U;
                m |= std::uint64_t(enemy_report_->position.y & 63) << 9U;
                m |= std::uint64_t(length_class(enemy_report_->length)) << 7U;
            }
            m |= symmetry_bits() << 5U;
            m |= packet_checksum(m, 5);

            int const round = game_.get_round_num();
            bool const beacon = king_fresh() && round >= Config::BEACON_START_ROUND;
            int const beacon_dir = (round + controller_.get_id()) % 4;
            // Portal pairs every few rounds while the swarm spreads, on a slot the
            // beacon does not use before BEACON_DOUBLE_ROUND.
            int const portal_dir = (beacon_dir + 2) % 4;
            // Coverage maps every other round while the swarm spreads.
            int const explore_dir = (beacon_dir + 1) % 4;
            bool const explore_share = Config::EXPLORE_SHARING && round < SPECIALISE_ROUND &&
                                       (round + controller_.get_id()) % 2 == 0 && team_explored_ != 0;
            std::uint64_t portal_msg = 0;
            if (Config::PORTAL_SHARING && round < Config::GATHER_ROUND &&
                (round + controller_.get_id()) % Config::PORTAL_SHARE_PERIOD == 0)
                portal_msg = next_portal_message(round / Config::PORTAL_SHARE_PERIOD);
            for (Direction direction : Direction::get_direction_list())
            {
                int const di = direction_index(direction);
                // King beacons relayed on one direction (three late); the unified
                // packet otherwise.
                if (beacon && (di == beacon_dir ||
                               (round >= Config::BEACON_DOUBLE_ROUND && di != (beacon_dir + 3) % 4)))
                    controller_.send_sonar(direction, beacon_message());
                else if (portal_msg && di == portal_dir)
                    controller_.send_sonar(direction, portal_msg);
                else if (explore_share && di == explore_dir)
                    controller_.send_sonar(direction, explore_message());
                else
                    controller_.send_sonar(direction, m);
            }
        }

        // Portal pairs (magic 0xB): a dragon that has seen both ends of a pair tells
        // the others, so a dragon that never saw the far end can still route through
        // it (the objective map follows portals) and judge what lies beyond.
        //  59 team | 58-43 portal id | 42 A horizontal | 41-36 A x | 35-30 A y
        //  29 B horizontal | 28-23 B x | 22-17 B y | 16-5 zero | 4-0 checksum
        std::uint64_t next_portal_message(int turn) const
        {
            int complete = 0;
            for (auto const &[id, ends] : portal_edges_)
                if (ends.size() == 2 && id >= 0 && id < 65536)
                    ++complete;
            if (complete == 0)
                return 0;
            int pick = turn % complete;
            for (auto const &[id, ends] : portal_edges_)
            {
                if (ends.size() != 2 || id < 0 || id >= 65536)
                    continue;
                if (pick-- > 0)
                    continue;
                std::uint64_t msg = 0xBULL << 60U;
                msg |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
                msg |= std::uint64_t(id) << 43U;
                auto put = [&](EdgeKey const &e, unsigned shift)
                {
                    msg |= (std::uint64_t(e.horizontal ? 1 : 0) << 12U | std::uint64_t(e.x & 63) << 6U |
                            std::uint64_t(e.y & 63))
                           << shift;
                };
                put(ends[0], 30U);
                put(ends[1], 17U);
                msg |= packet_checksum(msg, 5);
                return msg;
            }
            return 0;
        }

        // The two tiles that share an edge, and the side of each it lies on.
        std::array<std::pair<Position, Direction>, 2> edge_sides(EdgeKey const &e) const
        {
            if (e.horizontal)
                return {{{Position(e.x, e.y), Direction::NORTH}, {Position(e.x, wrap(e.y - 1, height_)), Direction::SOUTH}}};
            return {{{Position(e.x, e.y), Direction::WEST}, {Position(wrap(e.x - 1, width_), e.y), Direction::EAST}}};
        }

        void receive_portal(std::uint64_t message)
        {
            if ((message & 31ULL) != packet_checksum(message & ~31ULL, 5) || ((message >> 5U) & 0xFFFULL) != 0)
                return;
            int const id = static_cast<int>((message >> 43U) & 0xFFFFULL);
            auto get = [&](unsigned shift)
            {
                std::uint64_t const v = message >> shift;
                return EdgeKey{((v >> 12U) & 1ULL) != 0, static_cast<int>((v >> 6U) & 63ULL), static_cast<int>(v & 63ULL)};
            };
            EdgeKey const a = get(30U), b = get(17U);
            // A crossing keeps its direction, so both ends of a pair face the same way.
            if (a.x >= width_ || a.y >= height_ || b.x >= width_ || b.y >= height_ || a == b ||
                a.horizontal != b.horizontal)
                return;
            // It must agree with everything we have seen ourselves.
            if (auto const found = portal_edges_.find(id); found != portal_edges_.end())
                for (EdgeKey const &e : found->second)
                    if (!(e == a) && !(e == b))
                        return;
            for (EdgeKey const &e : {a, b})
                for (auto const &[p, d] : edge_sides(e))
                {
                    RememberedEdge const &known = cell(p).edges[static_cast<std::size_t>(direction_index(d))];
                    if (known.known && (known.type != EdgeType::PORTAL || known.portal_id != id))
                        return;
                }
#ifdef BOT_DEBUG
            if (portal_edges_[id].size() < 2)
                controller_.output_log("PLEARN", id);
#endif
            for (EdgeKey const &e : {a, b})
            {
                remember_portal(id, e);
                for (auto const &[p, d] : edge_sides(e))
                {
                    RememberedEdge &known = cell(p).edges[static_cast<std::size_t>(direction_index(d))];
                    if (!known.known)
                    {
                        known = {true, EdgeType::PORTAL, id};
                        nbr_all_dirty_ = true; // v22
                        nbr_valid_ = false;
                    }
                }
            }
        }
    };

} // namespace

int main()
{
    // v22: a turn's points run from reading its round to the flush in end_turn (work
    // after that counts for the next turn); the first turn's include our setup.
    long long turn_start = ReplayDerivedBot::clock_now();
    auto [controller, game] = unswbc::init();
    ReplayDerivedBot bot(controller, game);
    while (true)
    {
#ifdef BOT_PROFILE
        bot.prof_begin();
#endif
        if (!unswbc::update(controller, game))
            break;
#if !defined(__wasm__) && !defined(__wasm32__)
        // Native builds (local tests): waiting for the next round is not free time here
        // (other dragons are moving), so the turn starts once the round is read.
        turn_start = ReplayDerivedBot::clock_now();
#endif
        bot.start_turn_clock(turn_start);
#ifdef BOT_PROFILE
        bot.prof(0);
#endif
#ifdef BOT_DEBUG
        auto const t0 = std::chrono::steady_clock::now();
#endif
        bot.play_turn();
#ifdef BOT_PROFILE
        bot.prof(17);
        bot.prof_dump();
#endif
#ifdef BOT_DEBUG
        auto const us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
        controller.output_log("TIME", static_cast<long long>(us), controller.get_length());
#endif
        unswbc::end_turn();
        turn_start = ReplayDerivedBot::clock_now();
    }
    return 0;
}