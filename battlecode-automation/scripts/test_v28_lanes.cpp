// clang++ -std=c++20 -O1 -g -DBOT_UNLIMITED -fsanitize=undefined
//   battlecode-automation/scripts/test_v28_lanes.cpp -o /tmp/test_v28_lanes
#include "../../bots/New/current/bot-v28.0.0-Kushagra/helper.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#define main battlecode_program_main
#include "../../bots/New/current/bot-v28.0.0-Kushagra/main.cpp"
#undef main
#undef private

int failures = 0, checks = 0;
void check(bool ok, char const *name)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

struct LaneFixture
{
    unswbc::Game game;
    unswbc::Controller ct;
    ReplayDerivedBot bot;
    LaneFixture(int id = 4, unswbc::Team team = unswbc::Team::A, int width = 16, int height = 16)
        : game(width, height, 64), ct(id, team, Direction::EAST, {}, 64), bot(ct, game)
    {
        unswbc::game = &game;
        unswbc::ct = &ct;
        game.round_num = 12;
        ct.head.position = {7, 6};
        ct.length = 4;
        ct.unit_count = 8;
        bot.first_round_ = 2;
        bot.birth_length_ = 2;
        bot.known_body_ = 4;
        bot.own_trail_ = {{7, 6}, {6, 6}, {5, 6}, {4, 6}};
        for (int i = 0; i < 4; ++i)
            bot.own_idx_[static_cast<std::size_t>(bot.index(bot.own_trail_[static_cast<std::size_t>(i)]))] = i;
        for (auto &m : bot.memory_)
        {
            m.seen = true;
            m.last_seen = game.round_num;
            m.pearl_time = -1;
            for (auto &e : m.edges)
                e = {true, EdgeType::EMPTY, -1};
        }
        for (int x = 4; x <= 9; ++x)
        {
            edge({x, 6}, Direction::NORTH, {true, EdgeType::KELP, -1});
            edge({x, 6}, Direction::SOUTH, {true, EdgeType::KELP, -1});
        }
        edge({9, 6}, Direction::EAST, {true, EdgeType::KELP, -1});
        refresh();
    }
    void edge(Position p, Direction d, RememberedEdge e)
    {
        bot.cell(p).edges[static_cast<std::size_t>(direction_index(d))] = e;
        bot.cell(p.add_dir(d)).edges[static_cast<std::size_t>(direction_index(d.get_opposite()))] = e;
        bot.nbr_all_dirty_ = true;
    }
    void refresh()
    {
        bot.start_turn_clock(ReplayDerivedBot::clock_now());
        bot.refresh_neighbours();
        bot.prepare_lanes();
    }
    void observed_enemy(std::vector<Position> const &enemy_body, int enemy_id)
    {
        // Build actual vision and run the same body/head/threat preparation as a
        // live turn; do not inject threat-array values for these integration tests.
        std::vector<unswbc::Tile> tiles;
        Position const head = ct.get_position();
        auto const own = bot.own_trail_;
        auto const enemy_team = ct.get_team() == unswbc::Team::A ? unswbc::Team::B : unswbc::Team::A;
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
            {
                Position const p(bot.wrap(head.x + dx, game.width), bot.wrap(head.y + dy, game.height));
                tiles.emplace_back(p, std::nullopt, -1, false);
                auto &tile = tiles.back();
                for (Direction d : Direction::get_direction_list())
                {
                    auto const &e = bot.cell(p).edges[static_cast<std::size_t>(direction_index(d))];
                    tile.get_edge(d).edge_type = e.type;
                    tile.get_edge(d).portal_id = e.portal_id;
                }
                for (std::size_t i = 0; i < own.size(); ++i)
                    if (own[i] == p)
                    {
                        Direction dir = ct.get_dir();
                        if (i > 0)
                            for (Direction d : Direction::get_direction_list())
                                if (p.add_dir(d) == own[i - 1]) dir = d;
                        tile.dragon_part = unswbc::DragonPart(p, ct.get_id(), ct.get_team(), dir, i == 0);
                    }
                for (std::size_t i = 0; i < enemy_body.size(); ++i)
                    if (enemy_body[i] == p)
                    {
                        Direction dir = Direction::SOUTH;
                        if (i > 0)
                            for (Direction d : Direction::get_direction_list())
                                if (p.add_dir(d) == enemy_body[i - 1]) dir = d;
                        tile.dragon_part = unswbc::DragonPart(p, enemy_id, enemy_team, dir, i == 0);
                    }
            }
        ct.vision = unswbc::Vision(std::move(tiles));
        bot.start_turn_clock(ReplayDerivedBot::clock_now());
        bot.remember_vision();
        bot.refresh_neighbours();
        bot.update_body_model();
        bot.observe_heads();
        bot.remember_bodies();
        bot.compute_roles();
        bot.build_threat_maps();
        bot.prepare_survival();
        bot.prepare_lanes();
    }
    std::pair<bool, std::string> escape(std::vector<Candidate> const &candidates = {})
    {
        std::ostringstream output;
        auto *old = std::cout.rdbuf(output.rdbuf());
        bool const result = bot.play_lane_harvest_escape(candidates);
        std::cout.rdbuf(old);
        return {result, output.str()};
    }
};

