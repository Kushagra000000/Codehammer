#include "../../bots/alan/alan-v2.4/helper.hpp"

#include <cassert>
#include <iostream>

#define private public
#define main alan_v2_4_program_main
#include "../../bots/alan/alan-v2.4/main.cpp"
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
    game.round_num = Config::GATHER_ROUND;
    ReplayDerivedBot bot(controller, game);

    bot.known_body_ = 4;
    bot.own_trail_ = {Position(8, 8), Position(7, 8), Position(6, 8), Position(5, 8)};
    bot.king_ = ReplayDerivedBot::KingInfo{101, 20, Position(10, 8), game.round_num};
    bot.king_head_estimate_ = Position(10, 8);
    SeenHead king;
    king.id = 101;
    king.team = unswbc::Team::A;
    king.position = Position(10, 8);
    bot.heads_.push_back(king);

    // Both pearl-producing segments are within the visible king's collection radius.
    assert(bot.feed_body_collectible());

    // Moving one of two predicted pearls far away drops coverage to 50%, below
    // the configured 60% requirement.
    bot.own_trail_[2] = Position(20, 20);
    assert(!bot.feed_body_collectible());

    // An incomplete body model is never sufficient evidence for early death.
    bot.own_trail_[2] = Position(6, 8);
    bot.known_body_ = 3;
    assert(!bot.feed_body_collectible());

    std::cout << "alan-v2.4 collectible-body sacrifice fixtures passed\n";
}
