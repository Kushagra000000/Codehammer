#pragma once
// bot-v15.0 tunable constants. Every constant has a one-line comment; lanes append only.
#include <cstdint>

namespace Config
{
    constexpr int SPLIT_CUTOFF = 250, FINAL_GROWTH_CUTOFF = 425;  // growers retain from SPLIT_CUTOFF; final growth phase from FINAL_GROWTH_CUTOFF
    constexpr int TARGET_SWARM_SIZE = 64, MAX_SWARM_SIZE = 64;  // desired and hard upper swarm size for worker splits
    constexpr int GROWER_NUMERATOR = 1, GROWER_DENOMINATOR = 8;  // fraction of dragons hash-picked as growers
    constexpr int GROWER_SELECTION = 0; // 0 stable hash, 1 ID modulo
    constexpr int MAX_PRE_GROWTH_LENGTH = 3;  // dragons this short never split
    constexpr int PORTAL_EXPLORATION_PERCENT = 0; // disabled; not a score bonus
    constexpr int PORTAL_REWARD = 250, GROWER_PORTAL_REWARD = 35;  // bonus for stepping through a known portal (worker / grower)
    constexpr int UNKNOWN_PORTAL_PENALTY = 90, GROWER_UNKNOWN_PORTAL_PENALTY = 1200;  // penalty for a portal with an unknown exit (worker / grower)
    constexpr int EXPLORATION_WEIGHT = 10, GROWER_EXPLORATION_WEIGHT = 2; // NEW: increased worker exploration weight
    constexpr int EXPLORATION_CAP = 800, GROWER_EXPLORATION_CAP = 100;    // NEW: increased worker exploration cap
    constexpr int VISIT_PENALTY = 28, GROWER_VISIT_PENALTY = 8;  // penalty per previous visit of the destination (worker / grower)
    constexpr int SPACE_PENALTY = 1100, GROWER_SPACE_PENALTY = 4800;  // legacy space penalties (worker / grower)
    constexpr int ENEMY_THREAT_PENALTY = 260, GROWER_ENEMY_THREAT_PENALTY = 1700;  // penalty per enemy head adjacent to the destination (worker / grower)
    // Allies never step onto a visible part, so an ally head next to a tile is congestion,
    // not danger. (Was 1600, which made workers skip pearls beside teammates.)
    constexpr int ALLY_CONGESTION_PENALTY = 300;  // penalty per ally head adjacent to the destination
    constexpr int HEAD_TRADE_MAX_LENGTH = 3, SPRINT_TRADE_MAX_LENGTH = 4;  // longest dragon that trades heads with an equal; sprint trade length
    constexpr bool KING_ENABLED = true;  // enable king election
    constexpr int CLAIM_MIN_LENGTH = 8, FEED_MIN_GROWER_LENGTH = 10;  // min length to claim territory; min grower length that workers feed
    constexpr int ENEMY_TARGET_MIN_LENGTH = 5;  // min enemy length worth reporting or hunting
    constexpr bool PATROL_ENABLED = true;  // enable Hamiltonian patrol of claimed territory
    constexpr int PATROL_BONUS = 3400, FOOD_YIELD_PENALTY = 1800, TERRITORY_PENALTY = 3000;  // patrol bonus; penalty for eating reserved food; penalty for entering others' territory
    constexpr int KING_SELECTION = 0, KING_ID = -1, ROOT_POLICY = 0, BACKUP_KING_POLICY = 0;  // king election and root policies (0 = default)
    constexpr int FEED_START_ROUND = 380, FEEDER_MAX_LENGTH = 4;  // round short dragons start feeding growers; longest feeder
    constexpr int FEEDING_RADIUS = 3, KING_PROTECTION_RADIUS = 3;  // feeding radius; radius protected around the king
    constexpr int SACRIFICE_METHOD = 1; // verified own-body collision; preserves full death yield
    constexpr int SONAR_STRATEGY = 4, SONAR_FREQUENCY = 1;  // sonar packet strategy and send period (rounds)

