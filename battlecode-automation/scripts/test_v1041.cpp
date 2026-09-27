// Focused behavioral fixtures for the v10.4-based v10.4.1 rewrite.
// Run: clang++ -std=c++20 -O2 battlecode-automation/scripts/test_v1041.cpp -o /tmp/test_v1041 && /tmp/test_v1041
// These call the actual bot implementation; they do not replace the engine-match checks.
#ifndef BOT_HELPER
#define BOT_HELPER "../../bots/current/bot-v10.4.1/helper.hpp"
#endif
#include BOT_HELPER
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#define main battlecode_program_main
#ifndef BOT_SOURCE
#define BOT_SOURCE "../../bots/current/bot-v10.4.1/main.cpp"
#endif
#include BOT_SOURCE
#undef main
#undef private

struct Capture {
    std::ostringstream output;
    std::streambuf *old = std::cout.rdbuf(output.rdbuf());
    ~Capture() { std::cout.rdbuf(old); }
};

struct Fixture {
    unswbc::Game game{24, 24, 64};
    unswbc::Controller ct{100, unswbc::Team::A, Direction::EAST, {}, 64};
    ReplayDerivedBot bot{ct, game};
    std::vector<unswbc::Tile> board;

    Fixture() {
        unswbc::game = &game;
        unswbc::ct = &ct;
        game.round_num = 390;
        ct.unit_count = 20;
        for (int y = 0; y < game.height; ++y)
            for (int x = 0; x < game.width; ++x) {
                board.emplace_back(Position(x, y), std::nullopt, -1, false);
                auto &m = bot.cell(Position(x, y));
                m.seen = true;
                m.last_seen = game.round_num;
                m.pearl_time = -1;
                m.pearl_sample_round = game.round_num;
                for (auto &e : m.edges)
                    e = {true, EdgeType::EMPTY, -1};
            }
        bot.first_round_ = 10;
        bot.initialized_ = true;
    }

    unswbc::Tile &tile(Position p) { return board[p.y * game.width + p.x]; }

    static Direction towards(Position from, Position to) {
        for (auto d : Direction::get_direction_list())
            if (from.add_dir(d) == to)
                return d;
        assert(false && "fixture body must be contiguous");
        return Direction::NORTH;
    }

    void visible_window(unswbc::Controller &who) {
        std::vector<unswbc::Tile> tiles;
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx) {
                Position p((who.head.position.x + dx + game.width) % game.width,
                           (who.head.position.y + dy + game.height) % game.height);
                tiles.push_back(tile(p));
            }
        who.vision = unswbc::Vision(std::move(tiles));
    }

    void body(std::deque<Position> const &segments, bool grower) {
        for (auto &t : board)
            t.dragon_part.reset();
        ct.length = static_cast<int>(segments.size());
        ct.head.position = segments.front();
        ct.head.dir = towards(segments[1], segments[0]);
        bot.founder_ = grower;
        bot.heir_ = false;
        if (!grower)
            while (bot.selected_grower())
                ++ct.head.dragon_id;
        for (std::size_t i = 0; i < segments.size(); ++i)
            tile(segments[i]).dragon_part = unswbc::DragonPart(
                segments[i], ct.get_id(), ct.get_team(),
                i ? towards(segments[i], segments[i - 1]) : ct.head.dir, i == 0);
        bot.own_trail_ = segments;
        visible_window(ct);
        bot.remember_vision();
        bot.update_body_model();
        bot.compute_roles();
        assert(bot.body_complete());
        assert(bot.retaining_length() == grower);
    }

    void wall(Position p, Direction d) {
        tile(p).get_edge(d).edge_type = EdgeType::KELP;
        tile(p.add_dir(d)).get_edge(d.get_opposite()).edge_type = EdgeType::KELP;
        bot.cell(p).edges[direction_index(d)] = {true, EdgeType::KELP, -1};
        bot.cell(p.add_dir(d)).edges[direction_index(d.get_opposite())] = {true, EdgeType::KELP, -1};
        visible_window(ct);
    }

    void trap_head() {
        for (auto d : {Direction::NORTH, Direction::EAST, Direction::SOUTH})
            wall(ct.head.position, d);
    }
};

