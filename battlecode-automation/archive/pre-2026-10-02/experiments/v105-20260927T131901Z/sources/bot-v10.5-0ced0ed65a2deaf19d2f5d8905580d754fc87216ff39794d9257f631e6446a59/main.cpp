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

// v10.5: preserve v10.4.1; tune opening control and late grower consolidation.
namespace Config
{
    // Extra center pull is opening-only. Nearby food, crowding and observed barren
    // terrain disable it; the inherited center score remains unchanged afterward.
    constexpr int OPENING_CENTER_END_ROUND = 40;
    constexpr int OPENING_CENTER_WEIGHT = 300;
    constexpr int OPENING_CENTER_RADIUS = 3;
    constexpr int OPENING_FOOD_PRIORITY_RADIUS = 4;
    constexpr int OPENING_CENTER_CROWD_LIMIT = 3;
    constexpr int SPLIT_CUTOFF = 300, FINAL_GROWTH_CUTOFF = 425;
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
    constexpr int PATROL_FOOD_RADIUS = 4;
    constexpr int KING_SELECTION = 0, KING_ID = -1, ROOT_POLICY = 0, BACKUP_KING_POLICY = 0;
    constexpr int FEED_START_ROUND = 380, FEEDER_MAX_LENGTH = 4;
    constexpr int WORKER_SPLIT_STOP_ROUND = 350; // ordinary splits; emergency grower escapes remain
    constexpr int ALPHA_FEED_START_ROUND = 475; // even growers may donate to a strictly larger recipient
    constexpr int FEED_REQUEST_MAX_AGE = 1;
    constexpr int FEED_APPROACH_WEIGHT = 2400;
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
        int spawn_gap = 0; // smoothed interval from consecutive observed resets
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
        bool feed_request = false;
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
              stamp_(memory_.size(), 0), best_(memory_.size(), INF), done_(memory_.size(), 0),
              parent_(memory_.size(), -1), parent_dir_(memory_.size(), 0),
              objective_dist_(memory_.size(), INF) {}

        int axis_enemy_echo_ = 0;
        void read_echoes()
        {
            auto const echoes = controller_.get_sonar_echoes();
            // Total count of enemies or enemy heads hit by sonars last turn
            axis_enemy_echo_ = echoes.enemy + echoes.enemy_head;
        }

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
                initialized_ = true;
            }
            read_echoes();
            receive_status();
            remember_vision();
            update_body_model();
            observe_heads();
            compute_roles();
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
                                   feeder_target_ ? feeder_target_->id : -1);
#endif

            if (play_feeder_strategy())
                return;

            CellMemory &current = cell(controller_.get_position());
            ++current.visits;

            if (should_split())
            {
                int const child_size = worker_child_size();
                controller_.do_split(child_size);
                send_status(controller_.get_position(), controller_.get_length() - child_size);
                return;
            }

            build_objective_map();
            std::vector<Candidate> candidates = single_step_candidates(false);
            if (candidates.empty())
                candidates = single_step_candidates(true);
            add_tactical_sprints(candidates);
            add_escape_sprints(candidates);
            apply_known_space_penalties(candidates);
            if (play_grower_escape(candidates))
                return;

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
            if (valuable_now_)
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
                                           "ok", c.critical_ok, "safe", c.safe);
                }
