#pragma once

// alan-v1 policy constants. Phase boundaries and swarm size are calculated at
// runtime from map dimensions and team state; only bounded tuning values live
// here. Keeping this file independent of the controller makes active knobs easy
// to audit and prevents stale experimental switches accumulating in main.cpp.
namespace Config
{
    // Adaptive phase bounds.
    constexpr int MIN_SWARM_SIZE = 18;
    constexpr int MAX_SWARM_SIZE = 64;
    constexpr int CELLS_PER_SWARM_UNIT = 32;
    constexpr int SPECIALISE_MIN_ROUND = 200;
    constexpr int SPECIALISE_MAX_ROUND = 280;
    constexpr int GATHER_MIN_LEAD = 95;
    constexpr int GATHER_MAX_LEAD = 130;
    constexpr int SACRIFICE_MIN_LEAD = 75;
    constexpr int SACRIFICE_MAX_LEAD = 105;
    constexpr int ENDGAME_HUNT_LEAD = 30;
    constexpr int ESCAPE_UNIT_RESERVE = 2;

    // Opening distribution. The center pull is temporary and automatically
    // yields to nearby food, crowding, or evidence that the center is barren.
    constexpr int OPENING_CENTER_MIN_ROUNDS = 24;
    constexpr int OPENING_CENTER_MAX_ROUNDS = 44;
    constexpr int OPENING_CENTER_WEIGHT = 180;
    constexpr int OPENING_CENTER_RADIUS = 3;
    constexpr int OPENING_FOOD_PRIORITY_RADIUS = 4;
    constexpr int OPENING_CENTER_CROWD_LIMIT = 3;

    // Roles and growth.
    constexpr int GROWER_NUMERATOR = 1;
    constexpr int GROWER_DENOMINATOR = 8;
    constexpr int GROWER_SELECTION = 0; // stable hash
    constexpr int MAX_PRE_GROWTH_LENGTH = 3;
    constexpr int HEIR_MIN_BIRTH_LENGTH = 10;
    constexpr int TOP_TWO_MARGIN = 3;
    constexpr int LENGTH_CACHE_ROUNDS = 10;
    constexpr int VALUABLE_LENGTH = 16;
    constexpr int KING_MIN_LENGTH = 12;
    constexpr int LATE_KING_MIN_LENGTH = 3;
    constexpr int BACKUP_MIN_LENGTH = 16;
    constexpr int BACKUP_PERCENT = 70;

    // Navigation and resources.
    constexpr int PORTAL_REWARD = 250;
    constexpr int GROWER_PORTAL_REWARD = 35;
    constexpr int UNKNOWN_PORTAL_PENALTY = 250;
    constexpr int GROWER_UNKNOWN_PORTAL_PENALTY = 3000;
    constexpr int BLIND_PORTAL_PENALTY = 9000;
    constexpr int EXPLORATION_WEIGHT = 10;
    constexpr int GROWER_EXPLORATION_WEIGHT = 2;
    constexpr int EXPLORATION_CAP = 800;
    constexpr int GROWER_EXPLORATION_CAP = 100;
    constexpr int VISIT_PENALTY = 28;
    constexpr int GROWER_VISIT_PENALTY = 8;
    constexpr int PEARL_PULL = 450;
    constexpr int GROWER_PEARL_PULL = 700;
    constexpr int PEARL_PULL_RADIUS = 12;
    constexpr int REMEMBERED_PEARL_AGE_DIV = 10;
    constexpr int REMEMBERED_PEARL_MAX_AGE = 50;
    constexpr int SPAWN_SOON_ROUNDS = 4;
    constexpr int EXPIRED_TIMER_BASE_COST = 5;
    constexpr int EXPIRED_TIMER_AGE_DIV = 8;
    constexpr int EXPIRED_TIMER_MAX_AGE = 40;
    constexpr int BLOCKED_SPAWN_PENALTY = 350;

