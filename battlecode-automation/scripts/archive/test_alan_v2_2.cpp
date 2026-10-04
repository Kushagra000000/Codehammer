#include "../../bots/alan/alan-v2.2/helper.hpp"

#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

#define private public
#define main alan_v2_2_program_main
#include "../../bots/alan/alan-v2.2/main.cpp"
#undef main
#undef private

struct Fixture
{
    static constexpr int width = 8;
    static constexpr int height = 8;
    static constexpr int self_id = 100;
    static constexpr int king_id = 101;
    Position const start{2, 2};
    Position const king_position{4, 2};
    unswbc::Game game;
    unswbc::Controller controller;
    ReplayDerivedBot bot;

    static std::vector<unswbc::Tile> tiles()
    {
        std::vector<unswbc::Tile> result;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                std::optional<unswbc::DragonPart> part;
                if (x == 4 && y == 2)
                    part.emplace(Position(x, y), king_id, unswbc::Team::A,
                                 Direction::EAST, true);
                result.emplace_back(Position(x, y), part, -1, false);
            }
        return result;
    }

    Fixture()
        : game(width, height, 64),
          controller(self_id, unswbc::Team::A, Direction::EAST,
                     unswbc::Vision(tiles()), 64),
          bot(controller, game)
    {
        unswbc::game = &game;
        unswbc::ct = &controller;
        controller.head.position = start;
        controller.length = 4;
        controller.unit_count = 20;
        game.round_num = Config::GATHER_ROUND;
    }

    void set_edge(Position position, Direction direction, EdgeType type, int portal_id = -1)
    {
        auto &forward = controller.get_tile(position)->get_edge(direction);
        auto &reverse = controller.get_tile(position.add_dir(direction))->get_edge(direction.get_opposite());
        forward.edge_type = reverse.edge_type = type;
        if (type == EdgeType::PORTAL)
            forward.portal_id = reverse.portal_id = portal_id;
        else
            forward.portal_id = reverse.portal_id = std::nullopt;
    }

    void observe()
    {
        bot.remember_vision();
        bot.observe_heads();
        bot.king_ = ReplayDerivedBot::KingInfo{king_id, 20, king_position, game.round_num};
        bot.king_head_estimate_ = king_position;
    }
};

static void open_and_detoured_paths_qualify()
{
    Fixture open;
    open.observe();
    assert(open.bot.visible_path_to_king(Config::GATHER_DISTANCE));
    assert(open.bot.sacrifice_started());

    Fixture detour;
    detour.set_edge(detour.start, Direction::EAST, EdgeType::KELP);
    detour.observe();
    assert(detour.bot.visible_path_to_king(Config::GATHER_DISTANCE));
    assert(detour.bot.sacrifice_started());
}

static void barriers_and_portals_do_not_qualify()
{
    Fixture enclosed;
    for (Direction direction : Direction::get_direction_list())
        enclosed.set_edge(enclosed.start, direction, EdgeType::KELP);
    enclosed.observe();
    assert(!enclosed.bot.visible_path_to_king(Config::GATHER_DISTANCE));
    assert(!enclosed.bot.sacrifice_started());

    Fixture portal_only;
    for (Direction direction : Direction::get_direction_list())
        portal_only.set_edge(portal_only.start, direction, EdgeType::KELP);
    portal_only.set_edge(portal_only.start, Direction::EAST, EdgeType::PORTAL, 7);
    portal_only.observe();
    assert(!portal_only.bot.visible_path_to_king(Config::GATHER_DISTANCE));
    assert(!portal_only.bot.sacrifice_started());

    portal_only.game.round_num = Config::SACRIFICE_HARD_ROUND;
    assert(portal_only.bot.sacrifice_started());
}

int main()
{
    open_and_detoured_paths_qualify();
    barriers_and_portals_do_not_qualify();
    std::cout << "alan-v2.2 path-aware sacrifice fixtures passed\n";
}