#endif
            if (best->steps.size() == 1)
            {
                if (patrol_food_direction_ && best->steps.front() == *patrol_food_direction_)
                    controller_.output_log("PATROL_FOOD", controller_.get_length(), best->destination.x, best->destination.y);
                controller_.make_move(best->steps.front());
            }
            else
                controller_.make_moves(best->steps);
            if (best->steps.size() > 1)
                controller_.output_log(best->head_trade ? "ATTACK_SPRINT" : "ESCAPE_SPRINT",
                                       static_cast<int>(best->steps.size()));
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

        // Running average (x16) of enemy lengths seen whole; sizes hidden bodies.
        int typical_enemy_x16_ = 4 * 16;

        // v11: roles
        int first_round_ = -1;
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

        bool compute_retaining() const
        {
            int const round = game_.get_round_num();

            // From round 400 only the top two retain; everyone else becomes food.
            if (round >= 400)
                return is_top_two_grower();

            if (round >= Config::FEED_START_ROUND && controller_.get_length() <= Config::FEEDER_MAX_LENGTH)
            {
                for (auto const &s : signals_)
                    if (s.grower && s.id != controller_.get_id() &&
                        s.length >= Config::FEED_MIN_GROWER_LENGTH && round - s.round <= 2 &&
                        distance(s.position, controller_.get_position()) <= 8)
                        return false;
            }
            return round >= SPECIALISE_ROUND && selected_grower();
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

        bool should_split() const
        {
            if (game_.get_round_num() >= Config::WORKER_SPLIT_STOP_ROUND || retaining_length() || controller_.get_length() <= Config::MAX_PRE_GROWTH_LENGTH)
                return false;
            if (controller_.get_unit_count() >= std::min(Config::TARGET_SWARM_SIZE, Config::MAX_SWARM_SIZE))
                return false;
            return controller_.can_split(worker_child_size());
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
        // per-step vision lookup is needed.
        Step step_from(Position position, Direction direction) const
        {
            RememberedEdge const &edge = cell(position).edges[static_cast<std::size_t>(direction_index(direction))];
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
                if (remembered.pearl_sample_round == round - 1 && remembered.pearl_time >= 0 &&
                    remembered.pearl_time <= 1 && tile.get_pearl_time() > 0)
                    remembered.spawn_gap = remembered.spawn_gap == 0 ? tile.get_pearl_time() :
                        (3 * remembered.spawn_gap + tile.get_pearl_time() + 2) / 4;
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
                own_trail_.assign(trace.segments.begin(), trace.segments.end());
            else
                for (std::size_t i = own_trail_.size(); i < trace.segments.size(); ++i)
                    own_trail_.push_back(trace.segments[i]);
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
        int timed_space(PostMove const &pm, int cap, bool known_only = false) const
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
            int const start = index(pm.head);
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
                Position const p = position_of(c);
                for (Direction d : Direction::get_direction_list())
                {
                    Step const s = step_from(p, d);
                    if (!s.passable || !s.destination_known)
                        continue;
                    int const n = index(s.destination);
                    if (done_[static_cast<std::size_t>(n)] == gen || other_occ_[static_cast<std::size_t>(n)])
                        continue;
                    int arrive = t + 1;
                    bool wait_here = waited;
                    int const oi = post_index(pm, n);
                    if (oi == 0)
                        continue;
                    CellMemory const &m = memory_[static_cast<std::size_t>(n)];
                    if (oi < 0 && m.last_seen != now)
                    {
                        // Out of sight: a body seen there just now is probably still there.
                        if (m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY)
                            continue;
                        if (known_only && !(m.seen && !m.other_seen && now - m.last_seen <= Config::KNOWN_FREE_MEMORY))
                            continue;
                    }
                    if (oi > 0)
                    {
                        int const free_at = pm.length - oi + 1 + eaten;
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
            return settled;
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

        int space_cap(int length) const
        {
            return std::max(64, length + 20);
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

        // One multi-source search per turn from every pearl objective outwards.
        // Objectives: visible pearls (cost 0), remembered pearls (fading with age),
        // tiles about to spawn, and, weakly, unseen tiles whose countdown ran out.
        void build_objective_map()
        {
            std::fill(objective_dist_.begin(), objective_dist_.end(), INF);
            int const now = game_.get_round_num();
            int const max_cost = Config::PEARL_PULL_RADIUS + 1;
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
                if (!m.seen || blocks_pearl_path(c) || own_idx_[static_cast<std::size_t>(c)] == 0)
                    continue;
                Position const p = position_of(c);
                bool const visible = m.last_seen == now;
                if (m.pearl)
                {
                    int const age = now - m.pearl_seen;
                    if (age > Config::REMEMBERED_PEARL_MAX_AGE || (late && reserved_food(p)) || pearl_taken_by_ally(p))
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
                    if (remaining <= Config::SPAWN_SOON_ROUNDS && !(late && reserved_food(p)) && !pearl_taken_by_ally(p))
                        add(c, remaining + 1);
                }
                else if (!visible && -remaining <= Config::EXPIRED_TIMER_MAX_AGE && !(late && reserved_food(p)))
                {
                    add(c, Config::EXPIRED_TIMER_BASE_COST + (-remaining) / Config::EXPIRED_TIMER_AGE_DIV);
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
                        if (blocks_pearl_path(n))
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

        int opening_center_penalty(Position destination) const
        {
            if (game_.get_round_num() >= Config::OPENING_CENTER_END_ROUND ||
                retaining_length() ||
                objective_dist_[static_cast<std::size_t>(index(controller_.get_position()))] <=
                    Config::OPENING_FOOD_PRIORITY_RADIUS)
                return 0;

            Position const center(width_ / 2, height_ / 2);
            int allies = chebyshev_distance(controller_.get_position(), center) <= 3 ? 1 : 0;
            for (SeenHead const &head : heads_)
                if (head.team == controller_.get_team() && head.id != controller_.get_id() &&
                    chebyshev_distance(head.position, center) <= 3)
                    ++allies;
            if (allies >= Config::OPENING_CENTER_CROWD_LIMIT)
                return 0;

            // Use only remembered observations, never privileged map contents.
            int sampled = 0, fertile = 0;
            for (int dy = -3; dy <= 3; ++dy)
                for (int dx = -3; dx <= 3; ++dx)
                {
                    CellMemory const &m = cell(Position(wrap(center.x + dx, width_),
                                                       wrap(center.y + dy, height_)));
                    if (!m.seen)
                        continue;
                    ++sampled;
                    if (m.pearl || m.pearl_time >= 0)
                        ++fertile;
                }
            if (sampled >= 24 && fertile * 4 < sampled)
                return 0;

            int const gap = std::max(0, chebyshev_distance(destination, center) -
                                        Config::OPENING_CENTER_RADIUS);
            return gap * std::max(0, Config::OPENING_CENTER_WEIGHT - 45);
        }

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
                    candidate.space = timed_space(pm, space_cap(pm.length));
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
            if (candidate.portal)
                score += grow ? Config::GROWER_PORTAL_REWARD : Config::PORTAL_REWARD;

            // NEW: Better portal evaluation.
            if (candidate.unknown_portal)
            {
                int penalty = grow ? Config::GROWER_UNKNOWN_PORTAL_PENALTY : Config::UNKNOWN_PORTAL_PENALTY;
                if (axis_enemy_echo_ == 0)
                    penalty /= 3;
                else
                    penalty *= 2;
                score -= penalty;
            }
            if (axis_enemy_echo_ > 0 && !enemy_target_)
            {
                if (direction == controller_.get_dir())
                    score += 200;
            }

            {
                // Pull towards the nearest pearl objective, measured as path distance.
                int const radius = Config::PEARL_PULL_RADIUS;
                int const before = std::min(radius, objective_dist_[static_cast<std::size_t>(index(origin))]);
                int const after = step.destination_known
                                      ? std::min(radius, objective_dist_[static_cast<std::size_t>(index(step.destination))])
                                      : before;
                score += (before - after) * (grow ? Config::GROWER_PEARL_PULL : Config::PEARL_PULL);
            }
            if (candidate.direct_pearl)
                score += grow ? 3800 : 2500; // NEW: Was 2800 and 850.

            if (step.destination_known)
            {
                CellMemory const &destination = cell(step.destination);
                int const age = destination.seen
                                    ? std::max(0, game_.get_round_num() - destination.last_seen)
                                    : 120;
                score += grow ? std::min(Config::GROWER_EXPLORATION_CAP, age * Config::GROWER_EXPLORATION_WEIGHT) : std::min(Config::EXPLORATION_CAP, age * Config::EXPLORATION_WEIGHT);
                score -= destination.visits * (grow ? Config::GROWER_VISIT_PENALTY : Config::VISIT_PENALTY);

                // NEW: Directed Center-Bias Exploration
                int center_x = width_ / 2;
                int center_y = height_ / 2;
                int dist_to_center = chebyshev_distance(step.destination, Position(center_x, center_y));
                score -= dist_to_center * (grow ? 5 : 45);
                score -= opening_center_penalty(step.destination);

                if (!candidate.head_trade)
                {
                    score += candidate.exits * 55;
                    score += std::min(pm.length + 5, candidate.space) * (grow ? 13 : 8);
                    if (candidate.exits == 0)
                        score -= 5000;
                    else if (candidate.exits == 1)
                        score -= grow ? 1300 : 280;
                    // Time-aware: segments that will have moved on count as room.
                    int const space_needed = pm.length + 3;
                    if (candidate.space < space_needed)
                        score -= (space_needed - candidate.space) * (grow ? 800 : 400);
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
                score += coordination_score(step.destination, direction);
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
                if (blind_portal)
                    score -= Config::BLIND_PORTAL_PENALTY;
                if (step.destination_known)
                {
                    candidate.known_space = timed_space(pm, pm.length + 3, true);
                    score -= crowd_penalty(pm);
                }
            }

            candidate.critical_ok = candidate.destination_known && !candidate.unknown_portal &&
                                    !candidate.head_trade && candidate.exits > 0 &&
                                    candidate.space >= pm.length + 2 &&
                                    (!valuable_now_ || (candidate.hard_threats == 0 && !blind_portal));
            candidate.safe = candidate.critical_ok && candidate.enemy_threats == 0;
            candidate.score = score;
            return candidate;
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
            if (!retaining_length() && !hard_everywhere)
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
                        c.space = timed_space(pm, space_cap(pm.length));
                        int score = deterministic_jitter(steps.front());
                        score -= (pm.steps - 1) * Config::ESCAPE_SEGMENT_COST;
                        score += pm.eaten * 3800;
                        score -= c.hard_threats * Config::HARD_THREAT_PENALTY +
                                 c.soft_threats * Config::SOFT_THREAT_PENALTY;
                        score -= c.enemy_threats * Config::GROWER_ENEMY_THREAT_PENALTY;
                        score -= c.ally_threats * Config::ALLY_CONGESTION_PENALTY;
                        score += c.exits * 55 + std::min(pm.length + 5, c.space) * 13;
                        if (c.exits == 0)
                            score -= 5000;
                        else if (c.exits == 1)
                            score -= 1300;
                        if (c.space < pm.length + 3)
                            score -= (pm.length + 3 - c.space) * 800;
                        c.known_space = timed_space(pm, pm.length + 3, true);
                        score -= crowd_penalty(pm);
                        score += teammate_spacing_score(pm.head);
                        if (territory_.w && territory_.contains(controller_.get_position()) && !territory_.contains(pm.head))
                            score -= 900;
                        c.critical_ok = c.hard_threats == 0 && c.exits > 0 && c.space >= pm.length + 2;
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
            std::erase_if(claims_, [&](auto const &item)
                          { return now - item.second.round > 12; });
            int const own_team = controller_.get_team().value == unswbc::Team::B ? 1 : 0;

            for (std::uint64_t message : controller_.get_sonar_messages())
            {
                std::uint64_t const magic = message >> 60U;
                int const encoded_team = static_cast<int>((message >> 59U) & 1ULL);
                if (encoded_team != own_team)
                    continue;

                if (magic == 0xCULL)
                {
                    // CLAIM: checksum in bits 2-6, symmetry in bits 0-1.
                    std::uint64_t const sum_mask = 31ULL << 2U;
                    if (((message & sum_mask) >> 2U) != packet_checksum(message & ~sum_mask, 5))
                        continue;
                    Claim c;
                    c.id = static_cast<int>((message >> 7U) & 4095ULL);
                    c.length = static_cast<int>((message >> 19U) & 1023ULL);
                    c.x = static_cast<int>((message >> 29U) & 63ULL);
                    c.y = static_cast<int>((message >> 35U) & 63ULL);
                    c.w = static_cast<int>((message >> 41U) & 63ULL);
                    c.h = static_cast<int>((message >> 47U) & 63ULL);
                    int const age = (now - static_cast<int>((message >> 53U) & 63ULL) + 64) % 64;
                    c.round = now - age;
                    if (age > 1 || c.id == controller_.get_id() || c.length < 2 ||
                        c.w < 3 || c.h < 4 || c.x + c.w > width_ || c.y + c.h > height_)
                        continue;
                    claims_[c.id] = c;
                    remember_ally_length(c.id, c.length, c.round);
                    apply_symmetry_bits(message & 3ULL);
                    share_terrain(c);
                }
                else if (magic == 0xDULL || magic == 0xEULL)
                {
                    // STATUS: checksum in bits 0-4. A sonar is read at most one round
                    // after it is sent, so anything older is not ours.
                    if ((message & 31ULL) != packet_checksum(message & ~31ULL, 5))
                        continue;
                    TeamSignal signal;
                    signal.feed_request = magic == 0xEULL;
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
            // 63-60 magic 0xD status / 0xE feeding request; remaining layout identical.
            // 59 team | 58 grower | 57-52 x | 51-46 y | 45-37 length (max 511)
            // 36-34 round&7 | 33-22 id | 21 enemy flag | 20-15 enemy x | 14-9 enemy y
            // 8-7 enemy length class | 6-5 symmetry | 4-0 checksum
            bool const request_feed = game_.get_round_num() >= Config::FEED_START_ROUND &&
                                      retaining_length() && !feeder_target_ &&
                                      predicted_length >= Config::FEED_MIN_GROWER_LENGTH;
            std::uint64_t m = (request_feed ? 0xEULL : 0xDULL) << 60U;
            m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
            m |= std::uint64_t(game_.get_round_num() < 250 ? selected_grower() : retaining_length()) << 58U;
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

            std::uint64_t claim_msg = (territory_.w && retaining_length()) ? claim_message() : 0;

            for (Direction direction : Direction::get_direction_list())
            {
                // Send Claims occasionally so growers can establish loops, otherwise blast the unified packet
                if (claim_msg && (game_.get_round_num() % 4 == direction_index(direction)))
                    controller_.send_sonar(direction, claim_msg);
                else
                    controller_.send_sonar(direction, m);
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
