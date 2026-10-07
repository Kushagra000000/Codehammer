// Run with -std=c++20 -O1 -fsanitize=undefined -fno-elide-constructors.
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
 {
  Fixture f(4,10,5); f.window({4,4},Direction::EAST); f.own({{4,4},{3,4},{2,4},{1,4},{1,3}});
  f.part({5,5},6,unswbc::Team::A,Direction::NORTH,true); f.part({5,6},6,unswbc::Team::A,Direction::NORTH);
  f.wall({5,5},Direction::WEST); f.wall({5,5},Direction::EAST); f.prepare();
  PostMove p; bool legal=f.bot.simulate_path({Direction::EAST,Direction::NORTH,Direction::EAST,Direction::EAST,Direction::SOUTH},p,false);
  int ix=f.bot.post_index(p,f.bot.index({5,4})); int cost=f.bot.ally_box_cost(p);
  std::cout<<"paid sprint legal="<<legal<<" length="<<p.length<<" vacated_path_cell="<<ix<<" ally_exits="<<f.ally_exits_after(p,6)<<" cost="<<cost<<"\n";
  if(!legal || p.length!=2 || ix!=-1 || cost!=0 || f.ally_exits_after(p,6)!=1) return 1;
 }
 {
  Fixture f; f.window({5,5},Direction::EAST); f.own({{5,5},{4,5},{3,5}}); f.prepare();
  Position pearl{6,5}; int c=f.bot.index(pearl); f.bot.cell(pearl).pearl=true; f.bot.cell(pearl).last_seen=f.game.round_num;
  PostMove p; p.path={pearl,pearl}; p.length=4; p.steps=2;
  f.bot.surv_eaten_[c]=1;
  {
   auto one=f.bot.consumed_pearls(p);
   auto moved=std::move(one);
   if(!one.cells.empty() || f.bot.surv_eaten_[c]!=3) return 2;
   { auto nested=f.bot.consumed_pearls(p); if(f.bot.surv_eaten_[c]!=5) return 3; }
   if(f.bot.surv_eaten_[c]!=3) return 4;
  }
  if(f.bot.surv_eaten_[c]!=1) return 5;
  f.bot.surv_eaten_[c]=0;
  std::cout<<"consumed counts: explicit move, return move, duplicate path, nesting and preexisting route mark restored\n";
 }
}
