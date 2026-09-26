#ifndef BOT_HELPER
#define BOT_HELPER "../../bots/candidate/bot-v11/helper.hpp"
#endif
#include BOT_HELPER
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cassert>
#define private public
#define main battlecode_program_main
#ifndef BOT_SOURCE
#define BOT_SOURCE "../../bots/candidate/bot-v11/main.cpp"
#endif
#include BOT_SOURCE
#undef main
#undef private
int main() {
    unswbc::Game game(20,20,64);unswbc::game=&game;
    std::vector<unswbc::Tile> tiles;
    for(int y=0;y<20;++y)for(int x=0;x<20;++x)tiles.emplace_back(Position(x,y),std::nullopt,-1,false);
    unswbc::Controller ct(100,unswbc::Team::A,Direction::EAST,unswbc::Vision(tiles),64);unswbc::ct=&ct;
    ct.unit_count=20;ct.length=4;ct.head.position=Position(5,5);
    ReplayDerivedBot bot(ct,game);bot.remember_vision();
    for(int i=0;i<4;++i)ct.get_tile(Position(5-i,5))->dragon_part=unswbc::DragonPart(Position(5-i,5),ct.get_id(),ct.get_team(),Direction::EAST,i==0);
    bot.update_body_memory();assert(bot.own_trail_.size()==4);assert(bot.own_trail_.back()==Position(2,5));
    // Tail collision before movement: unlike most ordinary Snake implementations.
    ReplayDerivedBot::VirtualBody v{{Position(5,5),Position(5,6),Position(4,6),Position(4,5)}, {}};
    assert(!bot.virtual_step(v,Direction::WEST));
    // Consumed pearl grows once; the same coordinate cannot grow repeatedly.
    v={bot.own_trail_,{}};ct.get_tile(Position(6,5))->pearl=true;
    assert(bot.virtual_step(v,Direction::EAST));assert(v.body.size()==5);
    assert(bot.virtual_step(v,Direction::NORTH));assert(v.body.size()==5);
    // Sprint history records intermediate turns, not just its endpoint.
    bot.remember_action({Direction::EAST,Direction::NORTH,Direction::WEST});
    ct.head.position=Position(5,4);ct.length=3;bot.update_body_memory();
    assert(bot.own_trail_.size()==3 && bot.own_trail_[1]==Position(6,4) && bot.own_trail_[2]==Position(6,5));
    for(auto& t:ct.vision.tiles)t.dragon_part.reset();
    ct.length=20;bot.territory_={ct.get_id(),20,300,1,1,8,10};bot.build_patrol();
    assert(bot.patrol_.size()==80);std::unordered_set<int> seen;
    for(int i=0;i<80;++i){assert(seen.insert(bot.index(bot.patrol_[i])).second);assert(bot.distance(bot.patrol_[i],bot.patrol_[(i+1)%80])==1);}
    bot.own_trail_.clear();for(int i=0;i<20;++i)bot.own_trail_.push_back(bot.patrol_[(40-i+80)%80]);
    ct.head.position=bot.own_trail_.front();assert(bot.cycle_ordered());assert(bot.cycle_step(bot.patrol_[41]));
    assert(!bot.cycle_step(bot.own_trail_.back()));
    // Hundreds of cycle steps preserve the invariant; this catches wraparound
    // and return-column bugs that one adjacency check would miss.
    for(auto& tile:ct.vision.tiles)tile.pearl=false;
    for(int turn=0;turn<500;++turn) {
        assert(bot.cycle_ordered());
        auto it=std::find(bot.patrol_.begin(),bot.patrol_.end(),bot.own_trail_.front());
        Position next=bot.patrol_[(it-bot.patrol_.begin()+1)%bot.patrol_.size()];
        assert(bot.cycle_step(next));
        bool moved=false;ReplayDerivedBot::VirtualBody vb{bot.own_trail_,{}};
        for(auto dir:Direction::get_direction_list())if(bot.step_from(vb.body.front(),dir).destination==next) {assert(bot.virtual_step(vb,dir));moved=true;break;}
        assert(moved);bot.own_trail_=vb.body;ct.head.position=vb.body.front();
    }
    std::swap(bot.own_trail_[5],bot.own_trail_[6]);assert(!bot.cycle_ordered());
    // Center strategy stops by 25 and on a completely observed barren center.
    ct.length=3;game.round_num=10;bot.center_done_=false;
    bot.distribution_score(Position(5,5),Position(6,5),INF);assert(bot.center_done_);
    bot.center_done_=false;game.round_num=25;bot.distribution_score(Position(5,5),Position(6,5),INF);assert(bot.center_done_);
    // Full terrain packet round trip, expiry, and preservation of observation time.
    game.round_num=100;auto packet=bot.terrain_message();
    ReplayDerivedBot receiver(ct,game);receiver.receive_terrain(packet);
    Position p(int((packet>>53)&63),int((packet>>47)&63));assert(receiver.cell(p).edges[0].known);assert(receiver.cell(p).pearl_sample_round==100);
    game.round_num=113;ReplayDerivedBot stale(ct,game);stale.receive_terrain(packet);assert(!stale.cell(p).edges[0].known);
#ifdef NEW_SONAR_TAGS
    // Legacy C8/CA/CB messages are not claims in the new protocol namespace.
    ct.head.team=unswbc::Team::B;game.round_num=8;
    std::uint64_t legacy=(0xC8ULL<<56)|(8ULL<<41)|(10ULL<<47)|(20ULL<<19)|(2ULL<<29)|(2ULL<<35)|(101ULL<<7);
    ct.sonar_messages={legacy};ReplayDerivedBot isolated(ct,game);isolated.receive_status();assert(isolated.claims_.empty());
    ct.head.team=unswbc::Team::A;ct.sonar_messages.clear();
#endif
    // Share a real kelp edge, portal ID, current pearl and spawn countdown.
    game.round_num=100;
    auto* sampled=ct.get_tile(p);sampled->pearl=true;sampled->pearl_time=17;
    sampled->edges[0].edge_type=EdgeType::KELP;
    sampled->edges[1].edge_type=EdgeType::PORTAL;sampled->edges[1].portal_id=1234;
    auto terrain=bot.terrain_message();ReplayDerivedBot geometry(ct,game);geometry.receive_terrain(terrain);
    assert(geometry.cell(p).edges[0].type==EdgeType::KELP);
    assert(geometry.cell(p).edges[1].portal_id==1234);
    assert(geometry.cell(p).pearl && geometry.cell(p).pearl_time==17);
    sampled->edges[0].edge_type=EdgeType::EMPTY;sampled->edges[1].edge_type=EdgeType::EMPTY;sampled->edges[1].portal_id.reset();sampled->pearl=false;
    // An immediately threatened long snake can pay for a multi-step escape.
    ct.length=20;ct.head.position=Position(5,10);bot.patrol_.clear();bot.own_trail_.clear();
    for(int i=0;i<20;++i)bot.own_trail_.push_back(Position((25-i)%20,10));
    for(auto d:Direction::get_direction_list())bot.enemy_threats_[bot.index(ct.head.position.add_dir(d))]=1;
    std::vector<Candidate> dodges;bot.improve_navigation(dodges);assert(!dodges.empty());
    for(auto const& dodge:dodges) {
        assert(dodge.steps.size()>=2 && dodge.steps.size()<=5);
        ReplayDerivedBot::VirtualBody simulated{bot.own_trail_,{}};
        for(auto d:dodge.steps)assert(bot.virtual_step(simulated,d));
        assert(!bot.enemy_threats_[bot.index(simulated.body.front())]);
    }
#ifdef OPENING_TRADES
    game.round_num=10;ct.length=3;ct.unit_count=2;ct.head.position=Position(5,10);
    ct.get_tile(Position(6,10))->dragon_part=unswbc::DragonPart(Position(6,10),999,unswbc::Team::B,Direction::WEST,true);
    assert(bot.evaluate_single(Direction::EAST).head_trade);
    ct.unit_count=1;assert(!bot.evaluate_single(Direction::EAST).head_trade);
    ct.unit_count=20;ct.length=20;assert(!bot.evaluate_single(Direction::EAST).head_trade);
    ct.get_tile(Position(6,10))->dragon_part.reset();
#endif
    std::cout<<"v11 body reconstruction, sprint history, tail collision, cycle ordering, distribution and terrain fixtures passed\n";
}
