#include "../../bots/alan/alan-v2.1/helper.hpp"

#include <cassert>
#include <iostream>

#define private public
#define main alan_v2_1_program_main
#include "../../bots/alan/alan-v2.1/main.cpp"
#undef main
#undef private

int main()
{
    unswbc::Game game(32, 32, 64);
    unswbc::Controller controller(100, unswbc::Team::A, Direction::EAST, {}, 64);
    unswbc::game = &game;
    unswbc::ct = &controller;
    controller.head.position = Position(8, 8);
    controller.length = 4;
    controller.unit_count = 20;
    ReplayDerivedBot bot(controller, game);

    bot.king_ = ReplayDerivedBot::KingInfo{101, 20, Position(14, 8), 380};
    bot.king_head_estimate_ = Position(14, 8);
    game.round_num = 379;
    assert(!bot.sacrifice_started());
    game.round_num = 380;
    assert(bot.sacrifice_started());
    assert(bot.king_fresh());
    controller.unit_count = Config::SMALL_TEAM_UNITS;
    assert(bot.small_team_mode());
    assert(bot.king_fresh());
    controller.unit_count = 20;

    bot.king_head_estimate_ = Position(16, 8);
    assert(!bot.sacrifice_started());
    bot.king_.id = controller.get_id();
    bot.king_head_estimate_ = Position(8, 8);
    assert(!bot.sacrifice_started());

    bot.king_ = {};
    bot.king_head_estimate_.reset();
    game.round_num = Config::SACRIFICE_HARD_ROUND;
    assert(bot.sacrifice_started());
    std::cout << "alan-v2.1 arrival-triggered sacrifice fixtures passed\n";
}
