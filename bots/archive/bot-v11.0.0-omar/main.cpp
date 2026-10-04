#include "helper.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// v11: king safety (sprint-reach threat model, time-aware space, escape sprints),
// roles that follow the body, validated sonar, round-gated worker aggression.
// v13: no Hamiltonian patrols (the king roams for pearls), pocket raids with a
// split-reverse escape, value-and-risk portal scoring, shared portal maps, no
// centre bias.
namespace Config
{
    // Growers start retaining at SPLIT_CUTOFF (v13: 300, was 250): the roaming king
    // catches up quickly, so the swarm keeps splitting longer.
    constexpr int SPLIT_CUTOFF = 300, FINAL_GROWTH_CUTOFF = 425;
    constexpr int TARGET_SWARM_SIZE = 64, MAX_SWARM_SIZE = 64;
    constexpr int GROWER_NUMERATOR = 1, GROWER_DENOMINATOR = 8;
    constexpr int GROWER_SELECTION = 0; // 0 stable hash, 1 ID modulo
    constexpr int MAX_PRE_GROWTH_LENGTH = 3;
    constexpr int VISIT_PENALTY = 28, GROWER_VISIT_PENALTY = 8;
    constexpr int SPACE_PENALTY = 1100, GROWER_SPACE_PENALTY = 4800;
    constexpr int ENEMY_THREAT_PENALTY = 260, GROWER_ENEMY_THREAT_PENALTY = 1700;
    // Allies never step onto a visible part, so an ally head next to a tile is congestion,
    // not danger. (Was 1600, which made workers skip pearls beside teammates.)
    constexpr int ALLY_CONGESTION_PENALTY = 300;
    constexpr int HEAD_TRADE_MAX_LENGTH = 3, SPRINT_TRADE_MAX_LENGTH = 4;
    constexpr bool KING_ENABLED = true;
    constexpr int FEED_MIN_GROWER_LENGTH = 10;
    constexpr int ENEMY_TARGET_MIN_LENGTH = 5;
    constexpr int FOOD_YIELD_PENALTY = 1800;
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
    // 2 (default since v13): trade into anything at least as long; hunt visible
    // enemies 2+ longer and long reported ones nearby.
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
    constexpr int HARD_THREAT_PENALTY = 60000, SOFT_THREAT_PENALTY = 4000;
    // A hidden body is assumed as long as the enemies we have seen whole (running
    // average), capped at this reach.
    constexpr int HIDDEN_BODY_REACH = 10;
    constexpr int ENEMY_VISION_RADIUS = 3; // an enemy can only target a head it can see
    constexpr int ESCAPE_SEGMENT_COST = 3000, MAX_ESCAPE_STEPS = 6;

    // --- v11: pearl collection ---
    // Pull per step of path distance to the nearest objective (a pearl we can still
    // win, a tile about to spawn, an unexplored tile), out to the role's radius.
    // Must beat the straight-ahead bias (250).
    constexpr int PEARL_PULL = 450, GROWER_PEARL_PULL = 700;
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
    constexpr int SPAWN_SOON_ROUNDS = 4, KEEPER_SPAWN_SOON_ROUNDS = 10;
    // An unseen tile whose remembered countdown ran out may hold a pearl; it is a
    // weak objective that fades, so it never outweighs a pearl we know about.
    constexpr int EXPIRED_TIMER_BASE_COST = 5, EXPIRED_TIMER_AGE_DIV = 8, EXPIRED_TIMER_MAX_AGE = 40;
    constexpr int KEEPER_EXPIRED_TIMER_MAX_AGE = 200, EXPIRED_TIMER_AGE_CAP = 120;

    // --- v13: pocket raids ---
    // A worker may enter a dead end it cannot turn around in when it holds at least
    // this many pearls: it eats them, then splits (the rear L-2 reverse out, the 2 in
    // front are the sacrifice). Net gain: pearls - 2.
    constexpr int POCKET_MIN_PEARLS = 3, POCKET_PEARL_BONUS = 300;
    constexpr int POCKET_PEARL_MEMORY = 10; // remembered pearls this fresh count too
    // Any other move into such a trap (a closed region with no loop, e.g. a 1-wide
    // dead end of any depth) is unsafe, and its entrance is ignored by the objective
    // map for a while.
    constexpr int TRAP_PENALTY = 4000, GROWER_TRAP_PENALTY = 6000, TRAP_MARK_ROUNDS = 10;

