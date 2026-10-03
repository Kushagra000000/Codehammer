#pragma once
// bot-v15.0 tunable constants. Every constant has a one-line comment; lanes append only.
#include <cstdint>

namespace Config
{
    constexpr int SPLIT_CUTOFF = 250, FINAL_GROWTH_CUTOFF = 425;
    constexpr int TARGET_SWARM_SIZE = 64, MAX_SWARM_SIZE = 64;
    constexpr int GROWER_NUMERATOR = 1, GROWER_DENOMINATOR = 8;
    constexpr int GROWER_SELECTION = 0; // 0 stable hash, 1 ID modulo
    constexpr int MAX_PRE_GROWTH_LENGTH = 3;
    constexpr int PORTAL_EXPLORATION_PERCENT = 0; // disabled; not a score bonus
    constexpr int PORTAL_REWARD = 250, GROWER_PORTAL_REWARD = 35;
    constexpr int UNKNOWN_PORTAL_PENALTY = 90, GROWER_UNKNOWN_PORTAL_PENALTY = 1200;
    constexpr int EXPLORATION_WEIGHT = 10, GROWER_EXPLORATION_WEIGHT = 2; // NEW: increased worker exploration weight
    constexpr int EXPLORATION_CAP = 800, GROWER_EXPLORATION_CAP = 100;    // NEW: increased worker exploration cap
    constexpr int VISIT_PENALTY = 28, GROWER_VISIT_PENALTY = 8;
    constexpr int SPACE_PENALTY = 1100, GROWER_SPACE_PENALTY = 4800;
    constexpr int ENEMY_THREAT_PENALTY = 260, GROWER_ENEMY_THREAT_PENALTY = 1700;
    // Allies never step onto a visible part, so an ally head next to a tile is congestion,
    // not danger. (Was 1600, which made workers skip pearls beside teammates.)
    constexpr int ALLY_CONGESTION_PENALTY = 300;
    constexpr int HEAD_TRADE_MAX_LENGTH = 3, SPRINT_TRADE_MAX_LENGTH = 4;
    constexpr bool KING_ENABLED = true;
    constexpr int CLAIM_MIN_LENGTH = 8, FEED_MIN_GROWER_LENGTH = 10;
    constexpr int ENEMY_TARGET_MIN_LENGTH = 5;
    constexpr bool PATROL_ENABLED = true;
    constexpr int PATROL_BONUS = 3400, FOOD_YIELD_PENALTY = 1800, TERRITORY_PENALTY = 3000;
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
    constexpr int TRADE_BASE = 2400, TRADE_PER_LENGTH = 800, TRADE_LENGTH_CAP = 40;
    constexpr int ENDGAME_TRADE_BONUS = 8000;
    constexpr int MAX_ATTACK_STEPS = 6; // total steps including the one onto the enemy head
    constexpr int HUNT_WEIGHT = 450, LATE_HUNT_WEIGHT = 850, HUNT_RADIUS = 15, REPORT_HUNT_RADIUS = 12;
#ifndef AGGR_MODE
#define AGGR_MODE 2
#endif
    // 3 (default): trades must not cost us swarm size. Equal trades only at length <= 3
    // (as before); longer workers trade only into clearly longer enemies; walking
    // towards enemies only for long targets and only while the swarm is healthy.
    constexpr int FAVOURABLE_MARGIN = 2;
    constexpr int HUNT_MIN_TARGET_LENGTH = 8, HUNT_MIN_UNITS = 40, HUNT_MARGIN = 4;

    // --- v11: roles ---
    // Only a grower escape produces a child this long; worker splits are capped below it,
    // so a newborn this long knows it carries a grower's body.
    constexpr int HEIR_MIN_BIRTH_LENGTH = 10;
    constexpr int TOP_TWO_MARGIN = 3, LENGTH_CACHE_ROUNDS = 10;

    // --- v11: king safety ---
    constexpr int VALUABLE_LENGTH = 16;
    constexpr int HARD_THREAT_PENALTY = 60000, SOFT_THREAT_PENALTY = 4000;
    // A hidden body is assumed as long as the enemies we have seen whole (running
    // average), capped at this reach.
    constexpr int HIDDEN_BODY_REACH = 10;
    constexpr int ENEMY_VISION_RADIUS = 3; // an enemy can only target a head it can see
    constexpr int ESCAPE_SEGMENT_COST = 3000, MAX_ESCAPE_STEPS = 6;
    constexpr int BLIND_PORTAL_PENALTY = 6000;

