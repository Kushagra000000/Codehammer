// Focused behavior fixtures for bot-v22.1.0. Run from the repository root:
// clang++ -std=c++20 -O2 battlecode-automation/scripts/test_v221.cpp -o /tmp/test_v221
// /tmp/test_v221
// These exercise the real bot target lifecycle and scoring; match replays cover
// engine execution, population growth and elimination outcomes separately.
#include "../../bots/New/current/bot-v22.1.0/helper.hpp"
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
#include "../../bots/New/current/bot-v22.1.0/main.cpp"
#undef main
#undef private

static int failures = 0;
static int checks = 0;
static void check(bool ok, char const *name)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

struct Fixture
{
    unswbc::Game game;
    unswbc::Controller ct;
    ReplayDerivedBot bot;

    explicit Fixture(int w = 25, int h = 25, int id = 100, int round = 1)
        : game(w, h, 64), ct(id, unswbc::Team::A, Direction::EAST, {}, 64), bot(ct, game)
    {
        unswbc::game = &game;
        unswbc::ct = &ct;
        game.round_num = round;
        ct.head.position = Position(1, 1);
        ct.length = 3;
        ct.unit_count = 8;
        bot.first_round_ = 1;
        bot.ensure_blocks();
        bot.refresh_neighbours();
        bot.start_turn_clock(ReplayDerivedBot::clock_now());
    }

    void open_memory(bool seen = false)
    {
        for (CellMemory &m : bot.memory_)
        {
            m.seen = seen;
            m.last_seen = seen ? game.round_num : -1;
            for (RememberedEdge &e : m.edges)
                e = {true, EdgeType::EMPTY, -1};
        }
        bot.nbr_all_dirty_ = true;
        bot.refresh_neighbours();
    }

    void set_kelp(Position p, Direction d)
    {
        bot.cell(p).edges[static_cast<std::size_t>(direction_index(d))] = {true, EdgeType::KELP, -1};
        bot.cell(p.add_dir(d)).edges[static_cast<std::size_t>(direction_index(d.get_opposite()))] =
            {true, EdgeType::KELP, -1};
        bot.nbr_all_dirty_ = true;
    }

    void see_window(std::optional<Position> pearl = std::nullopt)
    {
        std::vector<unswbc::Tile> tiles;
        Position const head = ct.get_position();
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
            {
                Position const p(bot.wrap(head.x + dx, game.width), bot.wrap(head.y + dy, game.height));
                tiles.emplace_back(p, std::nullopt, -1, pearl && p == *pearl);
                if (p == head)
                    tiles.back().dragon_part = ct.head;
                else
                {
                    auto const [fx, fy] = ct.get_dir().get_offset();
                    for (int part = 1; part < ct.length && part <= 3; ++part)
                        if (dx == -part * fx && dy == -part * fy)
                            tiles.back().dragon_part = unswbc::DragonPart(p, ct.get_id(), ct.get_team(), ct.get_dir(), false);
                }
            }
        ct.vision = unswbc::Vision(std::move(tiles));
        bot.remember_vision();
        bot.refresh_neighbours();
    }

    void prepare_food()
    {
        bot.start_turn_clock(ReplayDerivedBot::clock_now());
        bot.update_body_model();
        bot.observe_heads();
        bot.remember_bodies();
        bot.compute_roles();
        bot.update_king_info();
        bot.build_threat_maps();
        bot.update_coordination();
        bot.prepare_survival();
        bot.prepare_etas();
        bot.build_objective_map();
        bot.plan_route();
        bot.choose_goal();
    }
};

