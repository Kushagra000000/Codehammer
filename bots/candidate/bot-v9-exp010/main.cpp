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

// EXP-001: exact v8 behaviour. Disabled subsystem controls are reserved and
// must not be treated as implemented features until their experiment lands.
namespace Config {
constexpr int SPLIT_CUTOFF = 300, FINAL_GROWTH_CUTOFF = 425;
constexpr int TARGET_SWARM_SIZE = 64, MAX_SWARM_SIZE = 64;
constexpr int GROWER_NUMERATOR = 1, GROWER_DENOMINATOR = 4;
constexpr int GROWER_SELECTION = 0; // 0 stable hash, 1 ID modulo
constexpr int MAX_PRE_GROWTH_LENGTH = 3;
constexpr int PORTAL_EXPLORATION_PERCENT = 0; // disabled; not a score bonus
constexpr int PORTAL_REWARD = 250, GROWER_PORTAL_REWARD = 35;
constexpr int UNKNOWN_PORTAL_PENALTY = 90, GROWER_UNKNOWN_PORTAL_PENALTY = 1200;
constexpr int EXPLORATION_WEIGHT = 6, GROWER_EXPLORATION_WEIGHT = 2;
constexpr int EXPLORATION_CAP = 360, GROWER_EXPLORATION_CAP = 100;
constexpr int VISIT_PENALTY = 28, GROWER_VISIT_PENALTY = 8;
constexpr int SPACE_PENALTY = 1100, GROWER_SPACE_PENALTY = 4800;
constexpr int ENEMY_THREAT_PENALTY = 260, GROWER_ENEMY_THREAT_PENALTY = 1700;
constexpr int ALLY_CONGESTION_PENALTY = 1600;
constexpr int HEAD_TRADE_MAX_LENGTH = 3, SPRINT_TRADE_MAX_LENGTH = 1000000;
constexpr bool KING_ENABLED = false;
constexpr int KING_SELECTION = 0, KING_ID = -1, ROOT_POLICY = 0, BACKUP_KING_POLICY = 0;
constexpr int FEED_START_ROUND = 450, FEEDER_MAX_LENGTH = 3;
constexpr int FEEDING_RADIUS = 3, KING_PROTECTION_RADIUS = 3;
constexpr int SACRIFICE_METHOD = 0; // 0 off; not implemented in EXP-001
constexpr int SONAR_STRATEGY = 4, SONAR_FREQUENCY = 1;
}

namespace {

using unswbc::Direction;
using unswbc::EdgeType;
using unswbc::Position;

constexpr int INF = std::numeric_limits<int>::max() / 4;

int direction_index(Direction direction) {
    switch (direction.value) {
    case Direction::NORTH: return 0;
    case Direction::EAST: return 1;
    case Direction::SOUTH: return 2;
    default: return 3;
    }
}

struct RememberedEdge {
    bool known = false;
    EdgeType type = EdgeType::EMPTY;
    int portal_id = -1;
};

struct CellMemory {
    bool seen = false;
    int last_seen = -1;
    int visits = 0;
    bool pearl = false;
    int pearl_seen = -1;
    int pearl_time = -1;
    int pearl_sample_round = -1;
    std::array<RememberedEdge, 4> edges{};
};

struct EdgeKey {
    bool horizontal = false;
    int x = 0;
    int y = 0;
    constexpr bool operator==(EdgeKey const&) const = default;
};

struct Step {
    bool passable = false;
    bool destination_known = false;
    bool portal = false;
    Position destination{};
};

struct SeenHead {
    int id = -1;
    unswbc::Team team = unswbc::Team::A;
    Position position{};
    Direction facing = Direction::NORTH;
    int visible_length = 1;
};

struct TeamSignal {
    int id = -1;
    Position position{};
    int length = 0;
    int round = 0;
    bool grower = false;
};

struct Candidate {
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

class ReplayDerivedBot {
  public:
    ReplayDerivedBot(unswbc::Controller& controller, unswbc::Game& game_state)
        : controller_(controller), game_(game_state), width_(game_state.width),
          height_(game_state.height),
          memory_(static_cast<std::size_t>(width_ * height_)),
          enemy_threats_(memory_.size(), 0), ally_threats_(memory_.size(), 0) {}

