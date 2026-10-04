// Focused v10.5 opening behavior checks against the actual bot implementation.
// clang++ -std=c++20 -O2 battlecode-automation/scripts/test_v105.cpp -o /tmp/test_v105 && /tmp/test_v105
// Override BOT_SOURCE/BOT_HELPER to check an immutable experiment snapshot.
#ifndef BOT_HELPER
#define BOT_HELPER "../../bots/current/bot-v10.5/helper.hpp"
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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#define main battlecode_program_main
#ifndef BOT_SOURCE
#define BOT_SOURCE "../../bots/current/bot-v10.5/main.cpp"
#endif
#include BOT_SOURCE
#undef main
#undef private

struct OpeningFixture {
    unswbc::Game game{32, 32, 64};
    unswbc::Controller ct{100, unswbc::Team::A, Direction::EAST, {}, 64};
    ReplayDerivedBot bot{ct, game};
    Position center{16, 16};

    OpeningFixture() {
        unswbc::game = &game;
        unswbc::ct = &ct;
        game.round_num = std::max(0, Config::OPENING_CENTER_END_ROUND - 1);
        ct.head.position = Position(6, 16);
        ct.length = 3;
        ct.unit_count = 20;
        bot.retaining_now_ = false;
        std::fill(bot.objective_dist_.begin(), bot.objective_dist_.end(), INF);
    }

    int penalty(Position p) const { return bot.opening_center_penalty(p); }

    void center_knowledge(bool fertile) {
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx) {
                auto &m = bot.cell(Position(center.x + dx, center.y + dy));
                m.seen = true;
                m.pearl_time = fertile ? 20 : -1;
                m.pearl = false;
            }
    }

    void head(int id, unswbc::Team team, Position p) {
        SeenHead h;
        h.id = id;
        h.team = team;
        h.position = p;
        bot.heads_.push_back(h);
    }
};

void active_opening_and_radius() {
    OpeningFixture f;
    int const extra = std::max(0, Config::OPENING_CENTER_WEIGHT - 45);
    for (int x = 0; x < f.game.width; ++x)
        for (int y = 0; y < f.game.height; ++y) {
            Position const p(x, y);
            int const gap = std::max(0, f.bot.chebyshev_distance(p, f.center) -
                                           Config::OPENING_CENTER_RADIUS);
            assert(f.penalty(p) == extra * gap);
        }
    assert(f.penalty(f.center) == 0);
    // A nearby forward move should never have greater center penalty.
    assert(f.penalty(Position(7, 16)) <= f.penalty(Position(5, 16)));
    f.center_knowledge(true);
    assert(f.penalty(Position(0, 16)) ==
           extra * std::max(0, 16 - Config::OPENING_CENTER_RADIUS));
}

void exact_round_and_food_boundaries() {
    OpeningFixture f;
    Position const far(0, 16);
    int const active = f.penalty(far);
    f.game.round_num = Config::OPENING_CENTER_END_ROUND;
    assert(f.penalty(far) == 0);
    f.game.round_num = 499;
    assert(f.penalty(far) == 0);
    f.game.round_num = std::max(0, Config::OPENING_CENTER_END_ROUND - 1);
    auto &food = f.bot.objective_dist_[f.bot.index(f.ct.get_position())];
    food = Config::OPENING_FOOD_PRIORITY_RADIUS + 1;
    assert(f.penalty(far) == active);
    food = Config::OPENING_FOOD_PRIORITY_RADIUS;
    assert(f.penalty(far) == 0);
    food = 0;
    assert(f.penalty(far) == 0);
    food = INF;
    f.bot.retaining_now_ = true;
    assert(f.penalty(far) == 0);
}

