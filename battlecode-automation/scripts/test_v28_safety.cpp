#include "../../bots/New/current/bot-v28.0.0-Kushagra/helper.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#define main bot_main
#include "../../bots/New/current/bot-v28.0.0-Kushagra/main.cpp"
#undef main
#undef private

struct Fixture {
    unswbc::Game game{25,25,64};
    unswbc::Controller ct;
    ReplayDerivedBot bot;
    Fixture(int id=4,int round=10,int length=3): ct(id,unswbc::Team::A,Direction::EAST,{},64), bot(ct,game) {
        unswbc::game=&game; unswbc::ct=&ct;
        game.round_num=round; ct.length=length; ct.unit_count=3; ct.head.position=Position(5,5);
        bot.first_round_=round; bot.birth_length_=length;
    }
    void window(Position head, Direction facing) {
        ct.head.position=head; ct.head.dir=facing;
        std::vector<unswbc::Tile> tiles;
        for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx)
            tiles.emplace_back(Position(bot.wrap(head.x+dx,25),bot.wrap(head.y+dy,25)),std::nullopt,-1,false);
        ct.vision=unswbc::Vision(std::move(tiles));
        ct.get_tile(head)->dragon_part=ct.head;
    }
    void part(Position p,int id,unswbc::Team team,Direction d,bool head=false) {
        ct.get_tile(p)->dragon_part=unswbc::DragonPart(p,id,team,d,head);
    }
    void own(std::vector<Position> const &body) {
        ct.length=static_cast<int>(body.size());
        for(std::size_t i=1;i<body.size();++i) {
            Direction d=Direction::NORTH;
            for(Direction v:Direction::get_direction_list()) if(body[i].add_dir(v)==body[i-1]) d=v;
            part(body[i],ct.get_id(),ct.get_team(),d);
        }
    }
    void wall(Position p,Direction d) {
        ct.get_tile(p)->get_edge(d).edge_type=EdgeType::KELP;
        if(auto *t=ct.get_tile(p.add_dir(d))) t->get_edge(d.get_opposite()).edge_type=EdgeType::KELP;
    }
    void prepare() {
        bot.start_turn_clock(ReplayDerivedBot::clock_now());
        bot.remember_vision(); bot.refresh_neighbours(); bot.prepare_centre();
        bot.update_body_model(); bot.observe_heads(); bot.remember_bodies(); bot.compute_roles(); bot.update_king_info();
        bot.build_threat_maps(); bot.update_coordination(); bot.prepare_survival(); bot.prepare_etas(); bot.build_objective_map();
        bot.plan_route(); bot.choose_goal();
    }
    int ally_exits_after(PostMove const&pm,int id) {
        auto *h=bot.seen_head(id); int exits=0;
        for(Direction d:Direction::get_direction_list()) {
            Step s=bot.step_from(h->position,d);
            if(!s.passable || !s.destination_known) continue;
            int n=bot.index(s.destination);
            if(bot.other_occ_[n] || bot.post_index(pm,n)>=0) continue;
            ++exits;
        }
        return exits;
    }
};

int main() {
    int checks=0;
    auto require=[&](bool ok){ if(!ok) { std::cerr << "regression failed after " << checks << " checks\n"; std::exit(1); } ++checks; };
    {
        Fixture f(4,10,2); f.window(Position(7,7),Direction::EAST); f.own({{7,7},{6,7}});
        f.part({6,6},6,unswbc::Team::A,Direction::EAST,true); f.part({5,6},6,unswbc::Team::A,Direction::EAST);
        f.wall({6,6},Direction::NORTH); f.prepare();
        PostMove pm; bool legal=f.bot.simulate_path({Direction::NORTH},pm,false);
        require(legal && f.ally_exits_after(pm,6)==1 && f.bot.ally_box_cost(pm)==0);
        std::cout<<"TAIL_RELEASE legal="<<legal<<" actual_ally_exits_after="<<f.ally_exits_after(pm,6)<<" ally_box_cost="<<f.bot.ally_box_cost(pm)<<" released_tail_post_index="<<f.bot.post_index(pm,f.bot.index({6,7}))<<"\n";
    }
    {
        Fixture f; f.window({5,5},Direction::EAST); f.own({{5,5},{4,5},{3,5}});
        f.part({7,6},6,unswbc::Team::A,Direction::NORTH,true); f.part({7,7},6,unswbc::Team::A,Direction::NORTH);
        f.wall({7,6},Direction::EAST); f.wall({7,6},Direction::WEST);
        f.part({8,5},9,unswbc::Team::B,Direction::SOUTH,true); f.part({8,4},9,unswbc::Team::B,Direction::SOUTH);
        f.ct.get_tile({7,5})->pearl=true; f.prepare();
        f.bot.goal_=ReplayDerivedBot::Goal::EXPLORE; f.bot.base_goal_=f.bot.goal_;
        std::vector<Candidate> candidates; f.bot.add_pearl_sprints(candidates);
        for(auto const&c:candidates) {
            PostMove pm; bool legal=f.bot.simulate_path(c.steps,pm,false);
            require(legal && f.bot.ally_box_cost(pm)>0 && c.score < -1000);
            std::cout<<"PEARL_SPRINT legal="<<legal<<" steps="<<c.steps.size()<<" actual_ally_exits_after="<<f.ally_exits_after(pm,6)<<" ally_box_cost="<<f.bot.ally_box_cost(pm)<<" score="<<c.score<<" safe="<<c.safe<<"\n";
        }
        require(!candidates.empty());
        std::cout<<"PEARL_SPRINT_CANDIDATES="<<candidates.size()<<"\n";
    }
    {
        Fixture f; f.ct.unit_count=1; f.window({5,5},Direction::EAST); f.own({{5,5},{4,5},{3,5}});
        f.part({6,5},1,unswbc::Team::B,Direction::WEST,true); f.part({7,5},1,unswbc::Team::B,Direction::WEST);
        f.part({5,8},9,unswbc::Team::B,Direction::SOUTH,true); f.part({4,8},9,unswbc::Team::B,Direction::EAST);
        f.prepare(); PostMove pm; bool safe_alternative=f.bot.simulate_path({Direction::NORTH},pm,false);
        std::ostringstream protocol; auto *old=std::cout.rdbuf(protocol.rdbuf()); bool attacks=f.bot.play_queen_kill(); std::cout.rdbuf(old);
        require(safe_alternative && !attacks && protocol.str().empty());
        f.ct.unit_count=2; old=std::cout.rdbuf(protocol.rdbuf()); attacks=f.bot.play_queen_kill(); std::cout.rdbuf(old); require(attacks);
        std::cout<<"LAST_UNIT safe_alternative="<<safe_alternative<<" alive_own="<<f.ct.unit_count<<" seen_enemy_heads="<<f.bot.heads_.size()<<" queen_attack="<<attacks<<" action="<<protocol.str();
    }
    std::cout << "PASS " << checks << " safety checks\n";
}