    void play_turn() {
        receive_status();
        remember_vision();
        observe_heads();
        build_threat_maps();

        CellMemory& current = cell(controller_.get_position());
        ++current.visits;

        if (should_split()) {
            int const child_size = controller_.get_length() - unswbc::Constants::MIN_SIZE;
            controller_.do_split(child_size);
            send_status(controller_.get_position(), unswbc::Constants::MIN_SIZE);
            return;
        }

        std::vector<Candidate> candidates = single_step_candidates(false);
        if (candidates.empty()) candidates = single_step_candidates(true);
        add_tactical_sprints(candidates);

        if (candidates.empty()) {
            // Preserve the replay's zero-kelp-death invariant even in a fully
            // occupied pocket: prefer a passable collision over a kelp edge.
            for (Direction direction : Direction::get_direction_list()) {
                Step const step = step_from(controller_.get_position(), direction);
                if (!step.passable) continue;
                controller_.make_move(direction);
                send_status(step.destination_known ? step.destination
                                                    : controller_.get_position(),
                            controller_.get_length());
                return;
            }
            // A tile enclosed by kelp on all four sides cannot normally be a
            // spawn, but an action is still required by the protocol.
            controller_.make_move(controller_.get_dir());
            send_status(controller_.get_position(), controller_.get_length());
            return;
        }

        bool const conservative = retaining_length() || controller_.get_length() >= 4;
        bool const has_safe = std::any_of(candidates.begin(), candidates.end(),
                                          [](Candidate const& move) { return move.safe; });
        Candidate const* best = nullptr;
        for (Candidate const& move : candidates) {
            if (conservative && has_safe && !move.safe) continue;
            if (!best || move.score > best->score) best = &move;
        }
        if (!best) best = &*std::max_element(
            candidates.begin(), candidates.end(),
            [](Candidate const& a, Candidate const& b) { return a.score < b.score; });

        if (best->steps.size() == 1)
            controller_.make_move(best->steps.front());
        else
            controller_.make_moves(best->steps);

        int projected_length = controller_.get_length();
        if (best->direct_pearl) ++projected_length;
        projected_length -= static_cast<int>(best->steps.size()) - 1;
        send_status(best->destination_known ? best->destination
                                            : controller_.get_position(),
                    std::max(2, projected_length));
    }

  private:
    // The replay has three behavioural periods. Up to round 300 every dragon
    // is a splitter. Through round 425 a stable quarter retains growth while
    // the rest maintain the swarm. In the final 75 rounds all survivors grow.
    static constexpr int SPECIALISE_ROUND = Config::SPLIT_CUTOFF;
    static constexpr int FINAL_GROWTH_ROUND = Config::FINAL_GROWTH_CUTOFF;
    static constexpr std::uint64_t STATUS_MAGIC = 0xB8ULL;

    unswbc::Controller& controller_;
    unswbc::Game& game_;
    int width_;
    int height_;
    std::vector<CellMemory> memory_;
    std::map<int, std::vector<EdgeKey>> portal_edges_;
    std::vector<SeenHead> heads_;
    std::vector<TeamSignal> signals_;
    std::vector<int> enemy_threats_;
    std::vector<int> ally_threats_;

    int wrap(int value, int size) const {
        value %= size;
        return value < 0 ? value + size : value;
    }

    int index(Position position) const {
        return position.y * width_ + position.x;
    }

    CellMemory& cell(Position position) {
        return memory_[static_cast<std::size_t>(index(position))];
    }

    CellMemory const& cell(Position position) const {
        return memory_[static_cast<std::size_t>(index(position))];
    }

    int toroidal_axis_distance(int a, int b, int size) const {
        int const direct = std::abs(a - b);
        return std::min(direct, size - direct);
    }

    int distance(Position a, Position b) const {
        return toroidal_axis_distance(a.x, b.x, width_) +
               toroidal_axis_distance(a.y, b.y, height_);
    }

    int chebyshev_distance(Position a, Position b) const {
        return std::max(toroidal_axis_distance(a.x, b.x, width_),
                        toroidal_axis_distance(a.y, b.y, height_));
    }

    static std::uint64_t mix(std::uint64_t value) {
        value += 0x9E3779B97F4A7C15ULL;
        value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31U);
    }

