#include "helper.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// v10: coordinated growers, mapped territory patrols and safe local feeding.
namespace Config
{
    constexpr int CENTER_WEIGHT = 45;
    constexpr int CENTER_UNTIL = 25, SECTOR_WEIGHT = 75;
    constexpr int LOOKAHEAD_DEPTH = 4, MAX_DODGE_STEPS = 5;
    constexpr bool BODY_ROUTES = true, CYCLE_SHORTCUTS = false;
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
    constexpr int ALLY_CONGESTION_PENALTY = 1600;
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
    };

    struct TeamSignal
    {
        int id = -1;
        Position position{};
        int length = 0;
        int round = 0;
        bool grower = false;
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
        int enemy_threats = 0;
        int ally_threats = 0;
        int exits = 0;
        int space = 0;
        int score = -INF;
    };

    class ReplayDerivedBot
    {
    public:
        ReplayDerivedBot(unswbc::Controller &controller, unswbc::Game &game_state)
            : controller_(controller), game_(game_state), width_(game_state.width),
              height_(game_state.height),
              memory_(static_cast<std::size_t>(width_ * height_)),
              enemy_threats_(memory_.size(), 0), ally_threats_(memory_.size(), 0) {}

        int axis_enemy_echo_ = 0;
        void read_echoes()
        {
            auto const echoes = controller_.get_sonar_echoes();
            // Total count of enemies or enemy heads hit by sonars last turn
            axis_enemy_echo_ = echoes.enemy + echoes.enemy_head;
        }

        void play_turn()
        {
            // NEW: read_echoes added to each turn
            read_echoes();
            receive_status();
            remember_vision();
            observe_heads();
            build_threat_maps();
            update_coordination();
            objective_cache_.clear();

            if (play_feeder_strategy())
                return;

            CellMemory &current = cell(controller_.get_position());
            ++current.visits;

            if (should_split())
            {
                int const child_size = controller_.get_length() - unswbc::Constants::MIN_SIZE;
                controller_.do_split(child_size);
                send_status(controller_.get_position(), unswbc::Constants::MIN_SIZE);
                return;
            }

            std::vector<Candidate> candidates = single_step_candidates(false);
            if (candidates.empty())
                candidates = single_step_candidates(true);
            improve_navigation(candidates);
            add_tactical_sprints(candidates);
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
                if (conservative && has_safe && !move.safe)
                    continue;
                if (!best || move.score > best->score)
                    best = &move;
            }
            if (!best)
                best = &*std::max_element(
                    candidates.begin(), candidates.end(),
                    [](Candidate const &a, Candidate const &b)
                    { return a.score < b.score; });

            remember_action(best->steps);
            if (best->steps.size() == 1)
                controller_.make_move(best->steps.front());
            else
                controller_.make_moves(best->steps);

            int projected_length = controller_.get_length();
            if (best->direct_pearl)
                ++projected_length;
            projected_length -= static_cast<int>(best->steps.size()) - 1;
            send_status(best->destination_known ? best->destination
                                                : controller_.get_position(),
                        std::max(2, projected_length));
        }

    private:
        // Growers retain length from round 250; small workers later feed or attack.
        static constexpr int SPECIALISE_ROUND = Config::SPLIT_CUTOFF;
        static constexpr int FINAL_GROWTH_ROUND = Config::FINAL_GROWTH_CUTOFF;
        static constexpr std::uint64_t STATUS_MAGIC = 0xC8ULL;

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
#include "navigation.hpp"

        int wrap(int value, int size) const
        {
            value %= size;
            return value < 0 ? value + size : value;
        }

        int index(Position position) const
        {
            return position.y * width_ + position.x;
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
            // from consecutive split-child IDs.
            // NEW: REMOVED: controller_.get_length() >= 8
            // Workers now remain workers, forcing them to split and maintain swarm size.
            return founder_ || controller_.get_length() >= 12 || controller_.get_unit_count() <= 2 ||
                   (Config::GROWER_SELECTION == 0 ? mix(static_cast<std::uint64_t>(controller_.get_id()) ^ 0xD1B54A32D192ED03ULL) : static_cast<std::uint64_t>(controller_.get_id())) % Config::GROWER_DENOMINATOR < Config::GROWER_NUMERATOR;
        }

        // --- NEW: 15-Round Memory Cache to fix Isolation Split-Brain ---
        mutable std::map<int, std::pair<int, int>> ally_length_cache_;

        bool is_top_two_grower() const
        {
            int const now = game_.get_round_num();
            std::vector<std::pair<int, int>> lengths;
            lengths.push_back({controller_.get_length(), controller_.get_id()});

            // Clean out dead dragons (15 rounds tolerates sonar blocks but drops dead dragons)
            for (auto it = ally_length_cache_.begin(); it != ally_length_cache_.end();)
            {
                if (now - it->second.second > 15)
                {
                    it = ally_length_cache_.erase(it);
                }
                else
                {
                    if (it->first != controller_.get_id())
                    {
                        lengths.push_back({it->second.first, it->first});
                    }
                    ++it;
                }
            }

            // Sort descending by length, breaking ties by lowest ID
            std::sort(lengths.begin(), lengths.end(), [](auto const &a, auto const &b)
                      {
                if (a.first != b.first) return a.first > b.first;
                return a.second < b.second; });

            int rank = 0;
            for (auto const &g : lengths)
            {
                if (g.second == controller_.get_id())
                    return rank < 2;
                rank++;
            }
            return false;
        }

        // NEW: Demotes everyone except the Top 2 at Round 450
        bool retaining_length() const
        {
            int const round = game_.get_round_num();

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

        bool should_split() const
        {
            if (game_.get_round_num() >= Config::FEED_START_ROUND || retaining_length() || controller_.get_length() <= Config::MAX_PRE_GROWTH_LENGTH)
                return false;
            if (controller_.get_unit_count() >= std::min(Config::TARGET_SWARM_SIZE, Config::MAX_SWARM_SIZE))
                return false;
            int const child_size = controller_.get_length() - unswbc::Constants::MIN_SIZE;
            return controller_.can_split(child_size);
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

        Step step_from(Position position, Direction direction) const
        {
            RememberedEdge edge;
            if (unswbc::Tile const *tile = controller_.get_tile(position))
            {
                unswbc::Edge const &visible = tile->get_edge(direction);
                edge = {true, visible.get_edge_type(), visible.get_portal_id()};
            }
            else
            {
                edge = cell(position).edges[static_cast<std::size_t>(direction_index(direction))];
            }

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

                for (Direction direction : Direction::get_direction_list())
                {
                    unswbc::Edge const &edge = tile.get_edge(direction);
                    RememberedEdge &known = remembered.edges[static_cast<std::size_t>(direction_index(direction))];
                    known = {true, edge.get_edge_type(), edge.get_portal_id()};
                    if (edge.is_portal())
                        remember_portal(edge.get_portal_id(),
                                        canonical_edge(pos, direction));
                }

                // Symmetry deduction
                if (confirmed_symmetry_ == SymmetryType::UNKNOWN)
                {
                    int expected_spawn = round + tile.get_pearl_time();
                    auto check = [&](SymmetryType type, bool &is_valid)
                    {
                        if (!is_valid)
                            return;
                        Position m = mirror(pos, type);
                        CellMemory const &m_cell = cell(m);

                        // 1. Verify Pearl Countdowns (Ensure cycles overlap to prevent false flags)
                        if (tile.get_pearl_time() >= 0 && m_cell.pearl_time >= 0 && m_cell.pearl_sample_round >= 0)
                        {
                            int m_expected = m_cell.pearl_sample_round + m_cell.pearl_time;
                            if (m_expected >= round && expected_spawn >= m_cell.pearl_sample_round)
                            {
                                if (m_expected != expected_spawn)
                                    is_valid = false;
                            }
                        }

                        // 2. Verify Static Kelp Geometry
                        for (Direction d : Direction::get_direction_list())
                        {
                            Direction md = mirror_dir(d, type);
                            RememberedEdge const &e1 = remembered.edges[direction_index(d)];
                            RememberedEdge const &e2 = m_cell.edges[direction_index(md)];
                            if (e1.known && e2.known)
                            {
                                if ((e1.type == EdgeType::KELP) != (e2.type == EdgeType::KELP))
                                    is_valid = false;
                            }
                        }
                    };

                    check(SymmetryType::HORIZONTAL, sym_h_);
                    check(SymmetryType::VERTICAL, sym_v_);
                    check(SymmetryType::ROTATIONAL, sym_r_);

                    int valid_count = sym_h_ + sym_v_ + sym_r_;
                    if (valid_count == 1)
                    {
                        if (sym_h_)
                            confirmed_symmetry_ = SymmetryType::HORIZONTAL;
                        else if (sym_v_)
                            confirmed_symmetry_ = SymmetryType::VERTICAL;
                        else if (sym_r_)
                            confirmed_symmetry_ = SymmetryType::ROTATIONAL;
                        controller_.output_log("SYMMETRY_DEDUCED");
                    }
                }

                // --- POPULATE MIRROR MAP ---
                if (confirmed_symmetry_ != SymmetryType::UNKNOWN)
                {
                    Position m = mirror(pos, confirmed_symmetry_);
                    CellMemory &m_cell = cell(m);

                    // Mirror pearl spawn countdowns across the map
                    m_cell.pearl_time = tile.get_pearl_time();
                    m_cell.pearl_sample_round = round;

                    // Mirror static Kelp barriers to aid long-distance pathfinding
                    for (Direction d : Direction::get_direction_list())
                    {
                        RememberedEdge &e1 = remembered.edges[direction_index(d)];
                        Direction md = mirror_dir(d, confirmed_symmetry_);
                        RememberedEdge &e2 = m_cell.edges[direction_index(md)];
                        if (e1.known && e1.type == EdgeType::KELP)
                        {
                            e2.known = true;
                            e2.type = EdgeType::KELP;
                        }
                    }
                }
            }
        }

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
                heads_.push_back({part->get_id(), part->get_team(), part->get_position(),
                                  part->get_dir(), visible_lengths[part->get_id()]});
            }
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
        }

        unswbc::DragonPart const *visible_part(Position position) const
        {
            unswbc::Tile const *tile = controller_.get_tile(position);
            return tile ? tile->get_dragon() : nullptr;
        }

        bool visible_occupied(Position position) const
        {
            return visible_part(position) != nullptr ||
                   (!body_occupancy_.empty() ? body_occupancy_[index(position)] :
                   std::find(own_trail_.begin(), own_trail_.end(), position) != own_trail_.end());
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

        int flood_space(Position start) const
        {
            if (!start.is_in_map())
                return 0;
            std::vector<bool> visited(memory_.size(), false);
            std::deque<Position> queue;
            visited[static_cast<std::size_t>(index(start))] = true;
            queue.push_back(start);
            int count = 0;

            // --- NEW: Dynamic Flood Limit ---
            int const max_space = std::max(64, controller_.get_length() + 15);

            while (!queue.empty() && count < max_space)
            {
                Position const current = queue.front();
                queue.pop_front();
                ++count;
                for (Direction direction : Direction::get_direction_list())
                {
                    Step const step = step_from(current, direction);
                    if (!step.passable || !step.destination_known)
                        continue;
                    int const destination_index = index(step.destination);
                    if (visited[static_cast<std::size_t>(destination_index)])
                        continue;
                    if (visible_occupied(step.destination) &&
                        !(step.destination == controller_.get_position()))
                        continue;
                    visited[static_cast<std::size_t>(destination_index)] = true;
                    queue.push_back(step.destination);
                }
            }
            return count;
        }

        mutable std::unordered_map<int,int> objective_cache_;
        int nearest_objective_distance(Position position) const
        {
            auto cached=objective_cache_.find(index(position));
            if(cached!=objective_cache_.end())return cached->second;
            std::vector<int> dist(memory_.size(), INF);
            std::deque<Position> q;
            q.push_back(position);
            dist[index(position)] = 0;
            int best = INF;
            while (!q.empty())
            {
                Position p = q.front();
                q.pop_front();
                int d = dist[index(p)];
                if (d >= best)
                    continue;
                auto const &m = cell(p);
                int round = game_.get_round_num();
                if (m.pearl && (game_.get_round_num() < 250 || !reserved_food(p)))
                    best = std::min(best, d + std::max(0, round - m.pearl_seen) / 18);
                else if ((game_.get_round_num() < 250 || !reserved_food(p)) && m.pearl_time >= 0 && m.pearl_sample_round >= 0)
                {
                    int remaining = m.pearl_time - (round - m.pearl_sample_round);
                    if (remaining <= 4)
                        best = std::min(best, d + std::max(0, remaining) + 2);
                }
                for (Direction dir : Direction::get_direction_list())
                {
                    auto step = step_from(p, dir);
                    if (!step.passable || !step.destination_known)
                        continue;
                    if (Config::BODY_ROUTES && visible_occupied(step.destination)) continue;
                    int i = index(step.destination);
                    if (dist[i] <= d + 1)
                        continue;
                    dist[i] = d + 1;
                    q.push_back(step.destination);
                }
            }
            objective_cache_[index(position)]=best;
            return best;
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
                if (signal.id == controller_.get_id())
                    continue;

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

            unswbc::DragonPart const *occupant =
                step.destination_known ? visible_part(step.destination) : nullptr;
            if (!occupant && step.destination_known && visible_occupied(step.destination))
                return candidate;
            if (occupant)
            {
                bool const enemy_head = occupant->is_head() &&
                                        occupant->get_team() != controller_.get_team();
                // NEW: Trading requires the swarm to have many units
                bool const may_trade = enemy_head && !retaining_length() &&
                                       controller_.get_length() <= Config::HEAD_TRADE_MAX_LENGTH &&
                                       controller_.get_unit_count() >= 6 &&
                                       game_.get_round_num() >= 50;
                if (!may_trade)
                    return candidate;
                candidate.head_trade = true;
            }

            candidate.direct_pearl = step.destination_known && visible_pearl(step.destination);
            if (step.destination_known)
            {
                int const destination_index = index(step.destination);
                candidate.enemy_threats =
                    enemy_threats_[static_cast<std::size_t>(destination_index)];
                candidate.ally_threats =
                    ally_threats_[static_cast<std::size_t>(destination_index)];
                candidate.exits = open_exits(step.destination);
                candidate.space = flood_space(step.destination);
            }

            int score = deterministic_jitter(direction);
            Direction const facing = controller_.get_dir();
            if (direction == facing)
                score += 80;
            else if (direction == facing.get_opposite())
                score -= 850;
            else
                score += 15;

            bool const grow = retaining_length();
            if (candidate.portal)
                score += grow ? Config::GROWER_PORTAL_REWARD : Config::PORTAL_REWARD;

            if (candidate.unknown_portal)
                score -= grow ? Config::GROWER_UNKNOWN_PORTAL_PENALTY : Config::UNKNOWN_PORTAL_PENALTY;
            // Aggregate echoes cannot certify a specific portal or direction.

            int const old_objective = nearest_objective_distance(origin);
            int const new_objective = step.destination_known
                                          ? nearest_objective_distance(step.destination)
                                          : old_objective;
            if (old_objective < INF && new_objective < INF)
            {
                int const weight = grow ? 310 : 105;
                score += (old_objective - new_objective) * weight;
            }
            if (candidate.direct_pearl)
                score += grow ? 3800 : 2500; // NEW: Was 2800 and 850.

            if (step.destination_known)
            {
                CellMemory const &destination = cell(step.destination);
                // NEW: Treat unseen tiles as highly aged (120 instead of 80) to force aggressive fog-sweeping
                int const age = destination.seen
                                    ? std::max(0, game_.get_round_num() - destination.last_seen)
                                    : 120;
                score += grow ? std::min(Config::GROWER_EXPLORATION_CAP, age * Config::GROWER_EXPLORATION_WEIGHT) : std::min(Config::EXPLORATION_CAP, age * Config::EXPLORATION_WEIGHT);
                score -= destination.visits * (grow ? Config::GROWER_VISIT_PENALTY : Config::VISIT_PENALTY);

                score += distribution_score(origin, step.destination, old_objective);

                score += candidate.exits * 55;
                // NEW: Scale the space bonus up to our full size instead of capping at 36
                score += std::min(controller_.get_length() + 5, candidate.space) * (grow ? 13 : 8);
                if (candidate.exits == 0)
                    score -= 5000;
                else if (candidate.exits == 1)
                    score -= grow ? 1300 : 280;
                // --- NEW: Uncapped Space Requirements ---
                int const space_needed = controller_.get_length() + 3;
                if (candidate.space < space_needed)
                {
                    // Massive progressive penalty based on exactly how many segments will be trapped
                    score -= (space_needed - candidate.space) * (grow ? 800 : 400);
                }
                score += teammate_spacing_score(step.destination);

                auto const [enemy_before, enemy_after] =
                    nearest_visible_enemy_delta(origin, step.destination);
                if (enemy_before < INF)
                {
                    // The replay swarm spreads around enemies but takes a direct
                    // head trade whenever one becomes available.
                    score += (enemy_after - enemy_before) * (grow ? 85 : 30);
                }
            }

            if (step.destination_known)
                score += coordination_score(step.destination, direction);
            if (candidate.head_trade)
                score += enemy_target_ ? 9000 : 2400;
            if (candidate.enemy_threats > 0 && !candidate.head_trade)
                score -= candidate.enemy_threats * (grow ? Config::GROWER_ENEMY_THREAT_PENALTY : Config::ENEMY_THREAT_PENALTY);
            if (candidate.ally_threats > 0)
                score -= candidate.ally_threats * Config::ALLY_CONGESTION_PENALTY;

            candidate.safe = candidate.destination_known && !candidate.unknown_portal &&
                             !candidate.head_trade && candidate.enemy_threats == 0 &&
                             candidate.ally_threats == 0 && candidate.exits > 0 &&
                             candidate.space >= controller_.get_length() + 2; // NEW: REMOVED the std::min(10, ...) cap
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

        void add_tactical_sprints(std::vector<Candidate> &candidates) const
        {
            if (retaining_length() || controller_.get_length() < 3 || controller_.get_length() > Config::SPRINT_TRADE_MAX_LENGTH)
                return;
            Position const origin = controller_.get_position();
            Direction const facing = controller_.get_dir();

            for (Direction first : Direction::get_direction_list())
            {
                if (first == facing.get_opposite())
                    continue;
                Step const one = step_from(origin, first);
                if (!one.passable || !one.destination_known || visible_occupied(one.destination))
                    continue;

                for (Direction second : Direction::get_direction_list())
                {
                    if (second == first.get_opposite())
                        continue;
                    Step const two = step_from(one.destination, second);
                    if (!two.passable || !two.destination_known)
                        continue;
                    unswbc::DragonPart const *target = visible_part(two.destination);
                    if (!target || !target->is_head() ||
                        target->get_team() == controller_.get_team())
                        continue;

                    Candidate attack;
                    attack.steps = {first, second};
                    attack.destination = two.destination;
                    attack.portal = one.portal || two.portal;
                    attack.head_trade = true;
                    attack.exits = open_exits(two.destination);
                    attack.space = flood_space(two.destination);
                    attack.score = (enemy_target_ ? 9500 : 2250) + deterministic_jitter(first) - 120;
                    attack.safe = false;
                    candidates.push_back(std::move(attack));
                }
            }
        }

        // NEW: receive_status revamped.
        void receive_status()
        {
            int const now = game_.get_round_num();
            std::erase_if(signals_, [&](auto const &s)
                          { return now - s.round > 2; });
            std::erase_if(claims_, [&](auto const &item)
                          { return now - item.second.round > 12; });

            for (std::uint64_t message : controller_.get_sonar_messages())
            {
                std::uint64_t const magic = message >> 60U;
                int const encoded_team = static_cast<int>((message >> 59U) & 1ULL);
                int const own_team = controller_.get_team().value == unswbc::Team::B ? 1 : 0;
                if (encoded_team != own_team)
                    continue;

                if (magic == 0x7ULL) { receive_terrain(message); continue; }
                if (magic == 0x5ULL)
                {
                    // Unpack 60-bit CLAIM message
                    Claim c;
                    c.id = (message >> 7U) & 4095ULL;
                    c.length = (message >> 19U) & 1023ULL;
                    c.x = (message >> 29U) & 63ULL;
                    c.y = (message >> 35U) & 63ULL;
                    c.w = (message >> 41U) & 63ULL;
                    c.h = (message >> 47U) & 63ULL;
                    int age = (now - int((message >> 53U) & 63ULL) + 64) % 64;
                    c.round = now - age;
                    if (age <= 12 && c.id != controller_.get_id() && c.w >= 3 && c.h >= 4 && c.x + c.w <= width_ && c.y + c.h <= height_)
                    {
                        claims_[c.id] = c;
                        // NEW: Update rolling memory cache
                        ally_length_cache_[c.id] = {c.length, now};
                        // NEW: Extract the symmetry broadcast from the claim message (bits 0-1)
                        std::uint64_t sym_bits = message & 3ULL;
                        if (confirmed_symmetry_ == SymmetryType::UNKNOWN && sym_bits != 0)
                        {
                            if (sym_bits == 1)
                                confirmed_symmetry_ = SymmetryType::HORIZONTAL;
                            else if (sym_bits == 2)
                                confirmed_symmetry_ = SymmetryType::VERTICAL;
                            else if (sym_bits == 3)
                                confirmed_symmetry_ = SymmetryType::ROTATIONAL;
                        }
                        share_terrain(c);
                    }
                }
                else if (magic == 0x6ULL)
                {
                    // Unpack 60-bit UNIFIED (Status + Enemy) message
                    TeamSignal signal;
                    signal.grower = ((message >> 58U) & 1ULL) != 0;
                    signal.position.x = static_cast<int>((message >> 52U) & 63ULL);
                    signal.position.y = static_cast<int>((message >> 46U) & 63ULL);
                    signal.length = static_cast<int>((message >> 36U) & 1023ULL);
                    signal.round = static_cast<int>((message >> 27U) & 511ULL);
                    signal.id = static_cast<int>((message >> 15U) & 4095ULL);

                    // Extract the symmetry broadcast from bits 0-1
                    std::uint64_t sym_bits = message & 3ULL;
                    if (confirmed_symmetry_ == SymmetryType::UNKNOWN && sym_bits != 0)
                    {
                        if (sym_bits == 1)
                            confirmed_symmetry_ = SymmetryType::HORIZONTAL;
                        else if (sym_bits == 2)
                            confirmed_symmetry_ = SymmetryType::VERTICAL;
                        else if (sym_bits == 3)
                            confirmed_symmetry_ = SymmetryType::ROTATIONAL;
                    }

                    int age = (now - signal.round + 512) % 512;
                    if (age <= 2 && signal.id != controller_.get_id() && signal.position.is_in_map())
                    {
                        signal.round = now - age;
                        // NEW: Update rolling memory cache
                        ally_length_cache_[signal.id] = {signal.length, now};
                        std::erase_if(signals_, [&](auto const &old)
                                      { return old.id == signal.id; });
                        signals_.push_back(signal);
                    }

                    // Unpack piggybacked Enemy Target if the flag (Bit 14) is set
                    if (age <= 2 && ((message >> 14U) & 1ULL))
                    {
                        Position enemy_pos(static_cast<int>((message >> 8U) & 63ULL), static_cast<int>((message >> 2U) & 63ULL));
                        if (!enemy_report_ || now > enemy_report_->round)
                        {
                            enemy_report_ = TeamSignal{-1, enemy_pos, 5, now - age, false};
                        }
                    }
                }
            }
        }

        // NEW: send_status revamped.
        void send_status(Position predicted_position, int predicted_length)
        {
            if (Config::SONAR_STRATEGY == 0 || game_.get_round_num() % Config::SONAR_FREQUENCY != 0)
                return;

            // Build the unified packet.
            std::uint64_t m = 0x6ULL << 60U;
            m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
            m |= std::uint64_t(game_.get_round_num() < 250 ? selected_grower() : retaining_length()) << 58U;
            m |= std::uint64_t(predicted_position.x & 63ULL) << 52U;
            m |= std::uint64_t(predicted_position.y & 63ULL) << 46U;
            m |= std::uint64_t(std::min(1023, predicted_length)) << 36U;
            m |= std::uint64_t(game_.get_round_num() & 511ULL) << 27U;
            m |= std::uint64_t(controller_.get_id() & 4095ULL) << 15U;

            // Put enemy reports into the same packet.
            if (enemy_report_ && std::any_of(heads_.begin(), heads_.end(), [&](auto const& h) { return h.team != controller_.get_team() && h.position == enemy_report_->position; }))
            {
                m |= 1ULL << 14U; // Set Has-Enemy Flag
                m |= std::uint64_t(enemy_report_->position.x & 63ULL) << 8U;
                m |= std::uint64_t(enemy_report_->position.y & 63ULL) << 2U;
            }

            // Piggyback confirmed symmetry onto bits 0-1
            std::uint64_t sym_bits = 0;
            if (confirmed_symmetry_ == SymmetryType::HORIZONTAL)
                sym_bits = 1;
            else if (confirmed_symmetry_ == SymmetryType::VERTICAL)
                sym_bits = 2;
            else if (confirmed_symmetry_ == SymmetryType::ROTATIONAL)
                sym_bits = 3;
            m |= sym_bits;

            std::uint64_t claim_msg = (territory_.w && retaining_length()) ? claim_message() : 0;

            for (Direction direction : Direction::get_direction_list())
            {
                // Send Claims occasionally so growers can establish loops, otherwise blast the unified packet
                if ((game_.get_round_num()+1) % 4 == direction_index(direction))
                    controller_.send_sonar(direction, terrain_message());
                else if (claim_msg && (game_.get_round_num() % 4 == direction_index(direction)))
                {
                    controller_.send_sonar(direction, claim_msg);
                }
                else
                {
                    controller_.send_sonar(direction, m);
                }
            }
        }
    };

} // namespace

int main()
{
    auto [controller, game] = unswbc::init();
    ReplayDerivedBot bot(controller, game);
    while (unswbc::update(controller, game))
    {
        bot.play_turn();
        unswbc::end_turn();
    }
    return 0;
}
