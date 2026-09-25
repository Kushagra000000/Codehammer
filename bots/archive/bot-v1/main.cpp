#include "helper.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <vector>

namespace {

using unswbc::Direction;
using unswbc::EdgeType;
using unswbc::Position;

constexpr int UNREACHABLE = std::numeric_limits<int>::max() / 4;

int direction_index(Direction direction) {
    switch (direction.value) {
    case Direction::NORTH: return 0;
    case Direction::EAST: return 1;
    case Direction::SOUTH: return 2;
    default: return 3;
    }
}

enum class GameStage {
    OPENING,
    EXPANSION,
    CONSOLIDATION,
    ENDGAME,
};

struct RememberedEdge {
    bool known = false;
    EdgeType type = EdgeType::EMPTY;
    int portal_id = -1;
};

struct CellMemory {
    bool seen = false;
    int last_seen_round = -1;
    bool has_pearl = false;
    int pearl_seen_round = -1;
    int pearl_time = -1;
    int pearl_sample_round = -1;
    int sonar_pearl_until = -1;
    int danger_until = -1;
    std::array<RememberedEdge, 4> edges{};
};

struct EdgeKey {
    // Horizontal: north edge of (x,y). Vertical: west edge of (x,y).
    bool horizontal = false;
    int x = 0;
    int y = 0;
    constexpr bool operator==(EdgeKey const&) const = default;
};

struct StepResult {
    bool passable = false;
    bool destination_known = false;
    Position destination{};
};

struct MovePlan {
    std::vector<Direction> steps;
    Position destination{};
    bool destination_known = true;
    bool destination_visible = true;
    int pearls_eaten = 0;
    int sprint_cost = 0;
    int open_exits = 0;
    int later_head_threats = 0;
    int nearby_head_threats = 0;
    int score = std::numeric_limits<int>::min();
};

enum class SignalType : std::uint32_t {
    PEARL = 0,
    DANGER = 1,
};

class SnakeBot {
  public:
    SnakeBot(unswbc::Controller& controller, unswbc::Game& game_state)
        : controller_(controller), game_(game_state), width_(game_state.width),
          height_(game_state.height),
          cells_(static_cast<std::size_t>(width_ * height_)),
          objective_cache_(cells_.size(), 0),
          objective_cached_(cells_.size(), false) {}

    void execute_turn() {
        receive_sonar();
        remember_vision();
        stage_ = get_stage();
        std::fill(objective_cached_.begin(), objective_cached_.end(), false);

        if (should_split()) {
            controller_.do_split(2);
            split_once_ = true;
            send_sonar();
            return;
        }

        std::vector<MovePlan> plans = generate_plans();
        if (plans.empty()) {
            // The engine kills a dragon that emits no valid action. If every
            // visible route is fatal, still issue a well-formed move.
            controller_.make_move(controller_.get_dir());
            send_sonar();
            return;
        }

        bool const has_fully_observed_move = std::any_of(
            plans.begin(), plans.end(), [](MovePlan const& plan) {
                return plan.destination_known && plan.destination_visible;
            });
        for (MovePlan& plan : plans)
            score_plan(plan, has_fully_observed_move);

        MovePlan const& best = *std::max_element(
            plans.begin(), plans.end(),
            [](MovePlan const& left, MovePlan const& right) {
                return left.score < right.score;
            });

        if (best.steps.size() == 1)
            controller_.make_move(best.steps.front());
        else
            controller_.make_moves(best.steps);
        send_sonar();
    }

  private:
    static constexpr std::uint32_t SIGNAL_MAGIC = 0xBU;

    unswbc::Controller& controller_;
    unswbc::Game& game_;
    int width_;
    int height_;
    std::vector<CellMemory> cells_;
    std::vector<int> objective_cache_;
    std::vector<bool> objective_cached_;
    std::map<int, std::vector<EdgeKey>> portal_edges_;
    GameStage stage_ = GameStage::OPENING;
    bool split_once_ = false;

    int wrap(int value, int size) const {
        value %= size;
        return value < 0 ? value + size : value;
    }

    int index(Position position) const {
        return position.y * width_ + position.x;
    }

    CellMemory const& memory(Position position) const {
        return cells_[static_cast<std::size_t>(index(position))];
    }

    CellMemory& memory(Position position) {
        return cells_[static_cast<std::size_t>(index(position))];
    }