    bool selected_grower() const {
        // A mixed ID produces a stable role without creating map-wide stripes
        // from consecutive split-child IDs.
        return (Config::GROWER_SELECTION == 0 ?
                mix(static_cast<std::uint64_t>(controller_.get_id()) ^ 0xD1B54A32D192ED03ULL) :
                static_cast<std::uint64_t>(controller_.get_id())) % Config::GROWER_DENOMINATOR < Config::GROWER_NUMERATOR;
    }

    bool retaining_length() const {
        int const round = game_.get_round_num();
        return round > FINAL_GROWTH_ROUND ||
               ((round > SPECIALISE_ROUND || controller_.get_unit_count() >= 48) && selected_grower());
    }

    bool should_split() const {
        if (retaining_length() || controller_.get_length() <= Config::MAX_PRE_GROWTH_LENGTH) return false;
        if (controller_.get_unit_count() >= std::min(Config::TARGET_SWARM_SIZE, Config::MAX_SWARM_SIZE)) return false;
        int const child_size = controller_.get_length() - unswbc::Constants::MIN_SIZE;
        return controller_.can_split(child_size);
    }

    EdgeKey canonical_edge(Position position, Direction direction) const {
        switch (direction.value) {
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

    void remember_portal(int portal_id, EdgeKey endpoint) {
        std::vector<EdgeKey>& endpoints = portal_edges_[portal_id];
        if (std::find(endpoints.begin(), endpoints.end(), endpoint) == endpoints.end())
            endpoints.push_back(endpoint);
    }

    std::optional<Position> portal_exit(Position position, Direction direction,
                                        int portal_id) const {
        auto const found = portal_edges_.find(portal_id);
        if (found == portal_edges_.end() || found->second.size() != 2)
            return std::nullopt;

        EdgeKey const entry = canonical_edge(position, direction);
        EdgeKey const* other = nullptr;
        for (EdgeKey const& endpoint : found->second) {
            if (!(endpoint == entry)) {
                other = &endpoint;
                break;
            }
        }
        if (!other) return std::nullopt;

        if (other->horizontal) {
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

    Step step_from(Position position, Direction direction) const {
        RememberedEdge edge;
        if (unswbc::Tile const* tile = controller_.get_tile(position)) {
            unswbc::Edge const& visible = tile->get_edge(direction);
            edge = {true, visible.get_edge_type(), visible.get_portal_id()};
        } else {
            edge = cell(position).edges[static_cast<std::size_t>(direction_index(direction))];
        }

        if (edge.known && edge.type == EdgeType::KELP) return {};
        if (edge.known && edge.type == EdgeType::PORTAL) {
            std::optional<Position> const exit = portal_exit(position, direction, edge.portal_id);
            if (!exit) return {true, false, true, position};
            return {true, true, true, *exit};
        }
        return {true, true, false, position.add_dir(direction)};
    }

    void remember_vision() {
        int const round = game_.get_round_num();
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            CellMemory& remembered = cell(tile.get_position());
            remembered.seen = true;
            remembered.last_seen = round;
            remembered.pearl = tile.has_pearl();
            remembered.pearl_seen = tile.has_pearl() ? round : -1;
            remembered.pearl_time = tile.get_pearl_time();
            remembered.pearl_sample_round = round;

            for (Direction direction : Direction::get_direction_list()) {
                unswbc::Edge const& edge = tile.get_edge(direction);
                RememberedEdge& known = remembered.edges[
                    static_cast<std::size_t>(direction_index(direction))];
                known = {true, edge.get_edge_type(), edge.get_portal_id()};
                if (edge.is_portal())
                    remember_portal(edge.get_portal_id(),
                                    canonical_edge(tile.get_position(), direction));
            }
        }
    }

    void observe_heads() {
        heads_.clear();
        std::unordered_map<int, int> visible_lengths;
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (part) ++visible_lengths[part->get_id()];
        }
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (!part || !part->is_head() || part->get_id() == controller_.get_id())
                continue;
            heads_.push_back({part->get_id(), part->get_team(), part->get_position(),
                              part->get_dir(), visible_lengths[part->get_id()]});
        }
    }

    void build_threat_maps() {
        std::fill(enemy_threats_.begin(), enemy_threats_.end(), 0);
        std::fill(ally_threats_.begin(), ally_threats_.end(), 0);
        for (SeenHead const& head : heads_) {
            for (Direction direction : Direction::get_direction_list()) {
                if (direction == head.facing.get_opposite()) continue;
                Step const step = step_from(head.position, direction);
                if (!step.passable || !step.destination_known) continue;
                std::vector<int>& threats =
                    head.team == controller_.get_team() ? ally_threats_ : enemy_threats_;
                ++threats[static_cast<std::size_t>(index(step.destination))];
            }
        }
    }

    unswbc::DragonPart const* visible_part(Position position) const {
        unswbc::Tile const* tile = controller_.get_tile(position);
        return tile ? tile->get_dragon() : nullptr;
    }

    bool visible_occupied(Position position) const {
        return visible_part(position) != nullptr;
    }

    bool visible_pearl(Position position) const {
        unswbc::Tile const* tile = controller_.get_tile(position);
        return tile && tile->has_pearl();
    }

    int open_exits(Position position) const {
        int exits = 0;
        for (Direction direction : Direction::get_direction_list()) {
            Step const step = step_from(position, direction);
            if (!step.passable) continue;
            if (step.destination_known && visible_occupied(step.destination)) continue;
            ++exits;
        }
        return exits;
    }

    int flood_space(Position start) const {
        if (!start.is_in_map()) return 0;
        std::vector<bool> visited(memory_.size(), false);
        std::deque<Position> queue;
        visited[static_cast<std::size_t>(index(start))] = true;
        queue.push_back(start);
        int count = 0;

        while (!queue.empty() && count < 48) {
            Position const current = queue.front();
            queue.pop_front();
            ++count;
            for (Direction direction : Direction::get_direction_list()) {
                Step const step = step_from(current, direction);
                if (!step.passable || !step.destination_known) continue;
                int const destination_index = index(step.destination);
                if (visited[static_cast<std::size_t>(destination_index)]) continue;
                if (visible_occupied(step.destination) &&
                    !(step.destination == controller_.get_position()))
                    continue;
                visited[static_cast<std::size_t>(destination_index)] = true;
                queue.push_back(step.destination);
            }
        }
        return count;
    }

    int nearest_objective_distance(Position position) const {
        int best = INF;
        int const round = game_.get_round_num();
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Position const target(x, y);
                CellMemory const& remembered = cell(target);
                if (remembered.pearl) {
                    int const staleness = std::max(0, round - remembered.pearl_seen);
                    best = std::min(best, distance(position, target) + staleness / 18);
                    continue;
                }
                if (remembered.pearl_time < 0 || remembered.pearl_sample_round < 0)
                    continue;
                int const remaining = remembered.pearl_time -
                                      (round - remembered.pearl_sample_round);
                if (remaining <= 4)
                    best = std::min(best, distance(position, target) +
                                          std::max(0, remaining) + 2);
            }
        }
        return best;
    }