static void opening_distribution()
{
    Fixture siblings(50, 50, 0);
    Position const first_founder = siblings.bot.opening_point(0);
    siblings.ct.head.dragon_id = 1;
    check(siblings.bot.opening_point(0) == first_founder, "opposing founders receive the same centre proposal");
    bool siblings_distinct = true;
    Position previous = siblings.bot.opening_point(0);
    for (int id = 2; id < 258; ++id)
    {
        siblings.ct.head.dragon_id = id;
        Position const proposal = siblings.bot.opening_point(0);
        siblings_distinct = siblings_distinct && proposal != previous;
        previous = proposal;
    }
    check(siblings_distinct, "consecutive split-child IDs receive distinct initial destination proposals");
    for (auto const &[w, h] : std::array<std::pair<int, int>, 5>{{{20, 40}, {25, 25}, {32, 32}, {40, 40}, {50, 50}}})
    {
        int count = 0, central = 0, outer = 0;
        std::array<int, 4> quadrants{};
        std::set<int> blocks;
        for (int id = 2; id < 258; ++id)
        {
            Fixture f(w, h, id);
            f.bot.update_explore_target();
            if (!f.bot.explore_target_)
                continue;
            Position const p = *f.bot.explore_target_;
            int const b = f.bot.block_of(p);
            ++count;
            ++quadrants[static_cast<std::size_t>((p.x >= w / 2 ? 1 : 0) + (p.y >= h / 2 ? 2 : 0))];
            central += b % 7 >= 2 && b % 7 <= 4 && b / 7 >= 2 && b / 7 <= 4;
            outer += b % 7 == 0 || b % 7 == 6 || b / 7 == 0 || b / 7 == 6;
            blocks.insert(b);
        }
        char label[160];
        std::snprintf(label, sizeof(label), "%dx%d starts regional exploration immediately", w, h);
        check(count == 256, label);
        std::snprintf(label, sizeof(label), "%dx%d destinations cover every quadrant and outer regions", w, h);
        check(*std::min_element(quadrants.begin(), quadrants.end()) > 0 && outer > 0 && blocks.size() >= 16, label);
        std::snprintf(label, sizeof(label), "%dx%d central nine blocks receive more than uniform share", w, h);
        check(central * 49 > count * 12, label); // uniform would be 9/49; require a material preference
        std::printf("  distribution %dx%d: %d targets, %d central, %d outer, %zu blocks\n",
                    w, h, count, central, outer, blocks.size());
    }
}

static void target_lifecycle()
{
    {
        Fixture f;
        f.bot.update_explore_target();
        auto const initial = f.bot.explore_target_;
        check(initial.has_value(), "opening target is assigned");
        if (initial)
        {
            f.ct.head.position = *initial;
            ++f.game.round_num;
            f.bot.update_explore_target();
            check(f.bot.explore_target_ != initial, "arrival immediately releases the assigned region");
            f.ct.head.position = Position(1, 1);
            bool returned = false;
            for (int turn = 0; turn < 25; ++turn)
            {
                ++f.game.round_num;
                f.bot.update_explore_target();
                returned = returned || f.bot.explore_target_ == initial;
            }
            check(!returned, "leaving a completed region does not reactivate its old attraction");
        }
    }
    {
        Fixture f;
        f.bot.update_explore_target();
        auto const initial = f.bot.explore_target_;
        if (initial)
        {
            f.bot.team_explored_ |= 1ULL << f.bot.block_of(*initial);
            ++f.game.round_num;
            f.bot.update_explore_target();
            check(f.bot.explore_target_ != initial, "shared coverage releases an already explored destination");
        }
    }
    {
        Fixture f;
        f.bot.update_explore_target();
        auto const initial = f.bot.explore_target_;
        if (initial)
        {
            int const b = f.bot.block_of(*initial);
            for (int y = 0; y < f.game.height; ++y)
                for (int x = 0; x < f.game.width; ++x)
                    if (f.bot.block_of(Position(x, y)) == b)
                    {
                        auto &m = f.bot.cell(Position(x, y));
                        m.seen = true;
                        m.last_seen = f.game.round_num;
                        m.pearl = false;
                        f.bot.note_first_sighting(Position(x, y));
                    }
            ++f.game.round_num;
            f.bot.update_explore_target();
            check(f.bot.explore_target_ != initial, "fully observed depleted region is released immediately");
        }
    }
    {
        Fixture f;
        f.bot.update_explore_target();
        auto const initial = f.bot.explore_target_;
        if (initial)
        {
            for (Direction d : Direction::get_direction_list())
                f.set_kelp(*initial, d);
            f.bot.refresh_neighbours();
            ++f.game.round_num;
            f.bot.update_explore_target();
            check(f.bot.explore_target_ != initial, "kelp-isolated destination is replaced");
        }
    }
    {
        Fixture f;
        f.bot.update_explore_target();
        auto const initial = f.bot.explore_target_;
        std::array<Position, 4> const loop{{Position(2, 1), Position(2, 2), Position(1, 2), Position(1, 1)}};
        for (int turn = 0; turn < 16 && f.bot.explore_target_ == initial; ++turn)
        {
            Candidate move;
            move.destination = loop[static_cast<std::size_t>(turn % 4)];
            f.bot.goal_ = ReplayDerivedBot::Goal::EXPLORE;
            f.bot.note_opening_move(move);
            f.ct.head.position = move.destination;
            ++f.game.round_num;
            f.bot.update_explore_target();
        }
        check(initial && f.bot.explore_target_ != initial, "unproductive travel abandons a stalled destination promptly");
        f.bot.team_explored_ = ReplayDerivedBot::ALL_BLOCKS;
        f.bot.update_explore_target();
        check(!f.bot.explore_target_, "fully explored map leaves no artificial destination to circle");
    }
}