int main()
{
    if (!ReplayDerivedBot::LANES_ENABLED)
    {
        LaneFixture f;
        check(!f.bot.lane_inward_ && !f.bot.lane_blocks_routine_split() && !f.escape().first,
              "V28_NO_LANES leaves all three hooks inactive");
        return failures != 0;
    }
    for (auto const team : {unswbc::Team::A, unswbc::Team::B})
    {
        LaneFixture f(team == unswbc::Team::A ? 4 : 5, team);
        check(f.bot.lane_inward_ && f.bot.lane_depth_[f.bot.index({7, 6})] > 0,
              "known inward lane is recognized for either team");
        f.bot.cell({8, 6}).pearl = true;
        f.refresh();
        check(f.bot.lane_food_ahead_ == 1 && f.bot.lane_blocks_routine_split(),
              "visible food ahead defers routine splitting");
        check(!f.escape().first, "productive lane does not reverse early");
        f.bot.cell({8, 6}).pearl = false;
        f.refresh();
        check(f.bot.lane_food_ahead_ == 0 && !f.bot.lane_blocks_routine_split(),
              "consuming the last pearl releases split deferral");
        auto const [acted, output] = f.escape();
        check(acted && output.find("SPLIT 2") != std::string::npos && output.find("lane_depleted") != std::string::npos,
              "depleted lane executes a legal reverse split with the existing handover");
        check(f.bot.remnant_after_escape_ && f.bot.escape_keep_ == 2,
              "lane escape retains the established remnant lifecycle");
    }
    {
        LaneFixture f;
        f.bot.cell({8, 6}).pearl_time = 2;
        f.refresh();
        check(f.bot.lane_blocks_routine_split() && !f.escape().first, "imminent spawn preserves harvesting");
        f.bot.cell({8, 6}).pearl_time = 200;
        f.refresh();
        check(f.bot.lane_food_ahead_ == 0 && f.escape().first, "distant future spawn does not cause lane camping");
    }
    {
        LaneFixture f;
        f.edge({9, 6}, Direction::EAST, {});
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "unknown edge is an opening, not a dead end");
    }
    {
        LaneFixture f;
        f.edge({9, 6}, Direction::EAST, {true, EdgeType::PORTAL, 19});
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "portal boundary is never peeled into a dead end");
    }
    {
        LaneFixture f;
        f.edge({4, 6}, Direction::WEST, {true, EdgeType::KELP, -1});
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "sealed tree without an exit is not a harvest lane");
    }
    {
        LaneFixture f;
        f.bot.cell({9, 6}).last_seen--;
        f.refresh();
        check(f.bot.lane_food_ahead_ == -1 && !f.escape().first, "stale view cannot prove depletion");
    }
    {
        LaneFixture f;
        f.bot.cell({8, 6}).other_seen = true;
        f.refresh();
        check(f.bot.lane_food_ahead_ == -1 && !f.escape().first, "another dragon ahead prevents harvest assumptions");
    }
    {
        LaneFixture f;
        f.ct.unit_count = f.bot.unit_cap() - Config::ESCAPE_UNIT_RESERVE;
        check(!f.escape().first, "proactive lane split preserves emergency unit reserves");
    }
    for (int id : {0, 1})
    {
        LaneFixture f(id, id == 0 ? unswbc::Team::A : unswbc::Team::B);
        check(!f.bot.lane_inward_ && !f.escape().first, "both queens are excluded");
    }
    {
        LaneFixture f;
        f.bot.retaining_now_ = true;
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "retaining dragons keep their existing role");
    }
    {
        LaneFixture f;
        f.bot.first_round_ = f.game.round_num;
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "newborn first turn cannot trigger proactive lane splitting");
    }