    // Threat and movement safety.
    constexpr int ENEMY_THREAT_PENALTY = 260;
    constexpr int GROWER_ENEMY_THREAT_PENALTY = 1700;
    constexpr int ALLY_CONGESTION_PENALTY = 300;
    constexpr int HARD_THREAT_PENALTY = 60000;
    constexpr int SOFT_THREAT_PENALTY = 4000;
    constexpr int HIDDEN_BODY_REACH = 10;
    constexpr int ENEMY_VISION_RADIUS = 3;
    constexpr int ESCAPE_SEGMENT_COST = 3000;
    constexpr int MAX_ESCAPE_STEPS = 6;
    constexpr int KNOWN_SPACE_WEIGHT = 400;
    constexpr int KNOWN_SPACE_PENALTY_CAP = 16000;
    constexpr int KNOWN_FREE_MEMORY = 4;
    constexpr int OCCUPIED_MEMORY = 2;
    constexpr int NO_ROBUST_EXIT_PENALTY = 2500;
    constexpr int ONE_ROBUST_EXIT_PENALTY = 700;
    constexpr int CROWD_PENALTY_PER_SEGMENT = 40;
    constexpr int CROWD_RADIUS = 2;
    constexpr int MAX_REACH_ENEMIES = 8;

    // Controlled aggression. Trades require confirmed lower-bound value rather
    // than optimistic hidden-body estimates.
    constexpr int AGGRESSION_START_ROUND = 50;
    constexpr int MIN_UNITS_FOR_TRADE = 6;
    constexpr int HEAD_TRADE_MAX_LENGTH = 3;
    constexpr int ENDGAME_HUNT_MIN_LENGTH = 10;
    constexpr int TRADE_BASE = 2400;
    constexpr int TRADE_PER_LENGTH = 800;
    constexpr int TRADE_LENGTH_CAP = 40;
    constexpr int ENDGAME_TRADE_BONUS = 8000;
    constexpr int MAX_ATTACK_STEPS = 6;
    constexpr int HUNT_WEIGHT = 450;
    constexpr int LATE_HUNT_WEIGHT = 850;
    constexpr int HUNT_RADIUS = 15;
    constexpr int REPORT_HUNT_RADIUS = 12;
    constexpr int FAVOURABLE_MARGIN = 2;
    constexpr int HUNT_MIN_TARGET_LENGTH = 8;
    constexpr int HUNT_MIN_UNITS = 40;
    constexpr int HUNT_MARGIN = 4;

    // Territory and patrol.
    constexpr bool PATROL_ENABLED = true;
    constexpr int CLAIM_MIN_LENGTH = 8;
    constexpr int PATROL_BONUS = 3400;
    constexpr int PATROL_MIN_LENGTH = 12;
    constexpr int PATROL_SLACK = 4;
    constexpr int PATROL_REGROW_SLACK = 12;
    constexpr int PATROL_JUMP_PENALTY = 900;
    constexpr int PATROL_BACKJUMP_PENALTY = 8000;
    constexpr int PATROL_LEAVE_PENALTY = 4000;
    constexpr int PATROL_RETURN_PULL = 500;
    constexpr int FOOD_YIELD_PENALTY = 1800;
    constexpr int TERRITORY_PENALTY = 3000;
    constexpr int TERRITORY_SEARCH_BUDGET = 320;

    // Gathering and feeding.
    constexpr int FEED_MIN_GROWER_LENGTH = 10;
    constexpr int FEEDER_MAX_LENGTH = 4;
    constexpr int GATHER_DISTANCE = 7;
    constexpr int RENDEZVOUS_PULL = 500;
    constexpr int RENDEZVOUS_KING_PULL = 200;
    constexpr int FEED_AIM_AHEAD = 8;
    constexpr int FEED_AHEAD_MIN = 3;
    constexpr int FEED_AHEAD_MAX = 40;
    constexpr int FEED_NEAR_RADIUS = 4;
    constexpr int FEEDER_FOOD_PENALTY = 6000;
    constexpr int FEED_COLLECTIBLE_PERCENT = 60;
    constexpr int SACRIFICE_METHOD = 1;
    constexpr int SMALL_TEAM_UNITS = 6;
    constexpr int SMALL_TEAM_FEED_RADIUS = 6;
    constexpr int SMALL_TEAM_LEAVE_PENALTY = 800;
    constexpr int SMALL_TEAM_KING_PULL = 400;
    constexpr int SMALL_TEAM_KING_TTL = 40;
    constexpr int SMALL_TEAM_FEED_PULL = 1000;

    // Communication freshness.
    constexpr int SONAR_FREQUENCY = 1;
    constexpr int KING_INFO_TTL = 15;
    constexpr int TERRITORY_TTL = 60;
    constexpr int ENEMY_TARGET_MIN_LENGTH = 5;

    // Emergency body transfer.
    constexpr int SPLIT_ESCAPE_MIN_LENGTH = 12;
}