    // --- v13: portals ---
    // Nothing is assumed about a portal. Its value is what we know of the far side:
    // the objective pull runs through known portals, plus how rich the area around
    // the exit is compared with where we are (pearls, timers about to fire, timers
    // that fired unwatched, unexplored tiles). Its risk depends on the role.
    constexpr int PORTAL_REGION_RADIUS = 4;   // 9x9 area around the exit / our head
    constexpr int PORTAL_REGION_WEIGHT = 120; // score per expected pearl of difference
    constexpr int PORTAL_REGION_CAP = 900;
    // Unknown exit (we have never seen where it leads).
    constexpr int UNKNOWN_PORTAL_RISK_WORKER = 150, UNKNOWN_PORTAL_RISK_GROWER = 3000, UNKNOWN_PORTAL_RISK_VALUABLE = 8000;
    constexpr int UNKNOWN_PORTAL_EXPLORE_COST = 8; // a worker with nothing closer treats it as exploring
    // Known exit we cannot see from here.
    constexpr int BLIND_PORTAL_RISK_WORKER = 100, BLIND_PORTAL_RISK_GROWER = 1500, BLIND_PORTAL_PENALTY = 6000;
    constexpr int PORTAL_EXIT_OCCUPIED_PENALTY = 4000; // a body was at the exit a moment ago
    // Taking the same portal pair again soon, with nothing to eat that way.
    constexpr int PORTAL_LOOP_PENALTY = 600, PORTAL_LOOP_ROUNDS = 12;
    // Portal pairs are shared by sonar (magic 0xB) every PERIOD rounds, until GATHER_ROUND,
    // on one of the four directions (the status packet gives way). false: never sent.
    constexpr bool PORTAL_SHARING = true;
    constexpr int PORTAL_SHARE_PERIOD = 3;
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
    constexpr int SMALL_TEAM_UNITS = 6, SMALL_TEAM_FEED_RADIUS = 6;
    constexpr int SMALL_TEAM_KING_PULL = 400; // a lonely king of a small team drifts to the centre
    constexpr int SMALL_TEAM_KING_TTL = 40, SMALL_TEAM_FEED_PULL = 1000;
    constexpr int KING_INFO_TTL = 15; // rounds a relayed king report stays valid
    // A second retainer ("vice") as insurance. Off: any dragon that knows of a clearly
    // longer ally feeds it. Set VICE_PERCENT to e.g. 60 to keep a vice at >= 60% of the king.
    constexpr int VICE_MIN_LENGTH = 20, VICE_PERCENT = 101;
    constexpr int BEACON_START_ROUND = SPLIT_CUTOFF, BEACON_DOUBLE_ROUND = 380;
    constexpr int FEED_NEAR_RADIUS = 4;       // die this close to a visible king
    constexpr int FEEDER_FOOD_PENALTY = 6000; // feeders leave the king's pearls alone
    // A trapped feeder farther than this from the king split-escapes instead of dying.
    constexpr int FEEDER_ESCAPE_KING_RADIUS = 6;
    // Escape by splitting: the tail becomes a new head carrying the body. Growers
    // this long also do it to get out of tight space (anyone does it rather than die).
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

namespace
{

    using unswbc::Direction;
    using unswbc::EdgeType;
    using unswbc::Position;

    constexpr int INF = std::numeric_limits<int>::max() / 4;

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
        bool other_seen = false; // another dragon's segment was here when last seen
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
        bool critical_ok = false; // no reach threat, an exit, and time-aware room for the body
        bool pocket_plan = false; // v13: a dead end worth raiding (eat, then split-reverse out)
        SpaceStats region;        // of the time-aware space search from the destination
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
            update_body_model();
            observe_heads();
            compute_roles();
            update_king_info();
            build_threat_maps();
            update_coordination();
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
            if (king_.id == controller_.get_id())
                controller_.output_log("IAMKING", controller_.get_length(), "units", controller_.get_unit_count());
            if (valuable_now_)
                controller_.output_log("BODYK", known_body_, controller_.get_length(), game_.get_round_num() - first_round_,
                                       birth_length_, dbg_resets_, dbg_truncs_, dbg_mem_links_);
#endif

            if (play_clear_remnant())
                return;
            if (play_feeder_strategy())
                return;

            CellMemory &current = cell(controller_.get_position());
            ++current.visits;

            build_objective_map();
            std::vector<Candidate> candidates = single_step_candidates(false);
            if (candidates.empty())
                candidates = single_step_candidates(true);
            add_tactical_sprints(candidates);
            add_escape_sprints(candidates);
            apply_known_space_penalties(candidates);
            if (play_split_escape(candidates))
                return;

            // v13: a worker splits only where both halves have a way out (never
            // halfway down a dead end: the front half could not escape it).
            if (should_split() && split_room_ok(candidates))
            {
                int const child_size = worker_child_size();
                controller_.do_split(child_size);
                send_status(controller_.get_position(), controller_.get_length() - child_size);
                return;
            }

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
                                           c.region.trap(c.new_length) ? "TRAP" : "-", c.pocket_plan ? "POCKET" : "-");
                }
            if (best->pocket_plan)
                controller_.output_log("POCKET", best->region.pearls, "len", controller_.get_length(), "space", best->space);
