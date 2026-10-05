// Focused behavior fixtures for bot-v25.0.0. Run from the repository root:
// clang++ -std=c++20 -O2 battlecode-automation/scripts/test_v25.cpp -o /tmp/test_v25
// /tmp/test_v25
// These exercise the real bot target lifecycle and scoring; match replays cover
// engine execution, population growth and elimination outcomes separately.
#include "../../bots/New/current/bot-v25.0.0/helper.hpp"
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
#include "../../bots/New/current/bot-v25.0.0/main.cpp"
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
        unswbc::game = &game;
        unswbc::ct = &ct;
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
        unswbc::game = &game;
        unswbc::ct = &ct;
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

static void targets()
{
    {
        Fixture f(16,16,100,1);
        f.open_memory(false);
        f.bot.update_explore_target();
        check(f.bot.explore_target_.has_value(), "small-map newborn explores immediately");
        auto const first=f.bot.explore_target_;
        ++f.game.round_num;
        f.bot.update_explore_target();
        check(f.bot.explore_target_==first, "assignment survives an unchanged turn");
        f.open_memory(true);
        f.bot.update_explore_target();
        check(!f.bot.explore_target_, "fully known barren map has no area pull");
    }
    {
        Fixture f;
        f.open_memory(true);
        Position const pearl(6,1);
        auto &m=f.bot.cell(pearl);
        m.pearl=true; m.pearl_seen=f.game.round_num;
        f.bot.team_explored_=ReplayDerivedBot::ALL_BLOCKS;
        f.bot.update_explore_target();
        check(f.bot.explore_target_==pearl, "covered region with a pearl remains productive");
        m.pearl=false;
        f.bot.update_explore_target();
        check(!f.bot.explore_target_, "consuming the last pearl releases a depleted region");
        m.pearl_time=500;
        f.game.round_num=20;
        f.bot.update_explore_target();
        check(!f.bot.explore_target_, "distant future spawn does not cause camping");
        m.last_seen=20; m.pearl_time=3;
        f.bot.update_explore_target();
        check(f.bot.explore_target_==pearl, "near-term spawn can attract a worker");
        m.last_seen=31; m.pearl_time=400;
        f.game.round_num=31;
        f.bot.update_explore_target();
        check(!f.bot.explore_target_, "failed or reset spawn releases patch promptly");
    }
    {
        Fixture f;
        f.open_memory(true);
        Position const pearl(6,1);
        f.bot.cell(pearl).pearl=true;
        f.bot.cell(pearl).pearl_seen=1;
        f.bot.update_explore_target();
        for (Direction d:Direction::get_direction_list()) f.set_kelp(pearl,d);
        f.bot.refresh_neighbours();
        f.bot.update_explore_target();
        check(!f.bot.explore_target_, "sealed resource target is abandoned");
    }
    {
        Fixture f(25,25,100,1);
        f.open_memory(false);
        f.bot.update_explore_target();
        int const first=f.bot.block_of(*f.bot.explore_target_);
        f.game.round_num=1+Config::REGION_STALL_ROUNDS;
        f.bot.update_explore_target();
        check(!f.bot.explore_target_ || f.bot.block_of(*f.bot.explore_target_)!=first,
              "no progress expires target despite unknown terrain");
        check(f.bot.region_retry_after_[first]>f.game.round_num, "stalled area receives a cooldown");
    }
    {
        Fixture f(64,64,100,1);
        f.open_memory(false);
        f.bot.update_explore_target();
        int const b=f.bot.block_of(*f.bot.explore_target_);
        Position const dest=*f.bot.explore_target_;
        for(int id=2;id<7;++id) f.bot.heads_.push_back(SeenHead{id,unswbc::Team::A,dest});
        ++f.game.round_num;
        f.bot.update_explore_target();
        check(!f.bot.explore_target_ || f.bot.block_of(*f.bot.explore_target_)!=b,
              "overcrowding can override commitment");
    }
    {
        std::set<int> blocks;
        for(int id=8;id<40;++id) {
            Fixture f(32,32,id,1); f.open_memory(false); f.bot.update_explore_target();
            if(f.bot.explore_target_) blocks.insert(f.bot.block_of(*f.bot.explore_target_));
        }
        check(blocks.size()>=3,"nearby newborn IDs distribute over multiple regions");
    }
}

