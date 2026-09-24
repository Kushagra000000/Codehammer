#include "helper.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <deque>
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

enum class BotRole {
    SURVIVOR,
    ATTACKER,
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
    int teammate_length = 0;
    int teammate_until = -1;
    int enemy_length = 0;
    int enemy_until = -1;
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
    int portals_used = 0;
    int pearls_eaten = 0;
    int sprint_cost = 0;
    int projected_length = 0;
    int open_exits = 0;
    int continuation_options = 0;
    int reachable_space = 0;
    int future_space = 0;
    int head_collision_threats = 0;
    int sprint_head_threats = 0;
    int priority_conflicts = 0;
    bool tail_reachable = false;
    bool immediate_trap = false;
    bool space_trap = false;
    int score = std::numeric_limits<int>::min();
};

enum class SignalType : std::uint32_t {
    STATUS = 0,
    PEARL = 1,
    DANGER = 2,
};

struct BodyInfo {
    bool complete = false;
    std::vector<Position> head_to_tail;
};

struct FloodResult {
    int space = 0;
    bool reached_tail = false;
};

class SnakeBot {
  public:
    SnakeBot(unswbc::Controller& controller, unswbc::Game& game_state)
        : controller_(controller), game_(game_state), width_(game_state.width),
          height_(game_state.height),
          cells_(static_cast<std::size_t>(width_ * height_)),
          objective_cache_(cells_.size(), 0),
          objective_cached_(cells_.size(), false),
          role_(controller.get_id() % 3 == 0 ? BotRole::ATTACKER
                                              : BotRole::SURVIVOR) {}

    void execute_turn() {
        ++turns_alive_;
        receive_sonar();
        remember_vision();
        stage_ = get_stage();
        std::fill(objective_cached_.begin(), objective_cached_.end(), false);

        int const split_size = planned_split_size();
        if (split_size > 0) {
            controller_.do_split(split_size);
            last_split_round_ = game_.get_round_num();
            send_sonar(controller_.get_position(),
                       controller_.get_length() - split_size);
            return;
        }

        std::vector<MovePlan> plans = generate_plans();
        if (plans.empty()) {
            // The engine kills a dragon that emits no valid action. If every
            // visible route is fatal, still issue a well-formed move.
            controller_.make_move(controller_.get_dir());
            send_sonar(controller_.get_position(), controller_.get_length());
            return;
        }

        bool const has_fully_observed_move = std::any_of(
            plans.begin(), plans.end(), [](MovePlan const& plan) {
                return plan.destination_known && plan.destination_visible;
            });
        bool const has_survivable_move = std::any_of(
            plans.begin(), plans.end(), [](MovePlan const& plan) {
                return plan.destination_known && plan.destination_visible &&
                       !plan.immediate_trap && !plan.space_trap &&
                       plan.head_collision_threats == 0;
            });
        for (MovePlan& plan : plans)
            score_plan(plan, has_fully_observed_move, has_survivable_move);

        MovePlan const& best = *std::max_element(
            plans.begin(), plans.end(),
            [](MovePlan const& left, MovePlan const& right) {
                return left.score < right.score;
            });

        if (best.steps.size() == 1)
            controller_.make_move(best.steps.front());
        else
            controller_.make_moves(best.steps);
        send_sonar(best.destination_known ? best.destination
                                          : controller_.get_position(),
                   best.projected_length);
    }

  private:
    static constexpr std::uint32_t SIGNAL_MAGIC = 0xCU;

    unswbc::Controller& controller_;
    unswbc::Game& game_;
    int width_;
    int height_;
    std::vector<CellMemory> cells_;
    std::vector<int> objective_cache_;
    std::vector<bool> objective_cached_;
    std::map<int, std::vector<EdgeKey>> portal_edges_;
    GameStage stage_ = GameStage::OPENING;
    BotRole role_;
    int turns_alive_ = 0;
    int last_split_round_ = -1000;

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

    bool late_growth_phase() const {
        return game_.get_round_num() > 300;
    }

    bool is_late_grower() const {
        return late_growth_phase() && controller_.get_id() % 2 == 0;
    }