void observed_barren_center() {
    OpeningFixture f;
    Position const far(0, 16);
    int const active = f.penalty(far);
    // Unknown cells do not prove that the center is barren.
    int sampled = 0;
    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx) {
            if (sampled++ >= 23)
                continue;
            auto &m = f.bot.cell(Position(f.center.x + dx, f.center.y + dy));
            m.seen = true;
            m.pearl_time = -1;
        }
    assert(f.penalty(far) == active);
    f.center_knowledge(false);
    assert(f.penalty(far) == 0);
    f.center_knowledge(true);
    assert(f.penalty(far) == active);
}

void central_crowding() {
    OpeningFixture f;
    Position const far(0, 16);
    int const active = f.penalty(far);
    for (int i = 0; i < Config::OPENING_CENTER_CROWD_LIMIT; ++i)
        f.head(1000 + i, unswbc::Team::B, f.center);
    assert(f.penalty(far) == active); // Enemy heads are not friendly congestion.
    f.bot.heads_.clear();
    for (int i = 0; i < Config::OPENING_CENTER_CROWD_LIMIT - 1; ++i)
        f.head(1000 + i, unswbc::Team::A, f.center);
    assert(f.penalty(far) == active);
    f.head(2000, unswbc::Team::A, f.center);
    assert(f.penalty(far) == 0);
    f.bot.heads_.clear();
    f.ct.head.position = f.center;
    // The current snake counts once, even if observe_heads includes its head.
    f.head(f.ct.get_id(), unswbc::Team::A, f.center);
    for (int i = 0; i < Config::OPENING_CENTER_CROWD_LIMIT - 1; ++i)
        f.head(1000 + i, unswbc::Team::A, f.center);
    assert(f.penalty(far) == 0);
}

void alpha_phase_boundary_and_recipient_safety() {
    OpeningFixture f;
    f.ct.length = 16;
    f.bot.retaining_now_ = true;
    f.bot.valuable_now_ = true;
    assert(Config::ALPHA_FEED_START_ROUND >= Config::FEED_START_ROUND);
    assert(f.ct.length > Config::FEEDER_MAX_LENGTH);
    f.game.round_num = Config::ALPHA_FEED_START_ROUND - 1;
    TeamSignal request{200, Position(8, 16),
                       std::max(17, Config::FEED_MIN_GROWER_LENGTH),
                       f.game.round_num, true, true};
    f.bot.signals_ = {request};
    f.bot.update_coordination();
    assert(!f.bot.feeder_target_); // A retaining large snake is not an early donor.

    f.game.round_num = Config::ALPHA_FEED_START_ROUND;
    request.round = f.game.round_num;
    auto selected = [&](TeamSignal const &s) {
        f.bot.signals_ = {s};
        f.bot.update_coordination();
        return f.bot.feeder_target_.has_value();
    };
    assert(selected(request));
    assert(f.bot.feeder_target_->id == request.id);
    // Default helper vision is empty: selecting a sonar recipient is allowed,
    // but actually sacrificing without seeing that recipient must remain blocked.
    assert(f.ct.get_tile(request.position) == nullptr);
    assert(!f.bot.play_feeder_strategy());

    TeamSignal changed = request;
    changed.length = f.ct.length;
    assert(!selected(changed));
    changed.length = f.ct.length - 1;
    assert(!selected(changed));
    changed = request;
    changed.round = f.game.round_num - Config::FEED_REQUEST_MAX_AGE;
    assert(selected(changed));
    --changed.round;
    assert(!selected(changed));
    changed = request;
    changed.feed_request = false;
    assert(!selected(changed));
    changed = request;
    changed.grower = false;
    assert(!selected(changed));
    changed = request;
    changed.id = f.ct.get_id();
    assert(!selected(changed));
}

int main() {
    active_opening_and_radius();
    exact_round_and_food_boundaries();
    observed_barren_center();
    central_crowding();
    alpha_phase_boundary_and_recipient_safety();
    std::cout << "v10.5 fixtures passed: opening attraction/radius/cutoff/food/terrain/crowding, "
                 "alpha cutoff/recipient validation/remote sacrifice safety\n";
}
