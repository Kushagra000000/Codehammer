#include "../../bots/alan/alan-v1.1/helper.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#define private public
#define main alan_v1_program_main
#include "../../bots/alan/alan-v1.1/main.cpp"
#undef main
#undef private

struct Fixture
{
    unswbc::Game game;
    unswbc::Controller controller;
    ReplayDerivedBot bot;

    Fixture(int width = 32, int height = 32, int id = 100)
        : game(width, height, 64),
          controller(id, unswbc::Team::A, Direction::EAST, {}, 64),
          bot(controller, game)
    {
        unswbc::game = &game;
        unswbc::ct = &controller;
        controller.head.position = Position(width / 4, height / 2);
        controller.length = 3;
        controller.unit_count = 20;
    }
};

static void adaptive_phases()
{
    Fixture small(16, 16);
    assert(small.bot.target_swarm_size() == 18);
    assert(small.bot.specialise_round() == 200);
    assert(small.bot.gather_round() == 405);
    assert(small.bot.sacrifice_round() == 425);

    Fixture large(64, 64);
    assert(large.bot.target_swarm_size() == 62);
    assert(large.bot.specialise_round() == 280);
    assert(large.bot.gather_round() == 371);
    assert(large.bot.sacrifice_round() == 403);
}

static void opening_distribution_guards()
{
    Fixture f;
    f.game.round_num = 0;
    f.bot.retaining_now_ = false;
    std::fill(f.bot.objective_dist_.begin(), f.bot.objective_dist_.end(), INF);
    Position const center(16, 16);
    Position const far(0, 16);
    assert(f.bot.opening_center_penalty(center) == 0);
    assert(f.bot.opening_center_penalty(far) > 0);

    f.bot.objective_dist_[static_cast<std::size_t>(f.bot.index(f.controller.get_position()))] =
        Config::OPENING_FOOD_PRIORITY_RADIUS;
    assert(f.bot.opening_center_penalty(far) == 0);
    f.bot.objective_dist_[static_cast<std::size_t>(f.bot.index(f.controller.get_position()))] = INF;

    for (int i = 0; i < Config::OPENING_CENTER_CROWD_LIMIT; ++i)
    {
        SeenHead h;
        h.id = 200 + i;
        h.team = unswbc::Team::A;
        h.position = center;
        f.bot.heads_.push_back(h);
    }
    assert(f.bot.opening_center_penalty(far) == 0);
    f.bot.heads_.clear();

    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
        {
            auto &m = f.bot.cell(Position(center.x + dx, center.y + dy));
            m.seen = true;
            m.pearl_time = -1;
        }
    assert(f.bot.opening_center_penalty(far) == 0);
    f.game.round_num = f.bot.opening_center_end_round();
    assert(f.bot.opening_center_penalty(far) == 0);
}

static void backup_retainer()
{
    Fixture f;
    f.game.round_num = f.bot.sacrifice_round();
    f.controller.length = 22;
    f.bot.king_ = ReplayDerivedBot::KingInfo{1, 30, Position(10, 10), f.game.round_num};
    assert(f.bot.compute_retaining());
    f.controller.length = 20;
    assert(!f.bot.compute_retaining());
}

static void packet_round_trips_and_legacy_rejection()
{
    Fixture sender;
    sender.game.round_num = 100;
    sender.controller.length = 14;
    sender.bot.retaining_now_ = true;
    Position const predicted(9, 10);
    std::uint64_t const status = sender.bot.status_message(predicted, 15);

    unswbc::Controller receiver_controller(101, unswbc::Team::A, Direction::NORTH, {}, 64);
    ReplayDerivedBot receiver(receiver_controller, sender.game);
    unswbc::ct = &receiver_controller;
    receiver_controller.sonar_messages = {status};
    receiver.receive_status();
    assert(receiver.signals_.size() == 1);
    assert(receiver.signals_[0].id == sender.controller.get_id());
    assert(receiver.signals_[0].position == predicted);
    assert(receiver.signals_[0].length == 15);

    sender.bot.territory_ = ReplayDerivedBot::Claim{sender.controller.get_id(), 14, 100, 2, 3, 8, 6};
    sender.bot.territory_variant_ = 2;
    sender.bot.patrol_dir_ = -1;
    std::uint64_t const claim = sender.bot.claim_message();
    receiver_controller.sonar_messages = {claim};
    receiver.receive_status();
    assert(receiver.claims_.contains(sender.controller.get_id()));
    ReplayDerivedBot::Claim const &decoded = receiver.claims_.at(sender.controller.get_id());
    assert(decoded.x == 2 && decoded.y == 3 && decoded.w == 8 && decoded.h == 6);

    std::uint64_t const legacy = (0xC8ULL << 56U) | (100ULL << 41U) |
                                 (14ULL << 19U) | (2ULL << 29U) | (3ULL << 35U) |
                                 (999ULL << 7U);
    std::size_t const before = receiver.claims_.size();
    receiver_controller.sonar_messages = {legacy};
    receiver.receive_status();
    assert(receiver.claims_.size() == before);

    int const edge_index = (sender.game.round_num + sender.controller.get_id()) & 3;
    auto &edge = sender.bot.cell(sender.controller.get_position()).edges[static_cast<std::size_t>(edge_index)];
    edge = {true, EdgeType::KELP, -1};
    std::uint64_t const terrain = sender.bot.terrain_message();
    receiver_controller.sonar_messages = {terrain};
    receiver.receive_status();
    assert(receiver.cell(sender.controller.get_position()).edges[static_cast<std::size_t>(edge_index)].known);
    assert(receiver.cell(sender.controller.get_position()).edges[static_cast<std::size_t>(edge_index)].type == EdgeType::KELP);

    edge = {};
    receiver.cell(sender.controller.get_position()).edges[static_cast<std::size_t>(edge_index)] = {};
    receiver_controller.sonar_messages = {sender.bot.terrain_message()};
    receiver.receive_status();
    assert(!receiver.cell(sender.controller.get_position()).edges[static_cast<std::size_t>(edge_index)].known);
}

