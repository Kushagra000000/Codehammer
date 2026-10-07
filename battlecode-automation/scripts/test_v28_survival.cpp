// Consumed-pearl survival/plan regression, count restoration, and existing reach guard.
// clang++ -std=c++20 -O2 battlecode-automation/scripts/test_v28_survival.cpp -o /tmp/test_v28_survival
#include "../../bots/New/current/bot-v28.0.0-Kushagra/helper.hpp"
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
#include "../../bots/New/current/bot-v28.0.0-Kushagra/main.cpp"
#undef main
#undef private


int main() {
 unswbc::Game game(11,11,64);
 unswbc::Controller ct(100,unswbc::Team::A,Direction::WEST,{},64);
 unswbc::game=&game; unswbc::ct=&ct; game.round_num=30;
 ct.length=2;ct.head.position=Position(5,6);
 ReplayDerivedBot bot(ct,game);
 for(auto &m:bot.memory_) {m.seen=true;m.last_seen=30;for(auto &e:m.edges)e={true,EdgeType::KELP,-1};}
 auto edge=[&](Position p,Direction d){
   bot.cell(p).edges[direction_index(d)]={true,EdgeType::EMPTY,-1};
   bot.cell(p.add_dir(d)).edges[direction_index(d.get_opposite())]={true,EdgeType::EMPTY,-1};
 };
 edge(Position(5,5),Direction::EAST);edge(Position(6,5),Direction::SOUTH);
 edge(Position(6,6),Direction::WEST);edge(Position(5,6),Direction::NORTH);
 bot.cell(Position(5,5)).pearl=true;
 bot.own_trail_={Position(5,6),Position(6,6)};
 bot.known_body_=2;
 bot.own_idx_[bot.index(Position(5,6))]=0;
 bot.own_idx_[bot.index(Position(6,6))]=1;
 bot.nbr_all_dirty_=true;bot.refresh_neighbours();bot.prepare_survival();
 auto run=[&](){bot.start_turn_clock(ReplayDerivedBot::clock_now());bot.search_deadline_=std::numeric_limits<long long>::max()/4;PostMove pm;pm.path={Position(5,5)};pm.head=Position(5,5);pm.length=3;pm.eaten=1;pm.steps=1;return bot.survival(pm,Direction::NORTH);};
 auto a=run();
 PostMove tested; tested.path={Position(5,5)};tested.head={5,5};tested.length=3;tested.steps=1;tested.eaten=1;
 bool planned=bot.plan_safe(tested,Direction::NORTH,1);
 bool restored=std::all_of(bot.surv_eaten_.begin(),bot.surv_eaten_.end(),[](int n){return n==0;});
 { auto one=bot.consumed_pearls(tested); auto two=bot.consumed_pearls(tested);
   if(bot.surv_eaten_[bot.index({5,5})]!=2) return 1; }
 restored=restored && bot.surv_eaten_[bot.index({5,5})]==0;
 bot.cell(Position(5,5)).pearl=false;bot.prepare_survival();auto b=run();bool planned_clean=bot.plan_safe(tested,Direction::NORTH,1);
 printf("real bot memory: depth=%d target=%d complete=%d ok=%d; consumed pearl removed: depth=%d target=%d complete=%d ok=%d\n",a.depth,a.target,a.complete,a.ok(),b.depth,b.target,b.complete,b.ok());
 
 unswbc::Game tg(11,11,64);unswbc::Controller tc(0,unswbc::Team::A,Direction::NORTH,{},64);
 unswbc::game=&tg;unswbc::ct=&tc;tc.length=6;tc.head.position=Position(5,5);
 ReplayDerivedBot threat(tc,tg);threat.valuable_now_=true;
 for(auto &m:threat.memory_)for(auto &e:m.edges)e={true,EdgeType::EMPTY,-1};
 threat.own_trail_={Position(5,5),Position(5,6),Position(5,7),Position(5,8),Position(5,9),Position(5,10)};
 threat.known_body_=6;
 for(int i=0;i<6;++i)threat.own_idx_[threat.index(threat.own_trail_[i])]=i;
 threat.heads_.push_back(SeenHead{1,unswbc::Team::B,Position(3,5),Direction::EAST,3,3,false,false});
 threat.refresh_neighbours();threat.build_reach_threats();
 int hard=threat.hard_threat_[threat.index(Position(5,5))];
 printf("occupied queen head in length3 enemy two-step reach: hard_threat=%d (expected1)\n",hard);
 return (planned==planned_clean && restored && a.depth==b.depth && a.ok()==b.ok() && b.ok() && hard==1)?0:1;
}