    GameStage get_stage() const {
        int const round = game_.get_round_num();
        if (round <= 125) return GameStage::OPENING;
        if (round <= 300) return GameStage::EXPANSION;
        if (round <= 424) return GameStage::CONSOLIDATION;
        return GameStage::ENDGAME;
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

    void remember_portal(int portal_id, EdgeKey edge) {
        auto& endpoints = portal_edges_[portal_id];
        if (std::find(endpoints.begin(), endpoints.end(), edge) == endpoints.end())
            endpoints.push_back(edge);
    }

    void remember_vision() {
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            CellMemory& cell = memory(tile.get_position());
            cell.seen = true;
            cell.last_seen_round = game_.get_round_num();
            cell.has_pearl = tile.has_pearl();
            cell.pearl_seen_round = tile.has_pearl() ? game_.get_round_num() : -1;
            cell.pearl_time = tile.get_pearl_time();
            cell.pearl_sample_round = game_.get_round_num();
            if (!tile.has_pearl()) cell.sonar_pearl_until = -1;

            for (Direction direction : Direction::get_direction_list()) {
                unswbc::Edge const& edge = tile.get_edge(direction);
                RememberedEdge& remembered =
                    cell.edges[static_cast<std::size_t>(direction_index(direction))];
                remembered.known = true;
                remembered.type = edge.get_edge_type();
                remembered.portal_id = edge.get_portal_id();
                if (edge.is_portal())
                    remember_portal(edge.get_portal_id(),
                                    canonical_edge(tile.get_position(), direction));
            }
        }
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

    StepResult step_from(Position position, Direction direction) const {
        if (unswbc::Tile const* tile = controller_.get_tile(position)) {
            unswbc::Edge const& edge = tile->get_edge(direction);
            if (!edge.is_passable()) return {};
            if (edge.is_portal()) {
                auto const destination =
                    portal_exit(position, direction, edge.get_portal_id());
                if (!destination) return {true, false, position};
                return {true, true, *destination};
            }
            return {true, true, position.add_dir(direction)};
        }

        RememberedEdge const& edge =
            memory(position).edges[static_cast<std::size_t>(direction_index(direction))];
        if (!edge.known)
            return {true, true, position.add_dir(direction)};
        if (edge.type == EdgeType::KELP) return {};
        if (edge.type == EdgeType::PORTAL) {
            auto const destination = portal_exit(position, direction, edge.portal_id);
            if (!destination) return {true, false, position};
            return {true, true, *destination};
        }
        return {true, true, position.add_dir(direction)};
    }

    std::optional<Position> remembered_step(Position position,
                                            Direction direction) const {
        StepResult const step = step_from(position, direction);
        if (!step.passable || !step.destination_known) return std::nullopt;
        return step.destination;
    }

    std::vector<bool> current_occupancy() const {
        std::vector<bool> occupied(cells_.size(), false);
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            if (tile.get_dragon())
                occupied[static_cast<std::size_t>(index(tile.get_position()))] = true;
        }
        return occupied;
    }

    int count_open_exits(Position position,
                         std::vector<bool> const& occupied) const {
        int exits = 0;
        for (Direction direction : Direction::get_direction_list()) {
            StepResult const step = step_from(position, direction);
            if (!step.passable || !step.destination_known) continue;
            if (occupied[static_cast<std::size_t>(index(step.destination))]) continue;
            ++exits;
        }
        return exits;
    }

    void count_head_threats(MovePlan& plan) const {
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (!part || !part->is_head()) continue;
            if (part->get_team() == controller_.get_team() &&
                part->get_id() == controller_.get_id())
                continue;

            bool can_reach = false;
            for (Direction direction : Direction::get_direction_list()) {
                StepResult const step = step_from(part->get_position(), direction);
                if (step.passable && step.destination_known &&
                    step.destination == plan.destination) {
                    can_reach = true;
                    break;
                }
            }
            if (!can_reach) continue;

            ++plan.nearby_head_threats;
            if (part->get_id() > controller_.get_id())
                ++plan.later_head_threats;
        }
    }