    bool small_snake_length_control_active() const {
        if (controller_.get_length() <= 2 || is_late_grower())
            return false;
        // Small-role dragons grow to four and split while slots exist. At the
        // cap they spend excess segments because another child is illegal.
        return controller_.get_unit_count() >= desired_team_size();
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
        std::map<int, int> friendly_segment_counts;
        std::map<int, Position> friendly_heads;
        std::map<int, int> enemy_segment_counts;
        std::map<int, Position> enemy_heads;

        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (!part || part->get_id() == controller_.get_id()) continue;
            if (part->get_team() == controller_.get_team()) {
                ++friendly_segment_counts[part->get_id()];
                if (part->is_head()) friendly_heads[part->get_id()] = part->get_position();
            } else {
                ++enemy_segment_counts[part->get_id()];
                if (part->is_head()) enemy_heads[part->get_id()] = part->get_position();
            }
        }

        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            CellMemory& cell = memory(tile.get_position());
            cell.seen = true;
            cell.last_seen_round = game_.get_round_num();
            cell.has_pearl = tile.has_pearl();
            cell.pearl_seen_round = tile.has_pearl() ? game_.get_round_num() : -1;
            cell.pearl_time = tile.get_pearl_time();
            cell.pearl_sample_round = game_.get_round_num();
            if (!tile.has_pearl()) cell.sonar_pearl_until = -1;

            unswbc::DragonPart const* part = tile.get_dragon();
            bool const friendly_head =
                part && part->is_head() &&
                part->get_team() == controller_.get_team() &&
                part->get_id() != controller_.get_id();
            bool const enemy_head =
                part && part->is_head() &&
                part->get_team() != controller_.get_team();
            if (!friendly_head) {
                cell.teammate_length = 0;
                cell.teammate_until = -1;
            }
            if (!enemy_head) {
                cell.enemy_length = 0;
                cell.enemy_until = -1;
            }

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

