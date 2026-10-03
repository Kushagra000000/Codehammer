#include "../../bots/alan/alan-v2/helper.hpp"

#include <cassert>
#include <iostream>

#define private public
#define main alan_v2_program_main
#include "../../bots/alan/alan-v2/main.cpp"
#undef main
#undef private

struct Fixture
{
    unswbc::Game game;
    unswbc::Controller controller;
    ReplayDerivedBot bot;

    Fixture(int width, int height)
        : game(width, height, 64),
          controller(100, unswbc::Team::A, Direction::EAST, {}, 64),
          bot(controller, game)
    {
        unswbc::game = &game;
        unswbc::ct = &controller;
        controller.head.position = Position(width / 4, height / 2);
        controller.length = 3;
        controller.unit_count = 20;
    }
};

static void map_aware_late_phases()
{
    Fixture arena(11, 11);
    assert(arena.bot.gather_round() == 405);
    assert(arena.bot.sacrifice_round() == 425);

    Fixture standard(32, 32);
    assert(standard.bot.gather_round() == 398);
    assert(standard.bot.sacrifice_round() == 418);

    Fixture schooltime(60, 40);
    assert(schooltime.bot.gather_round() == 380);
    assert(schooltime.bot.sacrifice_round() == 400);

    Fixture largest(64, 64);
    assert(largest.bot.gather_round() == 366);
    assert(largest.bot.sacrifice_round() == 386);
}

static void population_ready_specialisation()
{
    Fixture f(32, 32);
    f.game.round_num = 249;
    f.controller.unit_count = 61;
    assert(!f.bot.specialise_started());
    f.controller.unit_count = 62;
    assert(f.bot.specialise_started());
    f.controller.unit_count = 20;
    f.game.round_num = 250;
    assert(f.bot.specialise_started());
}

int main()
{
    map_aware_late_phases();
    population_ready_specialisation();
    std::cout << "alan-v2 adaptive phase timing fixtures passed\n";
}