void productive_location(bool measured_rates) {
    Fixture f;
    f.ct.length = 20;
    f.ct.head.position = Position(12, 12);
    f.bot.retaining_now_ = true;
    f.bot.valuable_now_ = true;
    // All rectangles are known clear. Compare both spawn density and measured
    // frequency: the same patch should win when other rows spawn more slowly.
    if (measured_rates)
        for (auto &m : f.bot.memory_) {
            m.pearl_time = 8;
            m.spawn_gap = 40;
        }
    for (int y = 12; y <= 15; ++y)
        for (int x = 0; x < 24; ++x) {
            auto &m = f.bot.cell(Position(x, y));
            m.pearl_time = 8;
            if (measured_rates)
                m.spawn_gap = 8;
            m.pearl_sample_round = f.game.round_num;
        }
    Capture captured;
    f.bot.map_territory();
    auto const &c = f.bot.territory_;
    assert(c.w * c.h >= 80);
    int fertile = 0;
    for (auto p : f.bot.patrol_)
        fertile += measured_rates ? f.bot.cell(p).spawn_gap == 8 : f.bot.cell(p).pearl_time >= 0;
    assert(fertile * 2 > static_cast<int>(f.bot.patrol_.size()));
    assert(f.bot.patrol_.size() == static_cast<std::size_t>(c.w * c.h));
    std::unordered_set<int> unique;
    for (std::size_t i = 0; i < f.bot.patrol_.size(); ++i) {
        assert(unique.insert(f.bot.index(f.bot.patrol_[i])).second);
        assert(f.bot.distance(f.bot.patrol_[i], f.bot.patrol_[(i + 1) % f.bot.patrol_.size()]) == 1);
    }
}

void pearl_detour(int pearl_distance, bool threatened) {
    Fixture f;
    f.bot.territory_ = {f.ct.get_id(), 16, f.game.round_num, 6, 6, 10, 8};
    f.bot.build_patrol();
    std::deque<Position> body;
    for (int i = 0; i < 16; ++i)
        body.push_back(f.bot.patrol_[(4 - i + 80) % 80]);
    f.body(body, true);
    auto old_direction = f.bot.patrol_direction();
    assert(old_direction && *old_direction == Direction::EAST);
    f.bot.build_objective_map();
    f.bot.update_patrol_food();
    assert(!f.bot.patrol_food_direction_);
    assert(f.bot.evaluate_single(Direction::EAST).score > f.bot.evaluate_single(Direction::NORTH).score);
    Position food(10, 6 - pearl_distance);
    f.tile(food).pearl = true;
    f.visible_window(f.ct);
    f.bot.remember_vision();
    f.bot.build_objective_map();
    if (threatened) {
        f.bot.hard_threat_[f.bot.index(Position(10, 5))] = 1;
        f.bot.enemy_threats_[f.bot.index(Position(10, 5))] = 1;
    }
    f.bot.update_patrol_food();
    auto const north = f.bot.evaluate_single(Direction::NORTH);
    auto const east = f.bot.evaluate_single(Direction::EAST);
    assert(east.safe);
    if (threatened) {
        assert(!north.safe);
        assert(east.score > north.score);
    } else {
        assert(north.safe);
        assert(north.score > east.score);
        if (pearl_distance == 1)
            assert(north.new_length == f.ct.length + 1);
    }
    // Before the old 475 threshold: nearby food should already matter.
    assert(f.game.round_num < 475);
}

std::deque<Position> trapped_body(bool grower) {
    if (!grower)
        return {Position(12, 12), Position(11, 12), Position(10, 12), Position(9, 12)};
    return {Position(12, 12), Position(11, 12), Position(10, 12), Position(10, 13),
            Position(10, 14), Position(11, 14), Position(12, 14), Position(13, 14),
            Position(14, 14), Position(14, 13), Position(14, 12), Position(14, 11)};
}