static void pearl_detour_and_parent_split()
{
    Fixture f(25, 25, 0); // the founder participates in opening scouting and splitting
    f.bot.update_explore_target();
    auto const target = f.bot.explore_target_;
    check(target.has_value(), "founder receives an opening destination");
    if (!target)
        return;
    Position const origin = f.ct.get_position();
    Direction detour = Direction::NORTH;
    int greatest = -INF;
    for (Direction d : Direction::get_direction_list())
    {
        if (d == f.ct.get_dir().get_opposite())
            continue;
        Position const p = origin.add_dir(d);
        int const dist = f.bot.target_dist_[static_cast<std::size_t>(f.bot.index(p))];
        if (dist > greatest)
            greatest = dist, detour = d;
    }
    Position const pearl = origin.add_dir(detour);
    f.see_window(pearl);
    f.prepare_food();
    f.bot.update_explore_target();
    check(f.bot.goal_ == ReplayDerivedBot::Goal::EAT, "visible open-path pearl takes the EAT goal during regional travel");
    check(f.bot.explore_target_ == target, "finding a pearl en route preserves the unfinished destination");

    std::vector<Candidate> const choices = f.bot.single_step_candidates(false);
    auto const best = std::max_element(choices.begin(), choices.end(), [](Candidate const &a, Candidate const &b)
                                      { return a.score < b.score; });
    check(best != choices.end() && best->destination == pearl && best->direct_pearl,
          "an immediately accessible pearl wins even when it leads away from the region");
    if (best == choices.end())
        return;
    PostMove pm;
    check(f.bot.simulate_path(best->steps, pm, false) && pm.eaten == 1 && pm.length == 4,
          "the pearl move grows the founder from length 3 to split length 4");
    int const with_target = f.bot.goal_progress(ReplayDerivedBot::Goal::EAT, *best, pm);
    auto const field = f.bot.target_dist_;
    f.bot.explore_target_.reset();
    f.bot.target_dist_.clear();
    int const without_target = f.bot.goal_progress(ReplayDerivedBot::Goal::EAT, *best, pm);
    f.bot.explore_target_ = target;
    f.bot.target_dist_ = field;
    check(with_target == without_target, "EAT progress is independent of regional attraction");

    f.bot.opening_stalled_moves_ = Config::OPENING_STALL_MOVES - 1;
    f.bot.note_opening_move(*best);
    check(f.bot.opening_stalled_moves_ == 0 && f.bot.explore_target_ == target,
          "food detours pause travel-stall accounting without dropping the destination");

    // Apply the fixture's engine state for the eaten pearl, then run a real turn.
    // Captured protocol proves play_turn executes a split, rather than merely
    // testing should_split in isolation. The engine creates the child separately.
    f.ct.head.position = pearl;
    f.ct.head.dir = detour;
    f.ct.length = 4;
    ++f.game.round_num;
    f.see_window();
    std::ostringstream protocol;
    std::streambuf *const old = std::cout.rdbuf(protocol.rdbuf());
    f.bot.start_turn_clock(ReplayDerivedBot::clock_now());
    f.bot.play_turn();
    std::cout.rdbuf(old);
    check(protocol.str().find("SPLIT 2\n") != std::string::npos,
          "the next real bot turn performs a normal population-building split");
    check(f.bot.explore_target_ == target, "the parent keeps its unfinished destination through the split action");

    // The head-owning parent retains two segments and proceeds on its next turn.
    f.ct.length = 2;
    ++f.ct.unit_count;
    ++f.game.round_num;
    f.see_window();
    f.prepare_food();
    f.bot.update_explore_target();
    check(f.bot.explore_target_ == target && f.bot.goal_ == ReplayDerivedBot::Goal::EXPLORE,
          "after splitting and exhausting local food, the parent resumes regional travel");
    int best_pull = 0;
    for (Direction d : Direction::get_direction_list())
        if (d != f.ct.get_dir().get_opposite())
            best_pull = std::max(best_pull, f.bot.disperse_score(f.ct.get_position().add_dir(d)));
    check(best_pull > 0, "the resumed destination still supplies an actionable travel preference");
}