    std::optional<MovePlan> evaluate_path(
        std::vector<Direction> const& steps) const {
        if (steps.empty() ||
            static_cast<int>(steps.size()) > controller_.get_length() - 1)
            return std::nullopt;

        MovePlan plan;
        plan.steps = steps;
        plan.sprint_cost = static_cast<int>(steps.size()) - 1;
        Position current = controller_.get_position();
        std::vector<bool> occupied = current_occupancy();
        std::vector<bool> eaten(cells_.size(), false);

        for (std::size_t step_index = 0; step_index < steps.size(); ++step_index) {
            StepResult const step = step_from(current, steps[step_index]);
            if (!step.passable) return std::nullopt;
            if (!step.destination_known) {
                if (steps.size() != 1) return std::nullopt;
                plan.destination = current;
                plan.destination_known = false;
                plan.destination_visible = false;
                return plan;
            }

            Position const destination = step.destination;
            std::size_t const destination_index =
                static_cast<std::size_t>(index(destination));
            if (occupied[destination_index]) return std::nullopt;

            unswbc::Tile const* destination_tile =
                controller_.get_tile(destination);
            if (!destination_tile && steps.size() > 1)
                return std::nullopt;
            if (destination_tile && destination_tile->has_pearl() &&
                !eaten[destination_index]) {
                ++plan.pearls_eaten;
                eaten[destination_index] = true;
            }

            // Conservative sprint simulation: old body cells are never removed.
            // That can reject a safe tail-vacating route, but cannot approve a
            // self-collision. Every new head cell stays blocked for later steps.
            occupied[destination_index] = true;
            current = destination;
        }

        plan.destination = current;
        plan.destination_visible = controller_.get_tile(current) != nullptr;
        plan.open_exits = count_open_exits(current, occupied);
        count_head_threats(plan);
        return plan;
    }

    bool sprint_is_worthwhile(MovePlan const& plan) const {
        if (plan.steps.size() == 1) return true;
        if (stage_ == GameStage::ENDGAME) return false;
        int const net_growth = plan.pearls_eaten - plan.sprint_cost;
        if (stage_ == GameStage::CONSOLIDATION) return net_growth >= 2;
        return net_growth >= 1;
    }

    void enumerate_sequences(int target_length,
                             std::vector<Direction>& sequence,
                             std::vector<MovePlan>& plans) const {
        if (static_cast<int>(sequence.size()) == target_length) {
            auto plan = evaluate_path(sequence);
            if (plan && sprint_is_worthwhile(*plan))
                plans.push_back(std::move(*plan));
            return;
        }
        for (Direction direction : Direction::get_direction_list()) {
            sequence.push_back(direction);
            enumerate_sequences(target_length, sequence, plans);
            sequence.pop_back();
        }
    }

    std::vector<MovePlan> generate_plans() const {
        std::vector<MovePlan> plans;
        int max_steps = std::min(3, controller_.get_length() - 1);
        if (stage_ == GameStage::ENDGAME) max_steps = 1;
        max_steps = std::max(1, max_steps);

        std::vector<Direction> sequence;
        sequence.reserve(static_cast<std::size_t>(max_steps));
        for (int length = 1; length <= max_steps; ++length)
            enumerate_sequences(length, sequence, plans);
        return plans;
    }

    std::vector<int> distances_from(Position start) const {
        std::vector<int> distances(cells_.size(), UNREACHABLE);
        std::queue<Position> frontier;
        distances[static_cast<std::size_t>(index(start))] = 0;
        frontier.push(start);

        while (!frontier.empty()) {
            Position const current = frontier.front();
            frontier.pop();
            int const next_distance =
                distances[static_cast<std::size_t>(index(current))] + 1;
            for (Direction direction : Direction::get_direction_list()) {
                auto const next = remembered_step(current, direction);
                if (!next) continue;
                int& known = distances[static_cast<std::size_t>(index(*next))];
                if (next_distance >= known) continue;
                known = next_distance;
                frontier.push(*next);
            }
        }
        return distances;
    }