    std::pair<int, int> nearest_visible_enemy_delta(Position before,
                                                     Position after) const {
        int old_distance = INF;
        int new_distance = INF;
        for (SeenHead const& head : heads_) {
            if (head.team == controller_.get_team()) continue;
            old_distance = std::min(old_distance, distance(before, head.position));
            new_distance = std::min(new_distance, distance(after, head.position));
        }
        return {old_distance, new_distance};
    }

    int teammate_spacing_score(Position destination) const {
        int score = 0;
        for (SeenHead const& head : heads_) {
            if (head.team != controller_.get_team()) continue;
            int const gap = chebyshev_distance(destination, head.position);
            if (gap <= 1) score -= 900;
            else if (gap == 2) score -= 260;
            else if (gap >= 4) score += 20;
            if (head.visible_length > controller_.get_length() && gap <= 3)
                score -= 180;
        }
        for (TeamSignal const& signal : signals_) {
            if (signal.id == controller_.get_id()) continue;
            int const gap = chebyshev_distance(destination, signal.position);
            if (gap <= 1) score -= 700;
            else if (gap == 2) score -= 180;
            if (signal.length > controller_.get_length() && gap <= 3)
                score -= 240;
        }
        return score;
    }

    int deterministic_jitter(Direction direction) const {
        Position const position = controller_.get_position();
        std::uint64_t seed = static_cast<std::uint64_t>(controller_.get_id());
        seed = seed * 1000003ULL + static_cast<std::uint64_t>(game_.get_round_num());
        seed = seed * 1009ULL + static_cast<std::uint64_t>(position.x * 131 + position.y);
        seed = seed * 17ULL + static_cast<unsigned char>(direction.value);
        return static_cast<int>(mix(seed) % 101ULL);
    }