static void nonadjacent_open_path_pearls()
{
    for (int steps : {2, 3})
    {
        Fixture f(25, 25, 0);
        f.ct.head.position = Position(1, 4);
        f.bot.update_explore_target();
        Position const next = f.ct.get_position().add_dir(Direction::NORTH);
        Position const pearl(1, 4 - steps);
        f.see_window(pearl);
        f.prepare_food();
        f.bot.update_explore_target();
        int const before = f.bot.target_dist_[static_cast<std::size_t>(f.bot.index(f.ct.get_position()))];
        int const after = f.bot.target_dist_[static_cast<std::size_t>(f.bot.index(next))];
        std::vector<Candidate> const choices = f.bot.single_step_candidates(false);
        auto const best = std::max_element(choices.begin(), choices.end(), [](Candidate const &a, Candidate const &b)
                                          { return a.score < b.score; });
        char label[180];
        std::snprintf(label, sizeof(label), "open-path pearl %d tiles away wins its first non-eating step away from the region", steps);
        check(after > before && f.bot.goal_ == ReplayDerivedBot::Goal::EAT &&
                  best != choices.end() && !best->direct_pearl && best->destination == next,
              label);
    }
}

static void imminent_resource_before_regional_travel()
{
    Fixture f(25, 25, 0);
    f.ct.head.position = Position(1, 4);
    f.bot.update_explore_target();
    f.see_window();
    // A real resource objective without a pearl route: this tile spawns next
    // round, two open steps north. Approach now, reach it after it spawns. The
    // assigned region lies south/east, so this detects geometry overriding food
    // even when no pearl exists yet and Goal::EAT cannot protect its priority.
    Position const source(1, 2);
    f.ct.get_tile(source)->pearl_time = 1;
    f.bot.remember_vision();
    f.prepare_food();
    f.bot.update_explore_target();
    Position const next = f.ct.get_position().add_dir(Direction::NORTH);
    int const before = f.bot.target_dist_[static_cast<std::size_t>(f.bot.index(f.ct.get_position()))];
    int const after = f.bot.target_dist_[static_cast<std::size_t>(f.bot.index(next))];
    std::vector<Candidate> const choices = f.bot.single_step_candidates(false);
    auto const best = std::max_element(choices.begin(), choices.end(), [](Candidate const &a, Candidate const &b)
                                      { return a.score < b.score; });
    check(f.bot.route_first_ < 0 && f.bot.goal_ == ReplayDerivedBot::Goal::EXPLORE && after > before &&
              best != choices.end() && best->destination == next && !best->direct_pearl,
          "an imminent reachable pearl spawn wins over opposing regional travel without a planned pearl route");
}