    int objective_score(Position start) const {
        std::vector<int> const distances = distances_from(start);
        int best_pearl = std::numeric_limits<int>::min() / 4;
        int best_exploration = std::numeric_limits<int>::min() / 4;
        int const round = game_.get_round_num();

        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Position const position(x, y);
                int const distance =
                    distances[static_cast<std::size_t>(index(position))];
                if (distance == UNREACHABLE) continue;
                CellMemory const& cell = memory(position);

                if (!cell.seen) {
                    int value = 550 - distance * 35;
                    if (stage_ == GameStage::OPENING) value += 250;
                    if (stage_ == GameStage::CONSOLIDATION) value /= 3;
                    if (stage_ == GameStage::ENDGAME) value /= 8;
                    best_exploration = std::max(best_exploration, value);
                } else {
                    int value = std::min(300, round - cell.last_seen_round) -
                                distance * 12;
                    if (stage_ == GameStage::OPENING) value *= 2;
                    if (stage_ == GameStage::ENDGAME) value /= 4;
                    best_exploration = std::max(best_exploration, value);
                }

                if (cell.has_pearl) {
                    int const age = round - cell.pearl_seen_round;
                    best_pearl = std::max(
                        best_pearl, 3600 - distance * 85 - age * 30);
                    continue;
                }

                if (stage_ != GameStage::ENDGAME &&
                    cell.sonar_pearl_until >= round) {
                    best_pearl = std::max(
                        best_pearl, 1500 - distance * 65);
                }

                if (stage_ == GameStage::ENDGAME || cell.pearl_time < 0)
                    continue;
                int const due_round =
                    cell.pearl_sample_round + cell.pearl_time;
                if (due_round <= round && cell.last_seen_round < due_round &&
                    round - due_round <= 30) {
                    best_pearl = std::max(
                        best_pearl,
                        1200 - distance * 55 - (round - due_round) * 12);
                } else if (due_round > round && due_round - round <= 20) {
                    int const wait = due_round - round;
                    best_pearl = std::max(
                        best_pearl,
                        750 - std::abs(distance - wait) * 35 - distance * 10);
                }
            }
        }

        if (best_pearl > 0) return best_pearl + best_exploration / 10;
        return best_exploration;
    }

    int cached_objective_score(Position start) {
        std::size_t const position_index =
            static_cast<std::size_t>(index(start));
        if (!objective_cached_[position_index]) {
            objective_cache_[position_index] = objective_score(start);
            objective_cached_[position_index] = true;
        }
        return objective_cache_[position_index];
    }

    int tie_break(MovePlan const& plan) const {
        std::uint32_t value =
            static_cast<std::uint32_t>(controller_.get_id() + 1);
        value ^= static_cast<std::uint32_t>(game_.get_round_num()) * 0x9E3779B9U;
        value ^= static_cast<std::uint32_t>(
                     direction_index(plan.steps.front()) + 1) *
                 0x85EBCA6BU;
        value ^= static_cast<std::uint32_t>(plan.steps.size()) * 0xC2B2AE35U;
        value ^= value >> 16U;
        return static_cast<int>(value & 7U);
    }

    void score_plan(MovePlan& plan, bool has_fully_observed_move) {
        int value = 0;
        int pearl_value = 120000;
        int exit_value = 45;
        if (stage_ == GameStage::CONSOLIDATION) exit_value = 85;
        if (stage_ == GameStage::ENDGAME) {
            pearl_value = controller_.get_length() < 6 ? 50000 : 22000;
            exit_value = 150;
        }

        value += plan.pearls_eaten * pearl_value;
        value -= plan.sprint_cost * 45000;
        if (plan.destination_known) {
            value += cached_objective_score(plan.destination);
            value += plan.open_exits * exit_value;
            if (memory(plan.destination).danger_until >= game_.get_round_num())
                value -= 12000;
        } else {
            value -= has_fully_observed_move ? 80000 : 4000;
        }

        if (!plan.destination_visible)
            value -= has_fully_observed_move ? 30000 : 1000;

        // A higher-id head can still move onto our final head tile this round.
        // Make that worse than any ordinary pearl grab.
        value -= plan.later_head_threats * 250000;
        value -= (plan.nearby_head_threats - plan.later_head_threats) *
                 (stage_ == GameStage::ENDGAME ? 2500 : 600);

        Direction const first = plan.steps.front();
        if (first == controller_.get_dir())
            value += 18;
        else if (first == controller_.get_dir().get_opposite())
            value -= 1200;
        else
            value += 8;

        if (stage_ == GameStage::ENDGAME)
            value -= plan.sprint_cost * 100000;
        value += tie_break(plan);
        plan.score = value;
    }

    int desired_team_size() const {
        int const area = width_ * height_;
        return std::clamp((area + 255) / 256 + 1, 2, 12);
    }

    int observed_safe_single_moves() const {
        int safe = 0;
        for (Direction direction : Direction::get_direction_list()) {
            std::vector<Direction> const path{direction};
            auto const plan = evaluate_path(path);
            if (plan && plan->destination_known && plan->destination_visible &&
                plan->later_head_threats == 0)
                ++safe;
        }
        return safe;
    }

    bool should_split() const {
        if (split_once_ || stage_ == GameStage::CONSOLIDATION ||
            stage_ == GameStage::ENDGAME)
            return false;
        if (controller_.get_unit_count() >= desired_team_size()) return false;
        int const minimum_length =
            stage_ == GameStage::OPENING ? 8 : 12;
        if (controller_.get_length() < minimum_length ||
            !controller_.can_split(2))
            return false;
        return observed_safe_single_moves() >= 2;
    }

    std::uint32_t encode_signal(SignalType type, Position position,
                                int payload) const {
        std::uint32_t value = SIGNAL_MAGIC << 28U;
        value |= (static_cast<std::uint32_t>(type) & 0x3U) << 26U;
        value |= (static_cast<std::uint32_t>(position.x) & 0x3FU) << 20U;
        value |= (static_cast<std::uint32_t>(position.y) & 0x3FU) << 14U;
        value |= (static_cast<std::uint32_t>(game_.get_round_num()) & 0x1FFU)
                 << 5U;
        value |= static_cast<std::uint32_t>(std::clamp(payload, 0, 31));
        return value;
    }

    void receive_sonar() {
        int const current_mod = game_.get_round_num() & 0x1FF;
        for (std::uint32_t message : controller_.get_sonar_messages()) {
            if ((message >> 28U) != SIGNAL_MAGIC) continue;
            auto const type = static_cast<SignalType>((message >> 26U) & 0x3U);
            Position const position(
                static_cast<int>((message >> 20U) & 0x3FU),
                static_cast<int>((message >> 14U) & 0x3FU));
            if (position.x >= width_ || position.y >= height_) continue;
            int const sent_mod = static_cast<int>((message >> 5U) & 0x1FFU);
            int const age = (current_mod - sent_mod + 512) % 512;
            if (age > 40) continue;

            if (type == SignalType::PEARL)
                memory(position).sonar_pearl_until =
                    std::max(memory(position).sonar_pearl_until,
                             game_.get_round_num() + std::max(2, 24 - age));
            else if (type == SignalType::DANGER)
                memory(position).danger_until =
                    std::max(memory(position).danger_until,
                             game_.get_round_num() + std::max(2, 12 - age));
        }
    }

    int wrapped_distance(Position left, Position right) const {
        int const dx = std::abs(left.x - right.x);
        int const dy = std::abs(left.y - right.y);
        return std::min(dx, width_ - dx) + std::min(dy, height_ - dy);
    }

    void send_sonar() {
        Position const here = controller_.get_position();
        unswbc::DragonPart const* closest_enemy = nullptr;
        int enemy_distance = UNREACHABLE;
        unswbc::Tile const* closest_pearl = nullptr;
        int pearl_distance = UNREACHABLE;

        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            if (unswbc::DragonPart const* part = tile.get_dragon();
                part && part->is_head() &&
                part->get_team() != controller_.get_team()) {
                int const distance = wrapped_distance(here, part->get_position());
                if (distance < enemy_distance) {
                    enemy_distance = distance;
                    closest_enemy = part;
                }
            }
            if (tile.has_pearl()) {
                int const distance = wrapped_distance(here, tile.get_position());
                if (distance < pearl_distance) {
                    pearl_distance = distance;
                    closest_pearl = &tile;
                }
            }
        }

        if (closest_enemy && enemy_distance <= 4) {
            controller_.send_sonar(encode_signal(
                SignalType::DANGER, closest_enemy->get_position(), 0));
        } else if (closest_pearl) {
            controller_.send_sonar(encode_signal(
                SignalType::PEARL, closest_pearl->get_position(),
                std::max(0, closest_pearl->get_pearl_time())));
        }
    }
};

} // namespace

int main() {
    auto [controller, game] = unswbc::init();
    SnakeBot bot(controller, game);

    while (unswbc::update(controller, game)) {
        bot.execute_turn();
        unswbc::end_turn();
    }
}