    // --- v11: worker aggression ---
    // No trades, attack sprints or hunting before this round: early game is for eating and splitting.
    constexpr int AGGRESSION_START_ROUND = 50;  // no trades or hunts before this round
    constexpr int MIN_UNITS_FOR_TRADE = 6;  // min units alive before a worker may trade
    // From this round any non-retaining dragon may trade into a long enemy (their tiebreak dragon).
    constexpr int ENDGAME_HUNT_ROUND = 470, ENDGAME_HUNT_MIN_LENGTH = 10;  // from this round long enemies may be traded into; their min length
    constexpr int TRADE_BASE = 2400, TRADE_PER_LENGTH = 800, TRADE_LENGTH_CAP = 40;  // trade value base, per enemy length, and length cap
    constexpr int ENDGAME_TRADE_BONUS = 8000;  // extra trade value in the endgame hunt
    constexpr int MAX_ATTACK_STEPS = 6; // total steps including the one onto the enemy head
    constexpr int HUNT_WEIGHT = 450, LATE_HUNT_WEIGHT = 850, HUNT_RADIUS = 15, REPORT_HUNT_RADIUS = 12;  // hunt pull weights and radii
#ifndef AGGR_MODE
#define AGGR_MODE 2
#endif
    // 3 (default): trades must not cost us swarm size. Equal trades only at length <= 3
    // (as before); longer workers trade only into clearly longer enemies; walking
    // towards enemies only for long targets and only while the swarm is healthy.
    constexpr int FAVOURABLE_MARGIN = 2;  // AGGR_MODE 3: enemy must be this much longer for a trade
    constexpr int HUNT_MIN_TARGET_LENGTH = 8, HUNT_MIN_UNITS = 40, HUNT_MARGIN = 4;  // AGGR_MODE 3: hunt target length, swarm size and margin

    // --- v11: roles ---
    // Only a grower escape produces a child this long; worker splits are capped below it,
    // so a newborn this long knows it carries a grower's body.
    constexpr int HEIR_MIN_BIRTH_LENGTH = 10;  // a newborn this long is a grower heir
    constexpr int TOP_TWO_MARGIN = 3, LENGTH_CACHE_ROUNDS = 10;  // margin to count an ally as longer; ally length cache lifetime

    // --- v11: king safety ---
    constexpr int VALUABLE_LENGTH = 16;  // dragons this long use the full threat model
    constexpr int HARD_THREAT_PENALTY = 60000, SOFT_THREAT_PENALTY = 4000;  // penalty per hard / soft enemy sprint-reach threat
    // A hidden body is assumed as long as the enemies we have seen whole (running
    // average), capped at this reach.
    constexpr int HIDDEN_BODY_REACH = 10;  // assumed max reach of an enemy with a hidden body
    constexpr int ENEMY_VISION_RADIUS = 3; // an enemy can only target a head it can see
    constexpr int ESCAPE_SEGMENT_COST = 3000, MAX_ESCAPE_STEPS = 6;  // cost per extra escape sprint step; longest escape sprint
    constexpr int BLIND_PORTAL_PENALTY = 6000;  // penalty for a portal leading out of sight

    // --- v11: pearl collection ---
    // Pull per step of path distance to the nearest pearl we can still win, out to
    // PEARL_PULL_RADIUS. Must beat the straight-ahead bias (235) and the centre pull.
    constexpr int PEARL_PULL = 450, GROWER_PEARL_PULL = 700, PEARL_PULL_RADIUS = 12;  // legacy linear pull (unused since v15 layer 2); objective map radius
    // A remembered pearl we can no longer see costs age/DIV extra steps, and is
    // forgotten after MAX_AGE rounds.
    constexpr int REMEMBERED_PEARL_AGE_DIV = 10, REMEMBERED_PEARL_MAX_AGE = 50;  // remembered pearl: age divisor for extra cost; forget after this age
    // A tile about to spawn counts once it is this close to spawning.
    constexpr int SPAWN_SOON_ROUNDS = 4;  // a tile spawning within this many rounds is an objective
    // An unseen tile whose remembered countdown ran out may hold a pearl; it is a
    // weak objective that fades, so it never outweighs a pearl we know about.
    constexpr int EXPIRED_TIMER_BASE_COST = 5, EXPIRED_TIMER_AGE_DIV = 8, EXPIRED_TIMER_MAX_AGE = 40;  // expired-timer objective: base cost, age divisor, max age
    // Standing on a tile when its countdown hits zero wastes the spawn (it only
    // appears on the mirror tile). Our body stays on a tile for `length` rounds.
    constexpr int BLOCKED_SPAWN_PENALTY = 350;  // penalty for blocking a tile that is about to spawn