static void observed_productive_region_then_depletion()
{
    Fixture f;
    f.bot.update_explore_target();
    auto const first = f.bot.explore_target_;
    if (!first)
    {
        check(false, "productive-region fixture obtains a destination");
        return;
    }
    f.ct.head.position = *first;
    Position const pearl = first->add_dir(Direction::NORTH);
    ++f.game.round_num;
    f.see_window(pearl);
    f.prepare_food();
    f.bot.update_explore_target();
    check(f.bot.explore_target_ != first && f.bot.goal_ == ReplayDerivedBot::Goal::EAT,
          "arrival retires regional pull while pearl routing still harvests resources there");
    auto const next = f.bot.explore_target_;
    f.ct.head.position = pearl;
    f.ct.head.dir = Direction::NORTH;
    ++f.game.round_num;
    f.see_window();
    f.prepare_food();
    f.bot.update_explore_target();
    check(f.bot.explore_target_ != first && f.bot.goal_ == ReplayDerivedBot::Goal::EXPLORE &&
              ((f.bot.opening_retired_ >> f.bot.block_of(*first)) & 1ULL),
          "after local pearls are consumed, the completed region has no restoring attraction");
    check(!next || f.bot.explore_target_ == next || f.bot.opening_region_done(*next),
          "depletion keeps the next unfinished region instead of restarting the old assignment");
}

static void active_target_cutoff_transition()
{
    Fixture small(25, 25, 100, 74);
    small.bot.update_explore_target();
    check(small.bot.opening_target_ && small.bot.explore_target_, "small-map target is active immediately before the opening cutoff");
    small.game.round_num = 75;
    small.bot.update_explore_target();
    check(!small.bot.opening_target_ && !small.bot.explore_target_ && small.bot.target_dist_.empty(),
          "small-map active opening field is cleared at round 75");

    Fixture big(50, 50, 100, 74);
    big.bot.update_explore_target();
    check(big.bot.opening_target_ && big.bot.explore_target_, "large-map target is active immediately before the opening cutoff");
    big.game.round_num = 75;
    big.bot.update_explore_target();
    check(!big.bot.opening_target_ && big.bot.explore_target_.has_value(),
          "large-map active opening target transitions to the legacy exploration policy at round 75");
    Position const head = big.ct.get_position();
    bool checked = false, legacy_pull = true;
    for (Direction d : Direction::get_direction_list())
    {
        Position const p = head.add_dir(d);
        int const delta = big.bot.target_dist_[static_cast<std::size_t>(big.bot.index(head))] -
                          big.bot.target_dist_[static_cast<std::size_t>(big.bot.index(p))];
        if (delta)
        {
            checked = true;
            legacy_pull = legacy_pull && big.bot.disperse_score(p) == std::clamp(delta, -1, 1) * Config::STEP_DISPERSE;
        }
    }
    check(checked && legacy_pull, "post-opening large-map movement uses the unchanged legacy pull strength");
}

static void late_behavior_and_split_boundaries()
{
    Fixture small(25, 25, 100, 75);
    small.bot.update_explore_target();
    check(!small.bot.explore_target_, "small-map opening bias is disabled from round 75");

    Fixture big(50, 50, 100, 75);
    big.bot.update_explore_target();
    check(big.bot.explore_target_.has_value(), "large-map legacy exploration remains active after opening");
    big.game.round_num = Config::GATHER_ROUND;
    big.bot.update_explore_target();
    check(!big.bot.explore_target_, "gather phase has no exploration destination");

    Fixture worker;
    worker.ct.length = 4;
    check(worker.bot.should_split() && worker.bot.worker_child_size() == 2,
          "early worker still splits at length 4 and keeps two segments");
    worker.ct.unit_count = 62;
    check(!worker.bot.should_split(), "normal split still preserves escape unit slots");

    Fixture queen(25, 25, 0, 74);
    queen.ct.length = 4;
    queen.bot.compute_roles();
    check(queen.bot.queen_seeding() && queen.bot.should_split(), "queen still seeds the swarm before round 75");
    queen.game.round_num = 75;
    queen.bot.compute_roles();
    check(queen.bot.queen_cycling() && !queen.bot.should_split() && queen.bot.retaining_length(),
          "queen preservation and cycling behavior are unchanged after round 75");
}

int main()
{
    opening_distribution();
    target_lifecycle();
    pearl_detour_and_parent_split();
    nonadjacent_open_path_pearls();
    imminent_resource_before_regional_travel();
    observed_productive_region_then_depletion();
    active_target_cutoff_transition();
    late_behavior_and_split_boundaries();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