    Candidate evaluate_single(Direction direction) const {
        Candidate candidate;
        candidate.steps.push_back(direction);
        Position const origin = controller_.get_position();
        Step const step = step_from(origin, direction);
        if (!step.passable) return candidate;

        candidate.destination_known = step.destination_known;
        candidate.portal = step.portal;
        candidate.unknown_portal = step.portal && !step.destination_known;
        candidate.destination = step.destination_known ? step.destination : origin;

        unswbc::DragonPart const* occupant =
            step.destination_known ? visible_part(step.destination) : nullptr;
        if (occupant) {
            bool const enemy_head = occupant->is_head() &&
                                    occupant->get_team() != controller_.get_team();
            bool const may_trade = enemy_head && !retaining_length() &&
                                   controller_.get_length() <= Config::HEAD_TRADE_MAX_LENGTH;
            if (!may_trade) return candidate;
            candidate.head_trade = true;
        }

        candidate.direct_pearl = step.destination_known && visible_pearl(step.destination);
        if (step.destination_known) {
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
        if (direction == facing) score += 80;
        else if (direction == facing.get_opposite()) score -= 850;
        else score += 15;

        bool const grow = retaining_length();
        if (candidate.portal) score += grow ? Config::GROWER_PORTAL_REWARD : Config::PORTAL_REWARD;
        if (candidate.unknown_portal) score -= grow ? Config::GROWER_UNKNOWN_PORTAL_PENALTY : Config::UNKNOWN_PORTAL_PENALTY;

        int const old_objective = nearest_objective_distance(origin);
        int const new_objective = step.destination_known
                                      ? nearest_objective_distance(step.destination)
                                      : old_objective;
        if (old_objective < INF && new_objective < INF) {
            int const weight = grow ? 310 : 105;
            score += (old_objective - new_objective) * weight;
        }
        if (candidate.direct_pearl) score += grow ? 2800 : 850;

        if (step.destination_known) {
            CellMemory const& destination = cell(step.destination);
            int const age = destination.seen
                                ? std::max(0, game_.get_round_num() - destination.last_seen)
                                : 80;
            score += grow ? std::min(Config::GROWER_EXPLORATION_CAP, age * Config::GROWER_EXPLORATION_WEIGHT) : std::min(Config::EXPLORATION_CAP, age * Config::EXPLORATION_WEIGHT);
            score -= destination.visits * (grow ? Config::GROWER_VISIT_PENALTY : Config::VISIT_PENALTY);
            score += candidate.exits * 55;
            score += std::min(36, candidate.space) * (grow ? 13 : 8);
            if (candidate.exits == 0) score -= 5000;
            else if (candidate.exits == 1) score -= grow ? 1300 : 280;
            int const space_needed = std::min(14, controller_.get_length() + 3);
            if (candidate.space < space_needed) score -= grow ? Config::GROWER_SPACE_PENALTY : Config::SPACE_PENALTY;
            score += teammate_spacing_score(step.destination);

            auto const [enemy_before, enemy_after] =
                nearest_visible_enemy_delta(origin, step.destination);
            if (enemy_before < INF) {
                // The replay swarm spreads around enemies but takes a direct
                // head trade whenever one becomes available.
                score += (enemy_after - enemy_before) * (grow ? 85 : 30);
            }
        }

        if (candidate.head_trade) score += 2400;
        if (candidate.enemy_threats > 0 && !candidate.head_trade)
            score -= candidate.enemy_threats * (grow ? Config::GROWER_ENEMY_THREAT_PENALTY : Config::ENEMY_THREAT_PENALTY);
        if (candidate.ally_threats > 0)
            score -= candidate.ally_threats * Config::ALLY_CONGESTION_PENALTY;

        candidate.safe = candidate.destination_known && !candidate.unknown_portal &&
                         !candidate.head_trade && candidate.enemy_threats == 0 &&
                         candidate.ally_threats == 0 && candidate.exits > 0 &&
                         candidate.space >= std::min(10, controller_.get_length() + 2);
        candidate.score = score;
        return candidate;
    }

    std::vector<Candidate> single_step_candidates(bool include_reverse) const {
        Direction const facing = controller_.get_dir();
        std::array<Direction, 4> const ordered{
            facing, facing.get_left(), facing.get_right(), facing.get_opposite()};
        std::vector<Candidate> candidates;
        for (Direction direction : ordered) {
            if (!include_reverse && direction == facing.get_opposite()) continue;
            Candidate move = evaluate_single(direction);
            if (move.score > -INF) candidates.push_back(std::move(move));
        }
        return candidates;
    }

    void add_tactical_sprints(std::vector<Candidate>& candidates) const {
        if (retaining_length() || controller_.get_length() < 3 || controller_.get_length() > Config::SPRINT_TRADE_MAX_LENGTH) return;
        Position const origin = controller_.get_position();
        Direction const facing = controller_.get_dir();

        for (Direction first : Direction::get_direction_list()) {
            if (first == facing.get_opposite()) continue;
            Step const one = step_from(origin, first);
            if (!one.passable || !one.destination_known || visible_occupied(one.destination))
                continue;

            for (Direction second : Direction::get_direction_list()) {
                if (second == first.get_opposite()) continue;
                Step const two = step_from(one.destination, second);
                if (!two.passable || !two.destination_known) continue;
                unswbc::DragonPart const* target = visible_part(two.destination);
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
                attack.score = 2250 + deterministic_jitter(first) - 120;
                attack.safe = false;
                candidates.push_back(std::move(attack));
            }
        }
    }

    void receive_status() {
        signals_.clear();
        int const now = game_.get_round_num();
        for (std::uint64_t message : controller_.get_sonar_messages()) {
            if ((message >> 56U) != STATUS_MAGIC) continue;
            int const encoded_team = static_cast<int>((message >> 55U) & 1ULL);
            int const own_team = controller_.get_team().value == unswbc::Team::B ? 1 : 0;
            if (encoded_team != own_team) continue;

            TeamSignal signal;
            signal.grower = ((message >> 54U) & 1ULL) != 0;
            signal.position.x = static_cast<int>((message >> 44U) & 0x3FFULL);
            signal.position.y = static_cast<int>((message >> 34U) & 0x3FFULL);
            signal.length = static_cast<int>((message >> 24U) & 0x3FFULL);
            signal.round = static_cast<int>((message >> 15U) & 0x1FFULL);
            signal.id = static_cast<int>(message & 0x7FFFULL);
            if (!signal.position.is_in_map()) continue;
            int const age = (now - signal.round + 512) % 512;
            if (age <= 2 || age >= 510) signals_.push_back(signal);
        }
    }

    std::uint64_t status_message(Position position, int length) const {
        std::uint64_t message = STATUS_MAGIC << 56U;
        int const team_bit = controller_.get_team().value == unswbc::Team::B ? 1 : 0;
        message |= static_cast<std::uint64_t>(team_bit) << 55U;
        message |= static_cast<std::uint64_t>(retaining_length()) << 54U;
        message |= static_cast<std::uint64_t>(position.x & 0x3FF) << 44U;
        message |= static_cast<std::uint64_t>(position.y & 0x3FF) << 34U;
        message |= static_cast<std::uint64_t>(std::min(1023, length)) << 24U;
        message |= static_cast<std::uint64_t>(game_.get_round_num() & 0x1FF) << 15U;
        message |= static_cast<std::uint64_t>(controller_.get_id() & 0x7FFF);
        return message;
    }

    void send_status(Position predicted_position, int predicted_length) {
        if (Config::SONAR_STRATEGY == 0 || game_.get_round_num() % Config::SONAR_FREQUENCY != 0) return;
        std::uint64_t const message = status_message(predicted_position, predicted_length);
        // Team B emits four directional sonar messages throughout the late
        // game. Doing this from the opening also makes right-of-way useful as
        // soon as the first split children appear.
        for (Direction direction : Direction::get_direction_list())
            controller_.send_sonar(direction, message);
    }
};

} // namespace

int main() {
    auto [controller, game] = unswbc::init();
    ReplayDerivedBot bot(controller, game);
    while (unswbc::update(controller, game)) {
        bot.play_turn();
        unswbc::end_turn();
    }
    return 0;
}