    // --- v12: late game ---
    constexpr int SPLIT_STOP_ROUND = 380;                            // no worker splits after this
    constexpr int GATHER_ROUND = 380;                                // every non-retaining dragon heads for the king...
    constexpr int SACRIFICE_ROUND = 400;                             // ...and from here turns itself into the king's pearls
    constexpr int GATHER_DISTANCE = 7;                               // before SACRIFICE_ROUND, wait this far from the king
    constexpr int KING_MIN_LENGTH = 12;                              // before GATHER_ROUND, shorter dragons never count as king
    constexpr int LATE_KING_MIN_LENGTH = 3;                          // from GATHER_ROUND a king is always elected
    constexpr int RENDEZVOUS_PULL = 500, RENDEZVOUS_KING_PULL = 200; // kingless dragons meet at the map centre
    // Small-team mode (from SACRIFICE_ROUND with at most this many of us alive).
    constexpr int SMALL_TEAM_UNITS = 6, SMALL_TEAM_FEED_RADIUS = 6, SMALL_TEAM_LEAVE_PENALTY = 800;  // small-team mode: unit limit, feed radius, leave penalty
    constexpr int SMALL_TEAM_KING_PULL = 400; // a small team's king without a territory heads for the centre
    constexpr int SMALL_TEAM_KING_TTL = 40, SMALL_TEAM_FEED_PULL = 1000;  // small-team king report lifetime; feeder pull
    constexpr int KING_INFO_TTL = 15; // rounds a relayed king report stays valid
    constexpr int TERRITORY_TTL = 60; // territories move rarely; remember them longer
    // A second retainer ("vice") as insurance. Off: any dragon that knows of a clearly
    // longer ally feeds it. Set VICE_PERCENT to e.g. 60 to keep a vice at >= 60% of the king.
    constexpr int VICE_MIN_LENGTH = 20, VICE_PERCENT = 101;  // vice-king min length and percent of king length (101 = off)
    constexpr int BEACON_START_ROUND = 250, BEACON_DOUBLE_ROUND = 380;  // rounds king beacons start and double
    constexpr int FEED_AIM_AHEAD = 8; // feeders aim this many cells ahead on the king's cycle
    constexpr int FEED_AHEAD_MIN = 3, FEED_AHEAD_MAX = 40;  // feeder aim-ahead clamp on the king's cycle
    constexpr int FEED_NEAR_RADIUS = 4;       // no known cycle: die this close to a visible king
    constexpr int FEEDER_FOOD_PENALTY = 6000; // feeders leave the king's pearls alone
    // Patrol: a Hamiltonian cycle over the claimed rectangle.
    constexpr int PATROL_MIN_LENGTH = 12;  // min length to patrol
    constexpr int PATROL_SLACK = 4;         // cycle must be at least this much longer than the body
    constexpr int PATROL_REGROW_SLACK = 12; // below this, claim a bigger rectangle
    constexpr int PATROL_JUMP_PENALTY = 900, PATROL_BACKJUMP_PENALTY = 8000;  // patrol: penalty for jumping the cycle; for jumping backwards
    constexpr int PATROL_LEAVE_PENALTY = 4000, PATROL_RETURN_PULL = 500;  // patrol: penalty for leaving; pull back to the cycle
    // Escape by splitting: the tail becomes a new head carrying the body.
    constexpr int SPLIT_ESCAPE_MIN_LENGTH = 12;  // min length for a split escape out of tight space
    // Workers leave this many unit slots free, so a trapped long dragon can always
    // split-escape (a split at the unit limit is illegal).
    constexpr int ESCAPE_UNIT_RESERVE = 2;  // unit slots kept free for split escapes
    // Tail reachability through cells we can vouch for (seen free recently, or our own body).
    constexpr int KNOWN_SPACE_WEIGHT = 400, KNOWN_SPACE_PENALTY_CAP = 16000;  // known-space deficit weight and cap
    constexpr int KNOWN_FREE_MEMORY = 4, OCCUPIED_MEMORY = 2;  // rounds a seen-free cell stays trusted; rounds a seen body is assumed still there
    // Crowds close pockets faster than any static check can see: prefer exits no other
    // head can step into this round, and thin surroundings.
    constexpr int NO_ROBUST_EXIT_PENALTY = 2500, ONE_ROBUST_EXIT_PENALTY = 700;  // penalty for no / one exit no other head can take
    constexpr int CROWD_PENALTY_PER_SEGMENT = 40, CROWD_RADIUS = 2;  // penalty per nearby other segment; crowd radius