void corner_escape(bool grower, bool offscreen_tail = false, bool incomplete_memory = false) {
    Fixture f;
    auto body = trapped_body(grower);
    if (offscreen_tail) {
        body.clear();
        for (int x = 12; x >= 1; --x)
            body.push_back(Position(x, 12));
        for (int x = 1; x <= 16; ++x)
            body.push_back(Position(x, 13));
        assert(body.size() == 28);
    }
    f.body(body, grower);
    f.trap_head();
    if (offscreen_tail)
        assert(f.ct.get_tile(body.back()) == nullptr);
    if (incomplete_memory) {
        // A restarted process has only the head-visible prefix. The engine
        // still knows the complete body and can legally reverse its tail.
        f.bot.own_trail_.clear();
        f.bot.update_body_model();
        assert(!f.bot.body_complete());
    }
    Capture captured;
    f.bot.play_turn();
    int const child_size = f.ct.length - 2;
    assert(captured.output.str().find("SPLIT " + std::to_string(child_size) + "\n") != std::string::npos);

    // Apply the engine's split orientation and instantiate a fresh child process.
    unswbc::Controller child(1000, f.ct.get_team(), Direction::NORTH, {}, 64);
    child.unit_count = f.ct.unit_count + 1;
    child.length = child_size;
    child.head.position = body.back();
    child.head.dir = Fixture::towards(body[body.size() - 2], body.back());
    std::vector<Position> reversed;
    for (int i = 0; i < child_size; ++i) {
        Position p = body[body.size() - 1 - i];
        reversed.push_back(p);
        f.tile(p).dragon_part = unswbc::DragonPart(
            p, child.get_id(), child.get_team(),
            i ? Fixture::towards(p, reversed[i - 1]) : child.head.dir, i == 0);
    }
    f.visible_window(child);
    unswbc::ct = &child;
    ReplayDerivedBot child_bot(child, f.game);
    captured.output.str("");
    captured.output.clear();
    child_bot.play_turn();
    std::istringstream lines(captured.output.str());
    std::string line, movement;
    while (std::getline(lines, line))
        if (line.starts_with("MOVE "))
            movement = line.substr(5);
    assert(!movement.empty());
    Position p = child.head.position;
    for (char ch : movement) {
        Direction direction(ch);
        assert(f.tile(p).get_edge(direction).edge_type == EdgeType::EMPTY);
        p = p.add_dir(direction);
        assert(!f.tile(p).dragon_part);
    }
    assert(!(p == child.head.position));
}

void escape_guards() {
    Fixture f;
    f.body(trapped_body(false), false);
    f.trap_head();
    Capture captured;
    Candidate viable;
    viable.destination_known = true;
    viable.destination = Position(12, 11);
    viable.exits = 2;
    viable.space = 30;
    viable.safe = viable.critical_ok = true;
    assert(!f.bot.play_grower_escape({viable}));
    assert(captured.output.str().empty());
    // A pessimistic space estimate is not certain death: preserve the body
    // whenever the legal move still has an exit and no hard threat.
    viable.exits = 1;
    viable.space = 2;
    viable.safe = viable.critical_ok = false;
    assert(!f.bot.play_grower_escape({viable}));
    assert(captured.output.str().empty());
    // A move into a currently closed pocket is still legal this turn. Another
    // dragon can open it before the next turn; do not fragment pre-emptively.
    viable.exits = 0;
    assert(!f.bot.play_grower_escape({viable}));
    assert(captured.output.str().empty());
    f.ct.unit_count = 64;
    assert(!f.bot.play_grower_escape({}));
    f.ct.unit_count = 20;
    f.ct.length = 3;
    assert(!f.bot.play_grower_escape({}));
    f.ct.length = 4;
    for (auto d : Direction::get_direction_list())
        f.wall(f.bot.own_trail_.back(), d);
    assert(!f.bot.play_grower_escape({}));
}

void spawn_observation() {
    Fixture f;
    f.ct.head.position = Position(12, 12);
    Position p(12, 11);
    f.tile(p).pearl_time = 1;
    f.visible_window(f.ct);
    f.bot.remember_vision();
    assert(f.bot.cell(p).spawn_gap == 0);
    ++f.game.round_num;
    f.tile(p).pearl_time = 8;
    f.tile(p).pearl = true;
    f.visible_window(f.ct);
    f.bot.remember_vision();
    assert(f.bot.cell(p).spawn_gap == 8);
    // Normal countdown is not another spawn-rate observation.
    ++f.game.round_num;
    f.tile(p).pearl_time = 7;
    f.visible_window(f.ct);
    f.bot.remember_vision();
    assert(f.bot.cell(p).spawn_gap == 8);
}

int main() {
    productive_location(false);
    productive_location(true);
    spawn_observation();
    pearl_detour(1, false);
    pearl_detour(2, false);
    pearl_detour(1, true);
    corner_escape(false);
    corner_escape(true);
    corner_escape(true, true);
    corner_escape(true, true, true);
    escape_guards();
    std::cout << "v10.4.1: productive cycle, nearby pearl detours, threat priority, "
                 "worker/grower tail-child escape and split legality fixtures passed\n";
}