        // Vision can only provide a lower bound when some body segments leave
        // the 7x7 window. Sonar status messages provide the exact capped length.
        for (auto const& [dragon_id, head] : friendly_heads) {
            CellMemory& cell = memory(head);
            cell.teammate_length = std::max(
                cell.teammate_length, friendly_segment_counts[dragon_id]);
            cell.teammate_until = game_.get_round_num();
        }
        for (auto const& [dragon_id, head] : enemy_heads) {
            CellMemory& cell = memory(head);
            cell.enemy_length = std::max(
                cell.enemy_length, enemy_segment_counts[dragon_id]);
            cell.enemy_until = game_.get_round_num();
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

    bool crosses_portal(Position position, Direction direction) const {
        if (unswbc::Tile const* tile = controller_.get_tile(position))
            return tile->get_edge(direction).is_portal();
        RememberedEdge const& edge =
            memory(position).edges[static_cast<std::size_t>(
                direction_index(direction))];
        return edge.known && edge.type == EdgeType::PORTAL;
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

    BodyInfo own_body_info() const {
        std::vector<unswbc::DragonPart const*> own_parts;
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (part && part->get_team() == controller_.get_team() &&
                part->get_id() == controller_.get_id())
                own_parts.push_back(part);
        }

        BodyInfo result;
        result.head_to_tail.push_back(controller_.get_position());
        std::vector<bool> used(own_parts.size(), false);
        for (std::size_t index = 0; index < own_parts.size(); ++index) {
            if (own_parts[index]->is_head()) used[index] = true;
        }

        Position toward_head = controller_.get_position();
        while (result.head_to_tail.size() < own_parts.size()) {
            int found = -1;
            for (std::size_t index = 0; index < own_parts.size(); ++index) {
                if (used[index] || own_parts[index]->is_head()) continue;
                StepResult const link = step_from(
                    own_parts[index]->get_position(), own_parts[index]->get_dir());
                if (link.passable && link.destination_known &&
                    link.destination == toward_head) {
                    if (found != -1) return result;
                    found = static_cast<int>(index);
                }
            }
            if (found == -1) break;
            used[static_cast<std::size_t>(found)] = true;
            toward_head = own_parts[static_cast<std::size_t>(found)]->get_position();
            result.head_to_tail.push_back(toward_head);
        }

        result.complete =
            static_cast<int>(own_parts.size()) == controller_.get_length() &&
            static_cast<int>(result.head_to_tail.size()) == controller_.get_length();
        return result;
    }

    std::optional<Position> confirmed_step(Position position,
                                           Direction direction) const {
        RememberedEdge const& edge =
            memory(position).edges[static_cast<std::size_t>(direction_index(direction))];
        if (!edge.known || edge.type == EdgeType::KELP) return std::nullopt;
        if (edge.type == EdgeType::PORTAL)
            return portal_exit(position, direction, edge.portal_id);
        return position.add_dir(direction);
    }

    bool hinted_occupied(Position position) const {
        CellMemory const& cell = memory(position);
        int const round = game_.get_round_num();
        return cell.teammate_until >= round || cell.enemy_until >= round;
    }

    std::vector<bool> current_occupancy() const {
        std::vector<bool> occupied(cells_.size(), false);
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            if (tile.get_dragon())
                occupied[static_cast<std::size_t>(index(tile.get_position()))] = true;
        }
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Position const position(x, y);
                if (hinted_occupied(position))
                    occupied[static_cast<std::size_t>(index(position))] = true;
            }
        }
        return occupied;
    }

    FloodResult flood_space(Position start, std::vector<bool> const& occupied,
                            int cell_limit,
                            std::optional<Position> released_tail = std::nullopt) const {
        std::vector<bool> visited(cells_.size(), false);
        std::queue<Position> frontier;
        visited[static_cast<std::size_t>(index(start))] = true;
        frontier.push(start);
        FloodResult result;

        while (!frontier.empty() && result.space < cell_limit) {
            Position const current = frontier.front();
            frontier.pop();
            ++result.space;
            if (released_tail && current == *released_tail)
                result.reached_tail = true;

            for (Direction direction : Direction::get_direction_list()) {
                auto const next = confirmed_step(current, direction);
                if (!next) continue;
                std::size_t const next_index =
                    static_cast<std::size_t>(index(*next));
                if (visited[next_index]) continue;
                bool const blocked = occupied[next_index] &&
                    (!released_tail || !(*next == *released_tail));
                if (blocked) continue;
                visited[next_index] = true;
                frontier.push(*next);
            }
        }
        return result;
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

    int visible_segment_count(unswbc::DragonPart const& head) const {
        int count = 0;
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (part && part->get_id() == head.get_id() &&
                part->get_team() == head.get_team())
                ++count;
        }
        return count;
    }

    void count_priority_conflicts(MovePlan& plan) const {
        int const round = game_.get_round_num();
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Position const position(x, y);
                CellMemory const& cell = memory(position);
                if (cell.teammate_until < round ||
                    cell.teammate_length <= plan.projected_length)
                    continue;
                int const distance = wrapped_distance(plan.destination, position);
                if (distance <= 2)
                    plan.priority_conflicts += 3 - distance;
            }
        }
    }

    void count_head_threats(MovePlan& plan,
                            std::vector<bool> const& occupied) const {
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (!part || !part->is_head()) continue;
            if (part->get_team() == controller_.get_team() &&
                part->get_id() == controller_.get_id())
                continue;

            bool can_reach_directly = false;
            bool can_sprint_to_head = false;
            for (Direction direction : Direction::get_direction_list()) {
                StepResult const first = step_from(part->get_position(), direction);
                if (!first.passable || !first.destination_known) continue;
                if (first.destination == plan.destination) {
                    can_reach_directly = true;
                    break;
                }
                std::size_t const first_index =
                    static_cast<std::size_t>(index(first.destination));
                if (occupied[first_index]) continue;
                for (Direction second_direction : Direction::get_direction_list()) {
                    StepResult const second =
                        step_from(first.destination, second_direction);
                    if (second.passable && second.destination_known &&
                        second.destination == plan.destination) {
                        can_sprint_to_head = true;
                        break;
                    }
                }
            }
            if (can_reach_directly) {
                // Higher IDs can hit us later this round. Lower IDs move before
                // our next turn and can hit us at the start of the next round.
                ++plan.head_collision_threats;
            } else if (can_sprint_to_head) {
                ++plan.sprint_head_threats;
            }

            if (part->get_team() == controller_.get_team()) {
                CellMemory const& head_memory = memory(part->get_position());
                int known_length = visible_segment_count(*part);
                if (head_memory.teammate_until >= game_.get_round_num())
                    known_length = std::max(
                        known_length, head_memory.teammate_length);
                if (known_length > plan.projected_length &&
                    wrapped_distance(plan.destination, part->get_position()) <= 2)
                    ++plan.priority_conflicts;
            }
        }
        count_priority_conflicts(plan);
    }

    std::optional<MovePlan> evaluate_path(
        std::vector<Direction> const& steps) const {
        if (steps.empty() ||
            static_cast<int>(steps.size()) > controller_.get_length() - 1)
            return std::nullopt;

        MovePlan plan;
        plan.steps = steps;
        plan.sprint_cost = static_cast<int>(steps.size()) - 1;
        plan.projected_length = controller_.get_length() - plan.sprint_cost;
        Position current = controller_.get_position();
        std::vector<bool> occupied = current_occupancy();
        std::vector<bool> eaten(cells_.size(), false);
        BodyInfo const body_info = own_body_info();
        std::deque<Position> simulated_body(
            body_info.head_to_tail.begin(), body_info.head_to_tail.end());

        for (std::size_t step_index = 0; step_index < steps.size(); ++step_index) {
            if (crosses_portal(current, steps[step_index]))
                ++plan.portals_used;
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
            bool const ate_pearl =
                destination_tile && destination_tile->has_pearl() &&
                !eaten[destination_index];
            if (ate_pearl) {
                ++plan.pearls_eaten;
                eaten[destination_index] = true;
            }

            occupied[destination_index] = true;
            if (body_info.complete) {
                simulated_body.push_front(destination);
                if (!ate_pearl && !simulated_body.empty()) {
                    Position const removed = simulated_body.back();
                    simulated_body.pop_back();
                    occupied[static_cast<std::size_t>(index(removed))] = false;
                }
                if (step_index > 0 && simulated_body.size() > 1) {
                    Position const removed = simulated_body.back();
                    simulated_body.pop_back();
                    occupied[static_cast<std::size_t>(index(removed))] = false;
                }
            }
            current = destination;
        }

        plan.destination = current;
        plan.projected_length =
            controller_.get_length() + plan.pearls_eaten - plan.sprint_cost;
        if (!sprint_is_worthwhile(plan)) return std::nullopt;
        plan.destination_visible = controller_.get_tile(current) != nullptr;
        plan.open_exits = count_open_exits(current, occupied);
        for (Direction direction : Direction::get_direction_list()) {
            StepResult const next = step_from(current, direction);
            if (!next.passable || !next.destination_known) continue;
            std::size_t const next_index =
                static_cast<std::size_t>(index(next.destination));
            if (occupied[next_index]) continue;
            std::vector<bool> future_occupied = occupied;
            future_occupied[next_index] = true;
            if (count_open_exits(next.destination, future_occupied) > 0)
                ++plan.continuation_options;
        }

        int const flood_limit = std::min(
            width_ * height_, std::max(64, plan.projected_length + 16));
        FloodResult const immediate_space =
            flood_space(current, occupied, flood_limit);
        plan.reachable_space = immediate_space.space;
        plan.future_space = immediate_space.space;
        if (body_info.complete && !simulated_body.empty()) {
            Position const tail = simulated_body.back();
            FloodResult const with_tail =
                flood_space(current, occupied, flood_limit, tail);
            plan.future_space = with_tail.space;
            plan.tail_reachable = with_tail.reached_tail;
        }

        int const safety_buffer = plan.projected_length >= 10 ? 6
                                : plan.projected_length >= 6 ? 4
                                                             : 2;
        int const required_space = std::min(
            width_ * height_, plan.projected_length + safety_buffer);
        plan.immediate_trap = plan.open_exits == 0 ||
                              plan.continuation_options == 0;
        plan.space_trap = plan.future_space < required_space &&
                          !plan.tail_reachable;
        count_head_threats(plan, occupied);
        return plan;
    }

    bool sprint_is_worthwhile(MovePlan const& plan) const {
        if (plan.steps.size() == 1) return true;
        int const net_growth = plan.pearls_eaten - plan.sprint_cost;
        if (small_snake_length_control_active())
            return net_growth <= 0;
        if (late_growth_phase())
            return net_growth >= 1;
        if (stage_ == GameStage::ENDGAME) return false;
        if (stage_ == GameStage::CONSOLIDATION) return net_growth >= 2;
        return net_growth >= 1;
    }

    void enumerate_sequences(int target_length,
                             std::vector<Direction>& sequence,
                             std::vector<MovePlan>& plans) const {
        if (static_cast<int>(sequence.size()) == target_length) {
            auto plan = evaluate_path(sequence);
            if (plan)
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
        if (!late_growth_phase() && stage_ == GameStage::ENDGAME &&
            controller_.get_length() < emergency_split_length())
            max_steps = 1;
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
        int const pearl_percent = is_late_grower()
            ? 250 + std::min(controller_.get_length(), 50) * 3
            : controller_.get_length() >= 4 ? 10
            : controller_.get_length() == 3 ? 40
                                            : 100;

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
                    if (!late_growth_phase()) value = value * 2 + 400;
                    if (stage_ == GameStage::CONSOLIDATION) value /= 3;
                    if (stage_ == GameStage::ENDGAME) value /= 8;
                    best_exploration = std::max(best_exploration, value);
                } else {
                    int value = std::min(300, round - cell.last_seen_round) -
                                distance * 12;
                    if (stage_ == GameStage::OPENING) value *= 2;
                    if (!late_growth_phase()) value *= 2;
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

        if (best_pearl > 0)
            return best_pearl * pearl_percent / 100 +
                   best_exploration / (late_growth_phase() ? 10 : 2);
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

    int risk_percent() const {
        int const length = controller_.get_length();
        if (length <= 3) return 70;
        if (length <= 5) return 85;
        if (length <= 9) return 110;
        if (length <= 15) return 140;
        return 175;
    }

    bool is_active_attacker() const {
        if (late_growth_phase()) return !is_late_grower();
        // Early attackers first grow into viable split-capable racers.
        return role_ == BotRole::ATTACKER && controller_.get_length() >= 4;
    }

    bool prioritizes_survival() const {
        return is_late_grower() || role_ == BotRole::SURVIVOR;
    }

    int nearest_enemy_distance(Position position) const {
        int best = UNREACHABLE;
        int const round = game_.get_round_num();

        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (part && part->is_head() &&
                part->get_team() != controller_.get_team()) {
                best = std::min(
                    best, wrapped_distance(position, part->get_position()));
            }
        }

        // Enemy sonar/status memory lasts only briefly, so it is useful for
        // pursuit without making an attacker chase a stale location forever.
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Position const remembered(x, y);
                if (memory(remembered).enemy_until >= round)
                    best = std::min(
                        best, wrapped_distance(position, remembered));
            }
        }
        return best;
    }

    int nearest_teammate_distance(Position position) const {
        int best = UNREACHABLE;
        int const round = game_.get_round_num();
        for (unswbc::Tile const& tile : controller_.get_tiles()) {
            unswbc::DragonPart const* part = tile.get_dragon();
            if (!part || !part->is_head() ||
                part->get_team() != controller_.get_team() ||
                part->get_id() == controller_.get_id())
                continue;
            best = std::min(
                best, wrapped_distance(position, part->get_position()));
        }
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                Position const remembered(x, y);
                if (memory(remembered).teammate_until >= round)
                    best = std::min(
                        best, wrapped_distance(position, remembered));
            }
        }
        return best;
    }

    int swarm_dispersion_score(Position position) const {
        if (late_growth_phase()) return 0;
        int const distance = nearest_teammate_distance(position);
        if (distance == UNREACHABLE) return 0;
        if (distance <= 2) return -120000;
        if (distance <= 6) return -(7 - distance) * 18000;
        return std::min(distance, 16) * 1200;
    }

    int role_position_score(Position position) const {
        // Late growers ignore combat objectives; the other 50% become small
        // saboteurs that pressure enemy routes without taking proven deaths.
        if (is_late_grower()) return 0;
        int const distance = nearest_enemy_distance(position);
        if (distance == UNREACHABLE) return 0;

        if (is_active_attacker()) {
            // Attackers pressure from two or three tiles away, where they can
            // cut off routes without volunteering for a head-on collision.
            if (distance <= 1) return -60000;
            if (distance == 2) return 100000;
            if (distance == 3) return 45000;
            return std::max(0, 12 - distance) * 2200;
        }

        // Survivors spread away from enemy heads and preserve independent
        // racers. Proven collision/trap penalties below remain much stronger.
        if (distance <= 6) return -(7 - distance) * 12000;
        return 0;
    }

    void score_plan(MovePlan& plan, bool has_fully_observed_move,
                    bool has_survivable_move) {
        int value = 0;
        int const risk = risk_percent();
        int pearl_value = controller_.get_length() <= 4 ? 45000
                        : controller_.get_length() <= 9 ? 30000
                                                       : 18000;
        int exit_value = controller_.get_length() <= 4 ? 55 : 90;
        if (stage_ == GameStage::CONSOLIDATION) exit_value = 85;
        if (stage_ == GameStage::ENDGAME) {
            pearl_value = controller_.get_length() < 6 ? 30000 : 12000;
            exit_value = 180;
        }
        if (is_late_grower()) {
            // Positive feedback deliberately creates one or more leaders. The
            // existing sonar rule makes short teammates yield to longer ones.
            pearl_value = 45000 +
                std::min(controller_.get_length(), 60) * 1500;
            exit_value = std::max(exit_value, 110);
        } else {
            if (controller_.get_length() == 3)
                pearl_value = std::min(pearl_value, 8000);
            if (controller_.get_length() >= 4)
                pearl_value = 500;
        }
        if (is_active_attacker() && plan.destination_known &&
            nearest_enemy_distance(plan.destination) <= 12)
            pearl_value /= 2;
        if (prioritizes_survival())
            exit_value += 60;
        else if (is_active_attacker())
            exit_value = std::max(45, exit_value - 20);

        value += plan.pearls_eaten * pearl_value;
        value -= plan.sprint_cost * 12000;
        if (small_snake_length_control_active())
            value += plan.sprint_cost * 15000;
        if (!late_growth_phase() && plan.portals_used > 0)
            value += plan.portals_used *
                     (plan.destination_known ? 45000 : 15000);
        if (plan.destination_known) {
            value += cached_objective_score(plan.destination);
            value += role_position_score(plan.destination);
            value += swarm_dispersion_score(plan.destination);
            value += plan.open_exits * exit_value;
            value += plan.continuation_options * (exit_value * 2);
            value += std::min(plan.future_space, 64) *
                     (prioritizes_survival()
                          ? (controller_.get_length() >= 10 ? 130 : 70)
                          : (controller_.get_length() >= 10 ? 75 : 40));
            if (plan.tail_reachable)
                value += prioritizes_survival() ? 9000 : 4000;
            if (memory(plan.destination).danger_until >= game_.get_round_num())
                value -= 18000 * risk / 100;
        } else {
            value -= has_fully_observed_move ? 100000 * risk / 100
                                             : 5000 * risk / 100;
        }

        if (!plan.destination_visible)
            value -= has_fully_observed_move ? 50000 * risk / 100
                                             : 1500 * risk / 100;

        // Food must never buy permission to enter a known shallow trap. Small
        // snakes accept more uncertainty, but not a provable dead end when a
        // survivable alternative exists.
        if (plan.immediate_trap)
            value -= has_survivable_move ? 1500000 : 250000 * risk / 100;
        if (plan.space_trap)
            value -= has_survivable_move ? 900000 : 120000 * risk / 100;

        // Every other head that can step onto our final tile acts before our
        // next decision: either later this round or earlier next round.
        value -= plan.head_collision_threats *
                 (prioritizes_survival() ? 1500000 : 1200000);
        value -= plan.sprint_head_threats *
                 ((prioritizes_survival() ? 70000 : 45000) * risk / 100);
        value -= plan.priority_conflicts *
                 (late_growth_phase() && !is_late_grower()
                      ? 320000
                      : prioritizes_survival() ? 240000 : 180000);

        Direction const first = plan.steps.front();
        if (first == controller_.get_dir())
            value += 18;
        else if (first == controller_.get_dir().get_opposite())
            value -= 1200;
        else
            value += 8;

        if (!late_growth_phase() && stage_ == GameStage::ENDGAME &&
            controller_.get_length() < emergency_split_length())
            value -= plan.sprint_cost * 100000;
        value += tie_break(plan);
        plan.score = value;
    }

    int desired_team_size() const {
        // The reference replay maintains roughly 62-64 tiny racers at the
        // phase boundary. Fill every engine-approved slot through round 300.
        return game_.get_unit_limit();
    }

    int observed_safe_single_moves() const {
        int safe = 0;
        for (Direction direction : Direction::get_direction_list()) {
            std::vector<Direction> const path{direction};
            auto const plan = evaluate_path(path);
            if (plan && plan->destination_known && plan->destination_visible &&
                !plan->immediate_trap && !plan->space_trap &&
                plan->head_collision_threats == 0)
                ++safe;
        }
        return safe;
    }

    int preferred_split_length() const {
        return is_late_grower() ? std::numeric_limits<int>::max() / 4 : 4;
    }

    int emergency_split_length() const {
        // An unseen tail makes child-space validation impossible. Do not let
        // that uncertainty turn into the much larger, observed risk of a very
        // long dragon self-colliding, especially late in the race.
        return is_late_grower() ? std::numeric_limits<int>::max() / 4 : 6;
    }

    int planned_split_size() const {
        constexpr int MIN_SPLIT_AGE = 2;
        constexpr int SPLIT_COOLDOWN = 4;
        constexpr int MIN_LEGAL_LENGTH = 2;

        if (is_late_grower() || turns_alive_ < MIN_SPLIT_AGE ||
            game_.get_round_num() - last_split_round_ < SPLIT_COOLDOWN ||
            controller_.get_unit_count() >= desired_team_size())
            return 0;

        int const length = controller_.get_length();
        bool const emergency = length >= emergency_split_length();
        if (length < preferred_split_length() ||
            length < 2 * MIN_LEGAL_LENGTH)
            return 0;

        // BattleDragon's engine rejects length-1 children, so the requested
        // 1..5 range is implemented as the full legal range 2..5. The mixed
        // ID/round seed makes sizes varied while keeping replays reproducible.
        int const maximum_child =
            std::min(5, length - MIN_LEGAL_LENGTH);
        if (maximum_child < MIN_LEGAL_LENGTH) return 0;
        std::uint32_t random =
            static_cast<std::uint32_t>(controller_.get_id() + 1) * 0x9E3779B9U;
        random ^= static_cast<std::uint32_t>(game_.get_round_num()) *
                  0x85EBCA6BU;
        random ^= static_cast<std::uint32_t>(length) * 0xC2B2AE35U;
        random ^= random >> 16U;
        int const child_size = MIN_LEGAL_LENGTH +
            static_cast<int>(random % static_cast<std::uint32_t>(
                maximum_child - MIN_LEGAL_LENGTH + 1));
        if (!controller_.can_split(child_size)) return 0;

        // The parent must have multiple independently safe continuations. The
        // child acts later in this round and will evaluate its own fresh 7x7
        // vision before moving.
        if (observed_safe_single_moves() < 2) return 0;

        BodyInfo const body = own_body_info();
        if (body.complete && !body.head_to_tail.empty()) {
            std::vector<bool> const occupied = current_occupancy();
            Position const child_head = body.head_to_tail.back();
            if (count_open_exits(child_head, occupied) < 2 && !emergency)
                return 0;
        } else if (!emergency) {
            // Prefer a fully checked tail split while the dragon is compact.
            // Once the hard length ceiling is reached, preserving multiple
            // racers is safer than retaining one unmanageably long body.
            return 0;
        }

        return child_size;
    }

    std::uint32_t encode_signal(SignalType type, Position position,
                                int payload) const {
        std::uint32_t value = SIGNAL_MAGIC << 28U;
        if (controller_.get_team().value == unswbc::Team::B)
            value |= 1U << 27U;
        value |= (static_cast<std::uint32_t>(type) & 0x3U) << 25U;
        value |= (static_cast<std::uint32_t>(position.x) & 0x3FU) << 19U;
        value |= (static_cast<std::uint32_t>(position.y) & 0x3FU) << 13U;
        value |= (static_cast<std::uint32_t>(game_.get_round_num()) & 0x7FU)
                 << 6U;
        value |= static_cast<std::uint32_t>(std::clamp(payload, 0, 63));
        return value;
    }

    void receive_sonar() {
        int const current_mod = game_.get_round_num() & 0x7F;
        for (std::uint32_t message : controller_.get_sonar_messages()) {
            if ((message >> 28U) != SIGNAL_MAGIC) continue;
            bool const sender_is_b = ((message >> 27U) & 0x1U) != 0;
            bool const same_team = sender_is_b ==
                (controller_.get_team().value == unswbc::Team::B);
            auto const type = static_cast<SignalType>((message >> 25U) & 0x3U);
            Position const position(
                static_cast<int>((message >> 19U) & 0x3FU),
                static_cast<int>((message >> 13U) & 0x3FU));
            if (position.x >= width_ || position.y >= height_) continue;
            int const sent_mod = static_cast<int>((message >> 6U) & 0x7FU);
            int const payload = static_cast<int>(message & 0x3FU);
            int const age = (current_mod - sent_mod + 128) % 128;
            if (age > 40) continue;

            if (same_team && type == SignalType::STATUS && age <= 2) {
                memory(position).teammate_length = payload;
                memory(position).teammate_until = game_.get_round_num() + 1;
            } else if (same_team && type == SignalType::PEARL) {
                memory(position).sonar_pearl_until =
                    std::max(memory(position).sonar_pearl_until,
                             game_.get_round_num() + std::max(2, 24 - age));
            } else if (same_team && type == SignalType::DANGER) {
                memory(position).danger_until =
                    std::max(memory(position).danger_until,
                             game_.get_round_num() + std::max(2, 12 - age));
                memory(position).enemy_length = payload;
                memory(position).enemy_until = game_.get_round_num() + 1;
            } else if (!same_team && type == SignalType::STATUS && age <= 2) {
                // A Codehammer opponent reveals itself honestly in self-matches.
                // Treat that as danger, never as friendly right-of-way data.
                memory(position).danger_until = game_.get_round_num() + 2;
                memory(position).enemy_length = payload;
                memory(position).enemy_until = game_.get_round_num() + 1;
            }
        }
    }

    int wrapped_distance(Position left, Position right) const {
        int const dx = std::abs(left.x - right.x);
        int const dy = std::abs(left.y - right.y);
        return std::min(dx, width_ - dx) + std::min(dy, height_ - dy);
    }

    void send_sonar(Position announced_position, int announced_length) {
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
            if (tile.has_pearl() && !(tile.get_position() == announced_position)) {
                int const distance = wrapped_distance(here, tile.get_position());
                if (distance < pearl_distance) {
                    pearl_distance = distance;
                    closest_pearl = &tile;
                }
            }
        }

        int const phase =
            (game_.get_round_num() + controller_.get_id()) & 3;
        if (phase == 0 && closest_enemy && enemy_distance <= 4) {
            controller_.send_sonar(encode_signal(
                SignalType::DANGER, closest_enemy->get_position(),
                visible_segment_count(*closest_enemy)));
        } else if (phase == 1 && closest_pearl) {
            controller_.send_sonar(encode_signal(
                SignalType::PEARL, closest_pearl->get_position(),
                std::max(0, closest_pearl->get_pearl_time())));
        } else {
            controller_.send_sonar(encode_signal(
                SignalType::STATUS, announced_position, announced_length));
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