static void handovers_and_centre()
{
    for(int id=2;id<8;++id) {
        Fixture start(16,16,id,1); start.bot.initialize_opening();
        check(start.bot.centre_seat_==(id<6?id/2:0), "only first two starting workers receive centre seats");
    }
    {
        Fixture start(16,16,0,1); start.ct.unit_count=2; start.bot.initialize_opening();
        check(start.bot.centre_grants_==1,"queen reserves existing founder's seat and fills only the missing one");
    }
    {
        Fixture start(16,16,4,1); start.bot.opening_handover_seen_=true; start.bot.initialize_opening();
        check(start.bot.centre_seat_==0,"round-one child handover overrides founder-ID allocation");
    }
    {
        Fixture start(16,16,3,10); start.bot.first_round_=10; start.bot.initialize_opening();
        check(start.bot.centre_seat_==0,"later child cannot claim centre by global ID");
    }
    Fixture queen(16,16,0,10);
    queen.bot.team_explored_=0x123456789abcdULL & ReplayDerivedBot::ALL_BLOCKS;
    queen.bot.explore_target_=Position(12,12);
    auto const msg=queen.bot.opening_handover_message(1);
    Fixture child(16,16,27,10);
    child.bot.first_round_=10;
    child.bot.receive_opening_handover(msg);
    check(child.bot.team_explored_==queen.bot.team_explored_, "new handover preserves all 49 coverage bits");
    check(child.bot.centre_seat_==1 && child.bot.parent_sector_==7, "newborn gets seat and parent's area");
    child.bot.receive_opening_handover(queen.bot.opening_handover_message(2));
    check(child.bot.centre_seat_==1,"duplicate handover cannot change a child's seat");
    child.bot.prepare_centre();
    check(child.bot.centre_scout_active(),"designated child approaches centre");
    check(child.bot.centre_pull(Position(1,1),Position(2,1))>0,"centre scout receives approach pull");
    Fixture worker(16,16,28,10);
    worker.bot.prepare_centre();
    check(worker.bot.centre_pull(Position(1,1),Position(2,1))==0,"ordinary worker has no universal centre pull");
    Fixture second(16,16,101,10); second.bot.first_round_=10;
    second.bot.receive_opening_handover(queen.bot.opening_handover_message(2));
    second.bot.prepare_centre();
    check(second.bot.centre_scout_active() && second.bot.centre_seat_==2,"second scout works with arbitrary global ID");
    Fixture bad(16,16,29,10); bad.bot.first_round_=10;
    bad.bot.receive_opening_handover(msg ^ (1ULL<<20));
    check(bad.bot.centre_seat_==0,"corrupt handover rejected");
    bad.ct.head.team=unswbc::Team::B;
    bad.bot.receive_opening_handover(msg);
    check(bad.bot.centre_seat_==0,"wrong-team handover rejected");
    Fixture old(16,16,30,11); old.bot.first_round_=10;
    old.bot.receive_opening_handover(msg);
    check(old.bot.centre_seat_==0,"existing worker cannot acquire a newborn assignment");
    Fixture legacy;
    legacy.bot.receive_explore(queen.bot.explore_message());
    check(legacy.bot.team_explored_==queen.bot.team_explored_,"legacy coverage format still decodes unchanged");

    child.ct.head.position=Position(8,8);
    child.open_memory(true);
    child.bot.update_explore_target();
    for(int n=0;n<3;++n) { ++child.game.round_num; child.bot.update_explore_target(); }
    check(!child.bot.centre_scout_active(),"scout leaves fully known empty centre");
    child.ct.head.position=Position(1,1);
    child.bot.update_explore_target();
    check(!child.bot.centre_scout_active(),"released scout is not pulled back to centre");

    second.ct.head.position=Position(8,8); second.open_memory(false);
    second.bot.update_explore_target();
    second.game.round_num+=Config::CENTRE_DWELL_ROUNDS;
    second.bot.update_explore_target();
    check(!second.bot.centre_scout_active(),"productive centre role still has a dwell limit");

    // Exercise real split-ray dispatch: straight queen body, tail directly behind.
    Fixture q(16,16,0,10); q.ct.length=6; q.ct.head.position=Position(8,8);
    q.open_memory(true); q.see_window(); q.bot.update_body_model();
    q.bot.own_trail_.clear();
    std::fill(q.bot.own_idx_.begin(),q.bot.own_idx_.end(),-1);
    for(int i=0;i<6;++i) {
        Position p(8-i,8); q.bot.own_trail_.push_back(p); q.bot.own_idx_[q.bot.index(p)]=i;
    }
    q.bot.known_body_=6;
    std::ostringstream capture; auto *saved=std::cout.rdbuf(capture.rdbuf());
    bool const sent=q.bot.send_opening_handover(2);
    q.bot.send_opening_handover(2); q.bot.send_opening_handover(2);
    std::cout.rdbuf(saved);
    check(sent && q.bot.centre_grants_==2,"queen issues two seats, never a third");
    q.bot.own_trail_={Position(8,8),Position(7,8),Position(7,7),Position(7,6),Position(7,5),Position(7,4)};
    std::fill(q.bot.own_idx_.begin(),q.bot.own_idx_.end(),-1);
    for(int i=0;i<6;++i) q.bot.own_idx_[q.bot.index(q.bot.own_trail_[i])]=i;
    check(q.bot.opening_ray_hits_child(Direction::WEST,4),"backward sonar follows bent shortened tail toward child");
    q.bot.known_body_=3;
    check(!q.bot.opening_ray_hits_child(Direction::WEST,4),"incomplete body cannot promise a child handover");
}