    // --- v15 layer 2: food gravity field ---
    constexpr int GRAVITY_WEIGHT = 12000;         // worker pull numerator: pearl at ETA e adds W/(e+1)^2
    constexpr int GROWER_GRAVITY_WEIGHT = 18000;  // retaining-dragon pull numerator
    constexpr int GRAVITY_RADIUS = 12;            // BFS depth from a candidate when summing pearls
    constexpr int GRAVITY_CAP = 9000;             // upper bound on one candidate's total gravity
    constexpr int GRAVITY_CONTEST_DIV = 8;        // divide pull of a pearl a rival reaches strictly first
    constexpr int GRAVITY_TIE_DIV = 2;            // divide pull of a pearl a rival reaches at the same time
    constexpr int RIVAL_ETA_RADIUS = 14;          // depth of the rival-head ETA BFS

    // --- v15 layer 3: budgeted DFS lookahead ---
    constexpr int LOOKAHEAD_MAX_DEPTH = 10;          // deepest future move the DFS must reach
    constexpr int LOOKAHEAD_MIN_DEPTH = 4;           // required depth for short dragons
    constexpr int LOOKAHEAD_BUDGET_PER_MOVE = 6000;  // DFS node expansions per candidate move
    constexpr int DEAD_END_PENALTY = 20000;          // fully searched shortfall (fatal trap), scaled by missing depth
    constexpr int JAM_PENALTY = 1500;                // budget-cut shortfall (temporary jam), scaled by missing depth

    // --- v15 layer 4: global BFS route planner ---
    constexpr int ROUTE_MIN_GRAVITY = 300;      // plan a route only when the best local gravity is below this
    constexpr int ROUTE_MAX_DIST = 80;          // BFS depth limit (cells)
    constexpr int ROUTE_PEARL_MAX_AGE = 60;     // remembered pearls older than this are not route targets
    constexpr int FRONTIER_STALE_ROUNDS = 40;   // a seen cell unseen for this long counts as frontier
    constexpr int ROUTE_PEARL_BOOST = 700;      // bonus for the first step towards a remembered pearl
    constexpr int ROUTE_FRONTIER_BOOST = 350;   // bonus for the first step towards the frontier

    // --- v15 layer 1: priority cascade ---
    constexpr bool ASSASSINATION_ENABLED = true;  // ram a visible enemy queen head when an exact sprint reaches it
    constexpr int ASSASSIN_MAX_STEPS = 6;         // longest assassination sprint, including the ramming step
    constexpr int QUEEN_FEED_ROUND = 350;         // from this round short dragons may die beside our queen
    constexpr int QUEEN_FEED_MAX_LENGTH = 6;      // only dragons this short sacrifice themselves to the queen
    constexpr int QUEEN_FEED_RADIUS = 3;          // our queen's head must be this close (Manhattan)
    constexpr int QUEEN_FEED_ENEMY_RADIUS = 3;    // no sacrifice with an enemy head this close
    constexpr int QUEEN_NO_SPLIT_ROUND = 200;     // the queen never worker-splits from this round
    // --- v15 layer 5: panic escapes ---
    constexpr bool PANIC_ENABLED = true;          // re-check the chosen move and fall back if it is fatal
    constexpr int PANIC_SPLIT_MIN_LENGTH = 4;     // shortest dragon that may split-escape in a panic
    // --- v15 rev2 ---
    constexpr int OTHER_HIDDEN_TAIL = 10;         // DFS: segments assumed behind another dragon's chain that leaves our view
}