static void feeding_requires_collectible_body()
{
    Fixture f;
    f.game.round_num = f.bot.sacrifice_round();
    f.controller.length = 4;
    f.bot.known_body_ = 4;
    f.bot.own_trail_ = {Position(8, 8), Position(7, 8), Position(6, 8), Position(5, 8)};
    f.bot.king_ = ReplayDerivedBot::KingInfo{1, 20, Position(10, 8), f.game.round_num};
    f.bot.king_head_estimate_ = Position(10, 8);
    f.bot.feeder_target_ = TeamSignal{1, Position(10, 8), 20, f.game.round_num, true};
    SeenHead king;
    king.id = 1;
    king.team = unswbc::Team::A;
    king.position = Position(10, 8);
    f.bot.heads_.push_back(king);
    assert(f.bot.feed_body_collectible());
    f.bot.own_trail_[2] = Position(20, 20);
    assert(!f.bot.feed_body_collectible());
}

static void confirmed_length_trades()
{
    Fixture f;
    f.game.round_num = 100;
    f.controller.length = 4;
    f.controller.unit_count = 20;
    SeenHead uncertain;
    uncertain.id = 400;
    uncertain.team = unswbc::Team::B;
    uncertain.lower_length = 4;
    uncertain.visible_length = 4;
    uncertain.truncated = true; // old estimated-length rule treated this as six
    assert(!f.bot.eligible_target(uncertain));
    uncertain.lower_length = 6;
    assert(f.bot.eligible_target(uncertain));
}

static void survival_first_candidate_selection()
{
    Fixture f;

    Candidate trade;
    trade.head_trade = true;
    trade.score = 100000;

    Candidate safe;
    safe.destination_known = true;
    safe.safe = true;
    safe.critical_ok = true;
    safe.exits = 2;
    safe.score = -1000;
    std::vector<Candidate> choices{trade, safe};
    assert(f.bot.choose_candidate(choices) == &choices[1]);

    Candidate threatened;
    threatened.destination_known = true;
    threatened.exits = 1;
    threatened.enemy_threats = 2;
    threatened.hard_threats = 1;
    threatened.score = -5000;
    choices = {trade, threatened};
    assert(f.bot.choose_candidate(choices) == &choices[1]);

    Candidate dead_end;
    dead_end.destination_known = true;
    dead_end.exits = 0;
    dead_end.score = 5000;
    Candidate unknown_portal;
    unknown_portal.destination_known = false;
    unknown_portal.unknown_portal = true;
    unknown_portal.score = -5000;
    choices = {dead_end, unknown_portal};
    assert(f.bot.choose_candidate(choices) == &choices[1]);
}

static void forced_head_trade_is_last_resort()
{
    int constexpr width = 8, height = 8;
    unswbc::Game game(width, height, 64);
    std::vector<unswbc::Tile> tiles;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            std::optional<unswbc::DragonPart> part;
            if (x == 4 && y == 4)
                part.emplace(Position(x, y), 600, unswbc::Team::A,
                             Direction::EAST, true);
            else if (x == 5 && y == 4)
                part.emplace(Position(x, y), 700, unswbc::Team::B,
                             Direction::WEST, true);
            tiles.emplace_back(Position(x, y), part, -1, false);
        }
    unswbc::Controller controller(600, unswbc::Team::A, Direction::EAST,
                                  unswbc::Vision(tiles), 64);
    unswbc::game = &game;
    unswbc::ct = &controller;
    controller.head.position = Position(4, 4);
    controller.length = 3;
    controller.unit_count = 1;
    game.round_num = 0; // normal aggression is disabled

    ReplayDerivedBot bot(controller, game);
    bot.remember_vision();
    bot.observe_heads();
    Candidate const normal = bot.evaluate_single(Direction::EAST);
    assert(normal.score == -INF);
    Candidate const forced = bot.evaluate_single(Direction::EAST, true);
    assert(forced.head_trade);
    assert(forced.forced_trade);
}

static void symmetry_from_static_topology()
{
    int constexpr width = 10, height = 10;
    unswbc::Game game(width, height, 64);
    std::vector<unswbc::Tile> tiles;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            tiles.emplace_back(Position(x, y), std::nullopt, -1, false);
    unswbc::Controller controller(500, unswbc::Team::A, Direction::EAST,
                                  unswbc::Vision(tiles), 64);
    unswbc::game = &game;
    unswbc::ct = &controller;
    controller.head.position = Position(5, 5);
    auto wall = [&](Position p, Direction d)
    {
        controller.get_tile(p)->edges[static_cast<std::size_t>(direction_index(d))].edge_type = EdgeType::KELP;
    };
    wall(Position(1, 2), Direction::EAST);
    wall(Position(2, 2), Direction::WEST);
    wall(Position(8, 7), Direction::WEST);
    wall(Position(7, 7), Direction::EAST);
    ReplayDerivedBot bot(controller, game);
    bot.remember_vision();
    assert(bot.confirmed_symmetry_ == ReplayDerivedBot::SymmetryType::ROTATIONAL);
}

int main()
{
    adaptive_phases();
    opening_distribution_guards();
    backup_retainer();
    packet_round_trips_and_legacy_rejection();
    feeding_requires_collectible_body();
    confirmed_length_trades();
    survival_first_candidate_selection();
    forced_head_trade_is_last_resort();
    symmetry_from_static_topology();
    std::cout << "alan-v1.1 survival-first movement, forced trades and inherited fixtures passed\n";
}