static void routing_and_safety()
{
    Fixture f(25,25,10,1); f.open_memory(false); f.bot.update_explore_target();
    Position const aim=*f.bot.explore_target_;
    check(f.bot.opening_route_weight(aim,4)>1.0,"pearl routes prefer assigned region");
    check(f.bot.opening_route_weight(aim,1)==1.0,"nearby pearls retain full weight");
    Position away(0,0);
    if(f.bot.block_of(away)==f.bot.block_of(aim)) away=Position(24,24);
    check(f.bot.opening_route_weight(away,4)<1.0,"outside routes are discounted, not forbidden");
    f.bot.explore_target_=Position(4,1); f.bot.build_target_field(*f.bot.explore_target_);
    f.bot.route_remaining_now_=4;
    PostMove pm; pm.head=Position(2,1); pm.steps=1; pm.path={pm.head};
    Candidate move;
    int const regional=f.bot.goal_progress_base(ReplayDerivedBot::Goal::EAT,move,pm);
    f.bot.explore_target_.reset();
    int const plain=f.bot.goal_progress_base(ReplayDerivedBot::Goal::EAT,move,pm);
    check(regional-plain==Config::STEP_DISPERSE,"EAT goal retains regional progress incentive");
    f.game.round_num=Config::OPENING_END_ROUND;
    f.bot.update_explore_target();
    check(!f.bot.explore_target_ && f.bot.opening_route_weight(aim,4)==1.0,
          "opening expires cleanly into small-map legacy policy");
    Fixture small(16,16), medium(32,32), large(64,64);
    check(small.bot.breeder_threshold()==6 && medium.bot.breeder_threshold()==16 && large.bot.breeder_threshold()==32,
          "safe population threshold scales with map area");
    Candidate unsafe; unsafe.safe=false; unsafe.head_trade=false;
    Candidate safe; safe.safe=true;
    small.ct.unit_count=5;
    check(!small.bot.candidate_allowed(unsafe,true) && small.bot.candidate_allowed(safe,true),
          "small breeder chooses safety below population threshold");
    unsafe.head_trade=true;
    check(!small.bot.candidate_allowed(unsafe,true),"understrength swarm does not override safety for ordinary trades");
    check(small.bot.candidate_allowed(unsafe,false),"no safe alternative retains escape/fallback choices");
    small.ct.unit_count=6; unsafe.head_trade=false;
    check(small.bot.candidate_allowed(unsafe,true),"healthy short worker retains previous selection policy");
    small.ct.length=4;
    check(!small.bot.candidate_allowed(unsafe,true),"existing long-worker safety remains");
    small.ct.unit_count=62;
    check(!small.bot.should_split(),"normal reproduction still stops at 62");
    check(Config::QUEEN_SEED_END_ROUND==75 && Config::QUEEN_CYCLE_END_ROUND==400,"queen calendar unchanged");
}

int main()
{
    targets(); handovers_and_centre(); routing_and_safety();
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