    // --- v11: pearl collection ---
    // Pull per step of path distance to the nearest pearl we can still win, out to
    // PEARL_PULL_RADIUS. Must beat the straight-ahead bias (235) and the centre pull.
    constexpr int PEARL_PULL = 450, GROWER_PEARL_PULL = 700, PEARL_PULL_RADIUS = 12;
    // A remembered pearl we can no longer see costs age/DIV extra steps, and is
    // forgotten after MAX_AGE rounds.
    constexpr int REMEMBERED_PEARL_AGE_DIV = 10, REMEMBERED_PEARL_MAX_AGE = 50;
    // A tile about to spawn counts once it is this close to spawning.
    constexpr int SPAWN_SOON_ROUNDS = 4;
    // An unseen tile whose remembered countdown ran out may hold a pearl; it is a
    // weak objective that fades, so it never outweighs a pearl we know about.
    constexpr int EXPIRED_TIMER_BASE_COST = 5, EXPIRED_TIMER_AGE_DIV = 8, EXPIRED_TIMER_MAX_AGE = 40;
    // Standing on a tile when its countdown hits zero wastes the spawn (it only
    // appears on the mirror tile). Our body stays on a tile for `length` rounds.
    constexpr int BLOCKED_SPAWN_PENALTY = 350;

    // --- v12: late game ---
    constexpr int SPLIT_STOP_ROUND = 380;                            // no worker splits after this
    constexpr int GATHER_ROUND = 380;                                // every non-retaining dragon heads for the king...
    constexpr int SACRIFICE_ROUND = 400;                             // ...and from here turns itself into the king's pearls
    constexpr int GATHER_DISTANCE = 7;                               // before SACRIFICE_ROUND, wait this far from the king
    constexpr int KING_MIN_LENGTH = 12;                              // before GATHER_ROUND, shorter dragons never count as king
    constexpr int LATE_KING_MIN_LENGTH = 3;                          // from GATHER_ROUND a king is always elected
    constexpr int RENDEZVOUS_PULL = 500, RENDEZVOUS_KING_PULL = 200; // kingless dragons meet at the map centre
    // Small-team mode (from SACRIFICE_ROUND with at most this many of us alive).
    constexpr int SMALL_TEAM_UNITS = 6, SMALL_TEAM_FEED_RADIUS = 6, SMALL_TEAM_LEAVE_PENALTY = 800;
    constexpr int SMALL_TEAM_KING_PULL = 400; // a small team's king without a territory heads for the centre
    constexpr int SMALL_TEAM_KING_TTL = 40, SMALL_TEAM_FEED_PULL = 1000;
    constexpr int KING_INFO_TTL = 15; // rounds a relayed king report stays valid
    constexpr int TERRITORY_TTL = 60; // territories move rarely; remember them longer
    // A second retainer ("vice") as insurance. Off: any dragon that knows of a clearly
    // longer ally feeds it. Set VICE_PERCENT to e.g. 60 to keep a vice at >= 60% of the king.
    constexpr int VICE_MIN_LENGTH = 20, VICE_PERCENT = 101;
    constexpr int BEACON_START_ROUND = 250, BEACON_DOUBLE_ROUND = 380;
    constexpr int FEED_AIM_AHEAD = 8; // feeders aim this many cells ahead on the king's cycle
    constexpr int FEED_AHEAD_MIN = 3, FEED_AHEAD_MAX = 40;
    constexpr int FEED_NEAR_RADIUS = 4;       // no known cycle: die this close to a visible king
    constexpr int FEEDER_FOOD_PENALTY = 6000; // feeders leave the king's pearls alone
    // Patrol: a Hamiltonian cycle over the claimed rectangle.
    constexpr int PATROL_MIN_LENGTH = 12;
    constexpr int PATROL_SLACK = 4;         // cycle must be at least this much longer than the body
    constexpr int PATROL_REGROW_SLACK = 12; // below this, claim a bigger rectangle
    constexpr int PATROL_JUMP_PENALTY = 900, PATROL_BACKJUMP_PENALTY = 8000;
    constexpr int PATROL_LEAVE_PENALTY = 4000, PATROL_RETURN_PULL = 500;
    // Escape by splitting: the tail becomes a new head carrying the body.
    constexpr int SPLIT_ESCAPE_MIN_LENGTH = 12;
    // Workers leave this many unit slots free, so a trapped long dragon can always
    // split-escape (a split at the unit limit is illegal).
    constexpr int ESCAPE_UNIT_RESERVE = 2;
    // Tail reachability through cells we can vouch for (seen free recently, or our own body).
    constexpr int KNOWN_SPACE_WEIGHT = 400, KNOWN_SPACE_PENALTY_CAP = 16000;
    constexpr int KNOWN_FREE_MEMORY = 4, OCCUPIED_MEMORY = 2;
    // Crowds close pockets faster than any static check can see: prefer exits no other
    // head can step into this round, and thin surroundings.
    constexpr int NO_ROBUST_EXIT_PENALTY = 2500, ONE_ROBUST_EXIT_PENALTY = 700;
    constexpr int CROWD_PENALTY_PER_SEGMENT = 40, CROWD_RADIUS = 2;
}