#ifndef V28_NO_NEWBORN_HOLD
    for (auto const team : {unswbc::Team::A, unswbc::Team::B})
    {
        LaneFixture f(team == unswbc::Team::A ? 4 : 5, team);
        f.bot.first_round_ = f.game.round_num;
        check(!f.bot.should_split(), "either team's newborn waits before ordinary reproduction");
        ++f.game.round_num;
        check(f.bot.should_split(), "ordinary split eligibility returns on the newborn's second turn");
    }
    for (int id : {0, 1})
    {
        LaneFixture f(id, id == 0 ? unswbc::Team::A : unswbc::Team::B);
        f.ct.length = Config::QUEEN_SEED_KEEP + 2;
        f.bot.first_round_ = f.game.round_num;
        check(f.bot.should_split(), "newborn hold does not suppress queen seeding");
    }
    {
        LaneFixture f(4, unswbc::Team::A, 40, 30);
        f.bot.first_round_ = f.game.round_num;
        check(f.bot.should_split(), "newborn routine hold is restricted to small maps");
    }
#endif
    {
        LaneFixture f;
        f.bot.escape_born_ = true;
        f.bot.first_round_ = f.game.round_num - 1;
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "escape child holding period remains respected");
    }
    {
        LaneFixture f;
        f.game.round_num = Config::GATHER_ROUND;
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "late economy remains unchanged");
    }
    {
        LaneFixture f(4, unswbc::Team::A, 40, 30);
        check(!f.bot.lane_inward_ && !f.escape().first, "large maps bypass the bounded lane analysis");
    }
    {
        LaneFixture f;
        f.bot.known_body_ = 3;
        f.refresh();
        check(!f.bot.lane_inward_ && !f.escape().first, "unknown body cannot be handed to a proactive lane child");
    }
    {
        LaneFixture f;
        f.bot.hard_threat_[f.bot.index({3, 6})] = 1;
        check(!f.escape().first, "threatened outward child exit prevents proactive split");
    }
    {
        LaneFixture f;
        f.bot.other_occ_[f.bot.index({3, 6})] = 1;
        check(!f.escape().first, "blocked tail exit prevents proactive split");
    }
    for (auto const team : {unswbc::Team::A, unswbc::Team::B})
        for (bool const threat_at_tail : {true, false})
        {
            int const own_id = team == unswbc::Team::A ? 4 : 5;
            int const enemy_id = team == unswbc::Team::A ? 9 : 8; // enemy moves before its newborn victim
            LaneFixture f(own_id, team);
            f.ct.head.position = {6, 6};
            f.bot.own_trail_ = {{6, 6}, {5, 6}, {4, 6}, {3, 6}};
            std::fill(f.bot.own_idx_.begin(), f.bot.own_idx_.end(), -1);
            for (int i = 0; i < 4; ++i)
                f.bot.own_idx_[f.bot.index(f.bot.own_trail_[static_cast<std::size_t>(i)])] = i;
            if (!threat_at_tail)
            {
                f.edge({3, 6}, Direction::NORTH, {true, EdgeType::KELP, -1});
                f.edge({3, 6}, Direction::SOUTH, {true, EdgeType::KELP, -1});
            }
            f.observed_enemy({}, enemy_id);
            check(f.escape().first, "clear control permits lane escape for either team and tail layout");
            f.bot.remnant_after_escape_ = false;
            std::vector<Position> enemy_body{{3, 5}, {3, 4}};
            if (!threat_at_tail)
                enemy_body.insert(enemy_body.end(), {{4, 4}, {5, 4}, {6, 4}});
            f.observed_enemy(enemy_body, enemy_id);
            bool const ordinary_maps = !f.bot.valuable_now_ &&
                std::all_of(f.bot.hard_threat_.begin(), f.bot.hard_threat_.end(), [](int n) { return n == 0; }) &&
                std::all_of(f.bot.soft_threat_.begin(), f.bot.soft_threat_.end(), [](int n) { return n == 0; });
            check(ordinary_maps, "live ordinary-worker preparation leaves hard and soft reach maps empty");
            int const tail = f.bot.index({3, 6}), exit = f.bot.index({2, 6});
            check(threat_at_tail ? f.bot.enemy_threats_[tail] > 0
                                 : f.bot.enemy_threats_[tail] == 0 && f.bot.enemy_threats_[exit] > 0,
                  "visible enemy head populates the tail or sole outward-exit free-step threat");
            check(!f.escape().first, "real enemy reach prevents a proactive lane sacrifice for either team");
            std::ostringstream output;
            auto *old = std::cout.rdbuf(output.rdbuf());
            bool const emergency = f.bot.play_split_escape({});
            std::cout.rdbuf(old);
            check(emergency, "certain-death emergency escape remains separate from the proactive lane veto");
        }
    {
        LaneFixture f;
        Candidate c;
        c.safe = true;
        c.destination = {3, 6};
        check(!f.escape({c}).first, "a safe outward move wins over a sacrificial split");
    }
    {
        LaneFixture f;
        Candidate c;
        c.head_trade = true;
        check(!f.escape({c}).first, "lane economy does not preempt an available head trade");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