#endif
            if (best->steps.size() == 1)
                controller_.make_move(best->steps.front());
            else
                controller_.make_moves(best->steps);
            if (best->steps.size() > 1)
                controller_.output_log(best->head_trade ? "ATTACK_SPRINT" : "ESCAPE_SPRINT",
                                       static_cast<int>(best->steps.size()));
            note_portals_taken(best->steps);
            record_pending_path(best->steps);

            int projected_length = best->new_length;
            if (projected_length <= 0)
            {
                projected_length = controller_.get_length();
                if (best->direct_pearl)
                    ++projected_length;
                projected_length -= static_cast<int>(best->steps.size()) - 1;
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
                if (it->first != controller_.get_id() &&
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
            if (L < Config::LATE_KING_MIN_LENGTH)
                return false;
            if (!king_fresh() || king_.id == controller_.get_id())
                return true;
            return king_beats(L, controller_.get_id(), king_.length, king_.id);
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
                if (it->first != controller_.get_id() && it->second.first >= L + Config::TOP_TWO_MARGIN)
                {
                    ++stronger;
                    top = std::max(top, it->second.first);
                    king_counted = king_counted || it->first == king_.id;
                }
                ++it;
            }
            if (king_fresh() && king_.id != controller_.get_id() && king_.length >= L + Config::TOP_TWO_MARGIN)
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

            // From round 400 exactly one dragon retains: the elected king. Everyone
            // else becomes food.
            if (round >= Config::SACRIFICE_ROUND)
                return elected_king();

            if (round >= Config::FEED_START_ROUND && controller_.get_length() <= Config::FEEDER_MAX_LENGTH)
            {
                for (auto const &s : signals_)
                    if (s.grower && s.id != controller_.get_id() &&
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
            valuable_now_ = retaining_now_ || controller_.get_length() >= Config::VALUABLE_LENGTH;
        }

        // Cached once per turn, after sonar and vision are read.
        bool retaining_length() const
        {
            return retaining_now_;
        }

        int worker_child_size() const
        {
            // The parent keeps at least two; the child never reaches heir length.
            return std::min(controller_.get_length() - unswbc::Constants::MIN_SIZE,
                            Config::HEIR_MIN_BIRTH_LENGTH - 1);
        }

        int unit_cap() const
        {
            return std::min({Config::TARGET_SWARM_SIZE, Config::MAX_SWARM_SIZE, game_.get_unit_limit()});
        }

        bool should_split() const
        {
            if (game_.get_round_num() >= Config::SPLIT_STOP_ROUND || retaining_length() || controller_.get_length() <= Config::MAX_PRE_GROWTH_LENGTH)
                return false;
            if (controller_.get_unit_count() >= unit_cap() - Config::ESCAPE_UNIT_RESERVE)
                return false;
            return controller_.can_split(worker_child_size());
        }

        // v13: split only where both halves have a way out. The half we keep (our
        // head) needs a move that is not into a trap (a dead end it could only leave
        // by splitting again, with too few segments left to do it). The child starts
        // at our tail, facing away from us, and needs the same.
        bool split_room_ok(std::vector<Candidate> const &candidates) const
        {
            int const child = worker_child_size();
            int const keep = controller_.get_length() - child;
            bool const head_ok = std::any_of(candidates.begin(), candidates.end(), [&](Candidate const &c)
                                             { return c.steps.size() == 1 && !c.head_trade && !c.pocket_plan &&
                                                      c.exits > 0 && c.space >= keep + 2 && !c.region.trap(keep); });
            if (!head_ok)
                return false;
            if (!body_complete())
                return true; // cannot judge the tail; do not hold up the swarm
            int exits = 0;
            SpaceStats stats;
            child_space(child, child + 20, exits, &stats);
            if (exits == 0 || stats.trap(child))
                return false;
            // A tail with a single way out is a corridor mouth; only split there when
            // we have actually seen that the way leads somewhere (a newborn has seen
            // nothing, and would otherwise hand its child an unexplored dead end).
            return exits >= 2 || !stats.unknown;
        }

        // v13.1: split-escape slots are tiered. Normal worker splits stop at cap - 2
        // (ESCAPE_UNIT_RESERVE); an ordinary worker may escape only below that, growers
        // and any other dragon of 16+ may take the next slot, and only the king the
        // last one. At the cap nobody can split out, and a boxed-in king dies.
        bool escape_slot_available() const
        {
            bool const king = retaining_length() && king_.id == controller_.get_id();
            int const reserve = king ? 0 : valuable_now_ ? 1
                                                         : Config::ESCAPE_UNIT_RESERVE;
            return controller_.get_unit_count() < unit_cap() - reserve;
        }

        // A dead end worth raiding: only workers (never the king or growers), only
        // while the swarm builds, and only if a split will be legal at the far end.
        bool pocket_plan_allowed() const
        {
            return !retaining_length() && game_.get_round_num() < Config::GATHER_ROUND &&
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
                endpoints.push_back(endpoint);
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
        Step step_from(Position position, Direction direction) const
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
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                Position pos = tile.get_position();
                CellMemory &remembered = cell(pos);
                remembered.seen = true;
                remembered.last_seen = round;
                remembered.pearl = tile.has_pearl();
                remembered.pearl_seen = tile.has_pearl() ? round : -1;
                remembered.pearl_time = tile.get_pearl_time();
                remembered.pearl_sample_round = round;
                remembered.other_seen = tile.get_dragon() && tile.get_dragon()->get_id() != controller_.get_id();

                for (Direction direction : Direction::get_direction_list())
                {
                    unswbc::Edge const &edge = tile.get_edge(direction);
                    RememberedEdge &known = remembered.edges[static_cast<std::size_t>(direction_index(direction))];
                    known = {true, edge.get_edge_type(), edge.get_portal_id()};
                    if (edge.is_portal())
                        remember_portal(edge.get_portal_id(),
                                        canonical_edge(pos, direction));
                }
                // Symmetry deduction remains disabled (see v10).
            }
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
                    if (!part || part->get_id() != id || part->is_head())
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
            if (s0 < 2 || s0 + 1 >= known_body_)
                return 0;
            std::uint64_t m = 0xAULL << 60U;
            m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
            m |= std::uint64_t(child & 511) << 50U;
            m |= std::uint64_t((L - 1 - s0) & 511) << 41U;
            Position const p0 = own_trail_[static_cast<std::size_t>(s0)];
            m |= std::uint64_t(p0.x & 63) << 35U;
            m |= std::uint64_t(p0.y & 63) << 29U;
            for (int i = 0; i < 12 && s0 - i >= 2; ++i)
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

        // Will this turn's sonar ray in direction d stop on the child (our segments 2+)?
        bool ray_hits_child(Direction d) const
        {
            Position p = controller_.get_position();
            if (d == controller_.get_dir().get_opposite())
            {
                // That ray leaves our tail, which after the split is our neck.
                if (known_body_ < 2)
                    return false;
                p = own_trail_[1];
            }
            for (int n = 0; n < width_ + height_; ++n)
            {
                Step const s = step_from(p, d);
                if (!s.passable || !s.destination_known || !controller_.get_tile(s.destination))
                    return false;
                std::size_t const c = static_cast<std::size_t>(index(s.destination));
                int const oi = own_idx_[c];
                if (oi == 0 || oi == 1 || other_occ_[c])
                    return false;
                if (oi >= 2 || oi == -2)
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
                while (j >= 2 && chebyshev_distance(own_trail_[static_cast<std::size_t>(j)], tail) <= 3)
                    --j;
                s_top = std::min(s_top, j + 2);
            }
            int chunk = 0, sent = 0;
            for (Direction d : Direction::get_direction_list())
            {
                if (!ray_hits_child(d))
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
        // and the extra tail segment per sprint step.
        bool simulate_path(std::vector<Direction> const &steps, PostMove &pm, bool attack_final) const
        {
            pm = PostMove{};
            Position current = controller_.get_position();
            int const L = controller_.get_length();
            int eaten = 0;
            for (std::size_t j = 0; j < steps.size(); ++j)
            {
                int const s = static_cast<int>(j);
                int const length_now = s == 0 ? L : L + eaten - (s - 1);
                if (s >= 1 && length_now <= 2)
                    return false;
                Step const step = step_from(current, steps[j]);
                if (!step.passable || !step.destination_known)
                    return false;
                Position const q = step.destination;
                int const c = index(q);
                bool const last = j + 1 == steps.size();
                if (!controller_.get_tile(q) && s >= 1)
                    return false; // sprints stay inside what we can see
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
                    pm.length = L + eaten - (pm.steps - 1);
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
                if (!already && visible_pearl(q))
                    ++eaten;
                pm.path.push_back(q);
                current = q;
            }
            pm.head = current;
            pm.steps = static_cast<int>(steps.size());
            pm.eaten = eaten;
            pm.length = L + eaten - (pm.steps - 1);
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
                        if (m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY)
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
        int crowd_penalty(PostMove const &pm) const
        {
            int robust = 0;
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(pm.head, d);
                if (!s.passable || !s.destination_known)
                    continue;
                std::size_t const n = static_cast<std::size_t>(index(s.destination));
                if (other_occ_[n] || post_index(pm, static_cast<int>(n)) >= 0)
                    continue;
                if (enemy_threats_[n] == 0 && ally_threats_[n] == 0)
                    ++robust;
            }
            int penalty = robust == 0 ? Config::NO_ROBUST_EXIT_PENALTY : robust == 1 ? Config::ONE_ROBUST_EXIT_PENALTY
                                                                                     : 0;
            int crowd = 0;
            for (int dy = -Config::CROWD_RADIUS; dy <= Config::CROWD_RADIUS; ++dy)
                for (int dx = -Config::CROWD_RADIUS; dx <= Config::CROWD_RADIUS; ++dx)
                {
                    Position const q(wrap(pm.head.x + dx, width_), wrap(pm.head.y + dy, height_));
                    if (other_occ_[static_cast<std::size_t>(index(q))])
                        ++crowd;
                }
            return penalty + crowd * Config::CROWD_PENALTY_PER_SEGMENT;
        }

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
        // vouch for. Scored relative to the best option, so it still ranks moves when no
        // option reaches the full body length (long kings rarely see their whole route).
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
            for (Candidate &c : candidates)
            {
                if (c.head_trade || !c.destination_known || c.known_space < 0)
                    continue;
                int const deficit = std::max(0, best - c.known_space);
                c.score -= std::min(Config::KNOWN_SPACE_PENALTY_CAP, deficit * Config::KNOWN_SPACE_WEIGHT);
            }
        }

        // ---------------------------------------------------------------

        void observe_heads()
        {
            heads_.clear();
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
                SeenHead head{part->get_id(), part->get_team(), part->get_position(),
                              part->get_dir(), visible_lengths[part->get_id()]};
                BodyTrace const trace = trace_body(part->get_position(), part->get_id());
                int const traced = static_cast<int>(trace.segments.size());
                head.lower_length = std::max({2, head.visible_length, traced});
                head.truncated = !trace.complete || head.visible_length > traced;
                if (!head.truncated && head.team != controller_.get_team())
                    typical_enemy_x16_ += (head.lower_length * 16 - typical_enemy_x16_) / 8;
                heads_.push_back(head);
            }
        }

        SeenHead const *seen_head(int id) const
        {
            for (SeenHead const &h : heads_)
                if (h.id == id)
                    return &h;
            return nullptr;
        }

        void build_threat_maps()
        {
            std::fill(enemy_threats_.begin(), enemy_threats_.end(), 0);
            std::fill(ally_threats_.begin(), ally_threats_.end(), 0);
            for (SeenHead const &head : heads_)
            {
                for (Direction direction : Direction::get_direction_list())
                {
                    if (direction == head.facing.get_opposite())
                        continue;
                    Step const step = step_from(head.position, direction);
                    if (!step.passable || !step.destination_known)
                        continue;
                    std::vector<int> &threats =
                        head.team == controller_.get_team() ? ally_threats_ : enemy_threats_;
                    ++threats[static_cast<std::size_t>(index(step.destination))];
                }
            }
            build_reach_threats();
        }

        // Every enemy moves exactly once between our consecutive turns, and a dragon
        // of length L can sprint L-1 steps with a collision check on every step. It
        // can only aim at a head inside its own 7x7 window.
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
                int const hard_reach = std::max(0, head.lower_length - 1);
                int const typical_reach = std::min(Config::HIDDEN_BODY_REACH, (typical_enemy_x16_ + 15) / 16 - 1);
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
                            if (!part || part->get_id() != head.id)
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
            CellMemory const &m = memory_[i];
            int const now = game_.get_round_num();
            return m.last_seen != now && m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY;
        }

        // An ally that reaches the pearl first will take it; chasing it too wastes a
        // worker. Ties go to the lower id, which moves first. Growers never yield.
        bool pearl_taken_by_ally(Position p) const
        {
            if (retaining_length())
                return false;
            int const mine = distance(controller_.get_position(), p);
            int const my_id = controller_.get_id();
            for (SeenHead const &h : heads_)
            {
                if (h.team != controller_.get_team())
                    continue;
                int const d = distance(h.position, p);
                if (d < mine || (d == mine && h.id < my_id))
                    return true;
            }
            int const now = game_.get_round_num();
            for (TeamSignal const &sig : signals_)
            {
                if (sig.id == my_id || now - sig.round > 1 || seen_head(sig.id))
                    continue;
                int const d = distance(sig.position, p);
                if (d < mine || (d == mine && sig.id < my_id))
                    return true;
            }
            return false;
        }

        // One multi-source search per turn from every objective outwards (path
        // distance, through known portals). Objectives: visible pearls (cost 0),
        // remembered pearls (fading with age), tiles about to spawn, unseen tiles whose
        // countdown ran out, and never-seen tiles (exploring). The radius depends on
        // the role: the king and growers see the whole map, so they always head for the
        // nearest thing worth eating instead of circling; feeders do not explore.
        int objective_radius_ = Config::WORKER_OBJECTIVE_RADIUS;
        void build_objective_map()
        {
            std::fill(objective_dist_.begin(), objective_dist_.end(), INF);
            int const now = game_.get_round_num();
            bool const keeper = retaining_length();
            bool const feeder = !keeper && now >= Config::GATHER_ROUND && feeder_target_.has_value();
            int const max_cost = keeper   ? Config::KEEPER_OBJECTIVE_RADIUS
                                 : feeder ? Config::FEEDER_OBJECTIVE_RADIUS
                                          : Config::WORKER_OBJECTIVE_RADIUS;
            objective_radius_ = max_cost;
            int const explore_cost = keeper ? Config::KEEPER_EXPLORE_COST : feeder ? INF
                                                                                   : Config::WORKER_EXPLORE_COST;
            int const soon = keeper ? Config::KEEPER_SPAWN_SOON_ROUNDS : Config::SPAWN_SOON_ROUNDS;
            int const pearl_max_age = keeper ? Config::KEEPER_REMEMBERED_PEARL_MAX_AGE : Config::REMEMBERED_PEARL_MAX_AGE;
            int const expired_max_age = keeper ? Config::KEEPER_EXPIRED_TIMER_MAX_AGE : Config::EXPIRED_TIMER_MAX_AGE;
            bool const late = now >= SPECIALISE_ROUND; // no pearl is reserved before this
            std::vector<std::vector<int>> buckets(static_cast<std::size_t>(max_cost + 1));
            auto add = [&](int c, int cost)
            {
                if (cost > max_cost || cost >= objective_dist_[static_cast<std::size_t>(c)])
                    return;
                objective_dist_[static_cast<std::size_t>(c)] = cost;
                buckets[static_cast<std::size_t>(cost)].push_back(c);
            };
            int const cells = width_ * height_;
            for (int c = 0; c < cells; ++c)
            {
                CellMemory const &m = memory_[static_cast<std::size_t>(c)];
                if (blocks_pearl_path(c) || own_idx_[static_cast<std::size_t>(c)] == 0 ||
                    trap_mark_[static_cast<std::size_t>(c)] > now)
                    continue;
                if (!m.seen)
                {
                    add(c, explore_cost);
                    continue;
                }
                Position const p = position_of(c);
                bool const visible = m.last_seen == now;
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
                if (remaining > 0)
                {
                    // Arrive the move after it spawns, not before (we would block it).
                    if (remaining <= soon && !(late && reserved_food(p)) && !pearl_taken_by_ally(p))
                        add(c, remaining + 1);
                }
                else if (!visible && -remaining <= expired_max_age && !(late && reserved_food(p)))
                {
                    add(c, Config::EXPIRED_TIMER_BASE_COST +
                               std::min(-remaining, Config::EXPIRED_TIMER_AGE_CAP) / Config::EXPIRED_TIMER_AGE_DIV);
                }
            }
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
                    score -= 900;
                else if (gap == 2)
                    score -= 260;
                else if (gap >= 4)
                    score += 20;
                if (head.visible_length > controller_.get_length() && gap <= 3)
                    score -= 180;
            }
            for (TeamSignal const &signal : signals_)
            {
                if (signal.id == controller_.get_id() || seen_head(signal.id))
                    continue; // a visible head was already counted above

                // NEW: Do not repel from the grower we are trying to feed!
                if (feeder_target_ && signal.id == feeder_target_->id)
                    continue;

                int const gap = chebyshev_distance(destination, signal.position);
                if (gap <= 1)
                    score -= 700;
                else if (gap == 2)
                    score -= 180;
                if (signal.length > controller_.get_length() && gap <= 3)
                    score -= 240;
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
            if (h.team == controller_.get_team() || round < Config::AGGRESSION_START_ROUND ||
                retaining_length())
                return false;
            int const est = h.estimated_length();
            // Endgame: their long dragon decides the tiebreak; anything that is not
            // one of our top two may trade into it.
            if (round >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                return true;
            if (valuable_now_ || controller_.get_unit_count() < Config::MIN_UNITS_FOR_TRADE)
                return false;
            int const L = controller_.get_length();
#if AGGR_MODE == 1
            return L <= Config::HEAD_TRADE_MAX_LENGTH;
#elif AGGR_MODE == 2
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
#if AGGR_MODE == 1
            return false;
#elif AGGR_MODE == 2
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
#if AGGR_MODE == 1
            return steps <= 2 && controller_.get_length() <= 4;
#elif AGGR_MODE == 2
            (void)steps;
            return true;
#else
            return est >= controller_.get_length() + Config::FAVOURABLE_MARGIN ||
                   (steps <= 2 && controller_.get_length() <= 4);
#endif
        }

        int trade_value(SeenHead const &h) const
        {
            int const est = h.estimated_length();
            int value = Config::TRADE_BASE + Config::TRADE_PER_LENGTH * std::min(est, Config::TRADE_LENGTH_CAP);
            if (game_.get_round_num() >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                value += Config::ENDGAME_TRADE_BONUS;
            return value;
        }

        // ---------------------------------------------------------------

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
                exit_occupied = m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY;
            }

            unswbc::DragonPart const *occupant =
                step.destination_known ? visible_part(step.destination) : nullptr;
            if (!occupant && step.destination_known && visible_occupied(step.destination))
                return candidate;
            int trade_bonus = 0;
            if (occupant)
            {
                bool const enemy_head = occupant->is_head() &&
                                        occupant->get_team() != controller_.get_team();
                SeenHead const *target = enemy_head ? seen_head(occupant->get_id()) : nullptr;
                if (!target || !eligible_target(*target))
                    return candidate;
                candidate.head_trade = true;
                trade_bonus = trade_value(*target);
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
                int const destination_index = index(step.destination);
                candidate.enemy_threats =
                    enemy_threats_[static_cast<std::size_t>(destination_index)];
                candidate.ally_threats =
                    ally_threats_[static_cast<std::size_t>(destination_index)];
                candidate.hard_threats = hard_threat_[static_cast<std::size_t>(destination_index)];
                candidate.soft_threats = soft_threat_[static_cast<std::size_t>(destination_index)];
                if (!candidate.head_trade)
                {
                    candidate.exits = post_exits(pm);
                    candidate.space = timed_space(pm, space_cap(pm.length), false, &candidate.region);
                }
            }

            int score = deterministic_jitter(direction);
            Direction const facing = controller_.get_dir();
            if (direction == facing)
                score += 250;
            else if (direction == facing.get_opposite())
                score -= 850;
            else
                score += 15;

            bool const grow = retaining_length();
            int gain = 0; // steps closer to the nearest objective
            {
                // Pull towards the nearest objective, measured as path distance (through
                // known portals and across the map edges, which wrap).
                int const radius = objective_radius_;
                int const before = std::min(radius, objective_dist_[static_cast<std::size_t>(index(origin))]);
                int after = before;
                if (step.destination_known)
                    after = std::min(radius, objective_dist_[static_cast<std::size_t>(index(step.destination))]);
                else if (candidate.unknown_portal && !grow && !valuable_now_ &&
                         before > Config::UNKNOWN_PORTAL_EXPLORE_COST)
                    after = before - 1; // nothing closer: a portal we never took is worth exploring
                gain = before - after;
                score += gain * (grow ? Config::GROWER_PEARL_PULL : Config::PEARL_PULL);
            }
            if (candidate.direct_pearl)
                score += grow ? 3800 : 2500; // NEW: Was 2800 and 850.
            if (candidate.portal)
                score += portal_score(candidate, direction, blind_portal, exit_occupied, gain);

            if (step.destination_known)
            {
                CellMemory const &destination = cell(step.destination);
                score -= destination.visits * (grow ? Config::GROWER_VISIT_PENALTY : Config::VISIT_PENALTY);

                if (!candidate.head_trade)
                {
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
                    score += candidate.exits * 55;
                    score += std::min(need + 5, candidate.space) * (grow ? 13 : 8);
                    if (candidate.pocket_plan)
                        score += (candidate.region.pearls - 2) * Config::POCKET_PEARL_BONUS;
                    else
                    {
                        if (candidate.exits == 0)
                            score -= 5000;
                        else if (candidate.exits == 1)
                            score -= grow ? 1300 : 280;
                        // Time-aware: segments that will have moved on count as room.
                        int const space_needed = need + 3;
                        if (candidate.space < space_needed)
                            score -= (space_needed - candidate.space) * (grow ? 800 : 400);
                        // (A region merely too small for us is priced by the room deficit.)
                        if (candidate.region.dead_end())
                        {
                            score -= grow ? Config::GROWER_TRAP_PENALTY : Config::TRAP_PENALTY;
                            // Its pearls are not worth chasing either: stop the pull from
                            // walking us back to the entrance every turn.
                            trap_mark_[static_cast<std::size_t>(index(step.destination))] = now + Config::TRAP_MARK_ROUNDS;
                        }
                    }
                }
                // Never let spacing talk a dragon out of an adjacent pearl.
                if (!candidate.direct_pearl)
                    score += teammate_spacing_score(step.destination);
                if (!candidate.direct_pearl)
                    if (unswbc::Tile const *tile = controller_.get_tile(step.destination))
                    {
                        int const countdown = tile->get_pearl_time();
                        if (countdown >= 1 && countdown <= candidate.new_length - 1)
                            score -= Config::BLOCKED_SPAWN_PENALTY;
                    }

                auto const [enemy_before, enemy_after] =
                    nearest_visible_enemy_delta(origin, step.destination);
                if (enemy_before < INF)
                    score += (enemy_after - enemy_before) * (grow ? 85 : 30);
            }

            if (step.destination_known)
                score += coordination_score(step.destination);
            if (candidate.head_trade)
                score += trade_bonus;
            if (candidate.enemy_threats > 0 && !candidate.head_trade)
                score -= candidate.enemy_threats * (grow ? Config::GROWER_ENEMY_THREAT_PENALTY : Config::ENEMY_THREAT_PENALTY);
            if (candidate.ally_threats > 0)
                score -= candidate.ally_threats * Config::ALLY_CONGESTION_PENALTY;
            if (valuable_now_ && !candidate.head_trade)
            {
                score -= candidate.hard_threats * Config::HARD_THREAT_PENALTY +
                         candidate.soft_threats * Config::SOFT_THREAT_PENALTY;
                if (step.destination_known)
                {
                    candidate.known_space = timed_space(pm, pm.length + 3, true);
                    score -= crowd_penalty(pm);
                }
            }

            candidate.critical_ok = candidate.destination_known && !candidate.unknown_portal &&
                                    !candidate.head_trade && candidate.exits > 0 &&
                                    candidate.space >= pm.length + unknown_body() + 2 && !trap && !exit_occupied &&
                                    (!valuable_now_ || (candidate.hard_threats == 0 && !blind_portal));
            // A pocket raid is deliberate: it passes the safety filter like a safe move.
            candidate.safe = (candidate.critical_ok || candidate.pocket_plan) && candidate.enemy_threats == 0;
            candidate.score = score;
            return candidate;
        }

        // Expected pearls (x100) in the 9x9 area around a tile, from memory: pearls we
        // know of (fading with age), tiles about to spawn, tiles whose countdown ran out
        // while nobody watched, and a little for tiles never seen.
        int region_value(Position centre) const
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
                        value += 3;
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
                        value += 50 + remaining / 2;
                }
            return value;
        }

        // v13 portals. Nothing is assumed: the objective pull (computed through known
        // portals) already says whether the nearest thing worth eating lies beyond it.
        // Added here: how rich the far side is compared with here, the risk of a portal
        // we cannot see through (much higher for growers and the king), and loops.
        int portal_score(Candidate const &c, Direction direction, bool blind, bool exit_occupied, int gain) const
        {
            bool const grow = retaining_length();
            int score = 0;
            if (c.unknown_portal)
                score -= valuable_now_ ? Config::UNKNOWN_PORTAL_RISK_VALUABLE
                         : grow        ? Config::UNKNOWN_PORTAL_RISK_GROWER
                                       : Config::UNKNOWN_PORTAL_RISK_WORKER;
            else
            {
                if (blind)
                    score -= valuable_now_ ? Config::BLIND_PORTAL_PENALTY
                             : grow        ? Config::BLIND_PORTAL_RISK_GROWER
                                           : Config::BLIND_PORTAL_RISK_WORKER;
                if (exit_occupied)
                    score -= Config::PORTAL_EXIT_OCCUPIED_PENALTY;
                int const diff = region_value(c.destination) - region_value(controller_.get_position());
                score += std::clamp(diff * Config::PORTAL_REGION_WEIGHT / 100,
                                    -Config::PORTAL_REGION_CAP, Config::PORTAL_REGION_CAP);
            }
            // Back through the pair we just used, with nothing to eat that way: a loop.
            Position const origin = controller_.get_position();
            int const id = cell(origin).edges[static_cast<std::size_t>(direction_index(direction))].portal_id;
            if (id >= 0 && id == last_portal_id_ &&
                game_.get_round_num() - last_portal_round_ <= Config::PORTAL_LOOP_ROUNDS && gain <= 0)
                score -= Config::PORTAL_LOOP_PENALTY;
            return score;
        }

        std::vector<Candidate> single_step_candidates(bool include_reverse) const
        {
            Direction const facing = controller_.get_dir();
            std::array<Direction, 4> const ordered{
                facing, facing.get_left(), facing.get_right(), facing.get_opposite()};
            std::vector<Candidate> candidates;
            for (Direction direction : ordered)
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
            int const max_steps = std::min(L - 1, Config::MAX_ATTACK_STEPS);
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
                        attack.score = trade_value(*h) - 60 * static_cast<int>(steps.size() - 1) +
                                       deterministic_jitter(steps.front());
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
            int const max_steps = std::min(L - 2, Config::MAX_ESCAPE_STEPS);
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
                        int score = deterministic_jitter(steps.front());
                        score -= (pm.steps - 1) * Config::ESCAPE_SEGMENT_COST;
                        score += pm.eaten * 3800;
                        score -= c.hard_threats * Config::HARD_THREAT_PENALTY +
                                 c.soft_threats * Config::SOFT_THREAT_PENALTY;
                        score -= c.enemy_threats * Config::GROWER_ENEMY_THREAT_PENALTY;
                        score -= c.ally_threats * Config::ALLY_CONGESTION_PENALTY;
                        score += c.exits * 55 + std::min(need + 5, c.space) * 13;
                        if (c.exits == 0)
                            score -= 5000;
                        else if (c.exits == 1)
                            score -= 1300;
                        if (c.space < need + 3)
                            score -= (need + 3 - c.space) * 800;
                        if (c.region.dead_end())
                            score -= Config::GROWER_TRAP_PENALTY;
                        c.known_space = timed_space(pm, pm.length + 3, true);
                        score -= crowd_penalty(pm);
                        score += teammate_spacing_score(pm.head);
                        c.critical_ok = c.hard_threats == 0 && c.exits > 0 && c.space >= need + 2 && !trap;
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
                    if (signal.id != controller_.get_id())
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
                        known = {true, EdgeType::PORTAL, id};
                }
            }
        }
    };

} // namespace

#ifdef BOT_DEBUG
#include <chrono>
#endif

int main()
{
    auto [controller, game] = unswbc::init();
    ReplayDerivedBot bot(controller, game);
    while (unswbc::update(controller, game))
    {
#ifdef BOT_DEBUG
        auto const t0 = std::chrono::steady_clock::now();
#endif
        bot.play_turn();
#ifdef BOT_DEBUG
        auto const us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
        controller.output_log("TIME", static_cast<long long>(us), controller.get_length());
#endif
        unswbc::end_turn();
    }
    return 0;
}