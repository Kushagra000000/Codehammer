// Deterministic safety/coordination fixtures; compile outside the bot project.
#ifndef BOT_HELPER
#define BOT_HELPER "../../bots/candidate/bot-v10/helper.hpp"
#endif
#include BOT_HELPER
#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <unordered_set>
#include <cassert>
#define private public
#define main battlecode_program_main
#ifndef BOT_SOURCE
#define BOT_SOURCE "../../bots/candidate/bot-v10/main.cpp"
#endif
#include BOT_SOURCE
#undef main
#undef private

int main() {
    unswbc::Game game(20,20,64);
    unswbc::game=&game;
    std::vector<unswbc::Tile> tiles;
    for(int y=0;y<20;++y)for(int x=0;x<20;++x)tiles.emplace_back(Position(x,y),std::nullopt,-1,false);
    unswbc::Controller ct(100,unswbc::Team::A,Direction::EAST,unswbc::Vision(tiles),64);
    unswbc::ct=&ct;ct.unit_count=20;ct.length=3;ct.head.position=Position(5,5);
    ReplayDerivedBot bot(ct,game);bot.remember_vision();
    bot.founder_=true;game.round_num=249;assert(!bot.retaining_length());
    game.round_num=250;assert(bot.retaining_length());
    bot.founder_=false;
    while(bot.selected_grower())++ct.head.dragon_id;
    game.round_num=450;assert(!bot.retaining_length());
    bot.signals_.push_back({1,Position(7,5),20,450,true});
    assert(bot.reserved_food(Position(8,5)));
    ct.length=21;assert(!bot.reserved_food(Position(8,5)));ct.length=3;

    ct.get_tile(Position(5,5))->dragon_part=ct.head;
    ct.get_tile(Position(4,5))->dragon_part=unswbc::DragonPart(Position(4,5),ct.get_id(),ct.get_team(),Direction::EAST,false);
    ct.get_tile(Position(7,5))->dragon_part=unswbc::DragonPart(Position(7,5),1,ct.get_team(),Direction::EAST,true);
    bot.feeder_target_=bot.signals_[0];
    std::ostringstream captured;auto old=std::cout.rdbuf(captured.rdbuf());
    assert(bot.play_feeder_strategy());assert(captured.str().find("MOVE W")!=std::string::npos);
    // Own neck replaced with a friendly head must never be a sacrifice target.
    ct.get_tile(Position(4,5))->dragon_part->is_dragon_head=true;
    assert(!bot.play_feeder_strategy());
    ct.get_tile(Position(4,5))->dragon_part->is_dragon_head=false;
    // No recipient means no suicide, even with an old position cached.
    ct.get_tile(Position(7,5))->dragon_part.reset();assert(!bot.play_feeder_strategy());
    ct.get_tile(Position(7,5))->dragon_part=unswbc::DragonPart(Position(7,5),1,ct.get_team(),Direction::EAST,true);
    for(auto& e:ct.get_tile(Position(7,5))->edges)e.edge_type=EdgeType::KELP;
    assert(!bot.play_feeder_strategy());
    for(auto& e:ct.get_tile(Position(7,5))->edges)e.edge_type=EdgeType::EMPTY;
    bot.heads_.push_back({2,unswbc::Team::B,Position(5,7),Direction::NORTH,8});
    assert(!bot.play_feeder_strategy());bot.heads_.clear();

    bot.territory_={ct.get_id(),30,450,2,2,8,10};bot.build_patrol();
    assert(bot.patrol_.size()==80);
    std::unordered_set<int> unique;
    for(std::size_t i=0;i<bot.patrol_.size();++i){
        assert(unique.insert(bot.index(bot.patrol_[i])).second);
        assert(bot.distance(bot.patrol_[i],bot.patrol_[(i+1)%80])==1);
    }
    // Packet round-trip, duplicate suppression, team separation and expiry.
    auto packet=bot.claim_message();auto sender=ct.head.dragon_id;
    ++ct.head.dragon_id;ct.sonar_messages={packet,packet};bot.receive_status();
    assert(bot.claims_.size()==1);assert(bot.claims_.at(sender).w==8);
    ct.sonar_messages={packet^(1ULL<<55U)};bot.claims_.clear();bot.receive_status();assert(bot.claims_.empty());
    ct.sonar_messages={packet};bot.receive_status();game.round_num+=13;ct.sonar_messages.clear();bot.receive_status();assert(bot.claims_.empty());
#ifdef V10_1
    game.round_num=470;
    bot.enemy_report_=TeamSignal{777,Position(12,13),25,469,false};
    auto enemy_packet=bot.enemy_message();bot.enemy_report_.reset();
    ct.sonar_messages={enemy_packet};bot.receive_status();
    assert(bot.enemy_report_ && bot.enemy_report_->id==777 && bot.enemy_report_->round==469);
    assert(bot.enemy_report_->position==Position(12,13));
    // Forwarding does not refresh an old observation's lifetime.
    assert(((bot.enemy_message()>>15U)&511)==469);
    game.round_num=473;ct.sonar_messages.clear();bot.signals_.clear();bot.update_coordination();
    assert(!bot.enemy_report_);
    bot.signals_={{1,Position(7,5),30,473,true}};bot.founder_=true;
    assert(!bot.retaining_length()); // even an undersized founder yields to a real grower
    bot.feeder_target_=TeamSignal{1,Position(7,5),30,470,true};
    assert(!bot.play_feeder_strategy()); // stale recipient report
    bot.memory_.assign(bot.memory_.size(),CellMemory{});
    bot.share_terrain({1,30,473,2,2,8,10});
    assert(bot.known_clear_edge(Position(3,3),Direction::EAST));
    assert(!bot.known_clear_edge(Position(2,2),Direction::WEST)); // boundary never guessed
    ct.length=12;bot.signals_.clear();bot.remember_vision();bot.own_trail_.clear();
    for(int x=5;x<17;++x)bot.own_trail_.push_back(Position(x,5));
    assert(bot.play_grower_escape({}));
    assert(captured.str().find("SPLIT 10")!=std::string::npos);
    // If the tail exits are blocked, transferring length cannot save the child.
    for(auto& edge:ct.get_tile(Position(16,5))->edges)edge.edge_type=EdgeType::KELP;
    assert(!bot.play_grower_escape({}));
    // The emergency path must prefer self-death to killing an allied head.
    ct.length=3;game.round_num=100;ct.sonar_messages.clear();
    ct.get_tile(Position(5,4))->dragon_part=unswbc::DragonPart(Position(5,4),1,ct.get_team(),Direction::SOUTH,true);
    ct.get_tile(Position(6,5))->dragon_part=unswbc::DragonPart(Position(6,5),2,unswbc::Team::B,Direction::WEST,false);
    ct.get_tile(Position(5,6))->dragon_part=unswbc::DragonPart(Position(5,6),3,ct.get_team(),Direction::NORTH,false);
    ct.get_tile(Position(4,5))->dragon_part=unswbc::DragonPart(Position(4,5),ct.get_id(),ct.get_team(),Direction::EAST,false);
    ReplayDerivedBot trapped(ct,game);captured.str("");trapped.play_turn();
    assert(captured.str().find("TRAPPED_SELF")!=std::string::npos);
    assert(captured.str().find("MOVE W")!=std::string::npos);
    ct.length=20;game.round_num=400;
    ReplayDerivedBot claimant(ct,game);claimant.remember_vision();
    claimant.claims_[1]={1,40,400,2,2,8,10};
    claimant.map_territory();assert(claimant.territory_.w==0);
    claimant.claims_.clear();claimant.map_territory();
    assert(claimant.territory_.w>=3 && claimant.territory_.h>=4);
#ifdef V10_R4
    assert(claimant.territory_.w*claimant.territory_.h<=80);
#endif
    std::vector<Candidate> attacks;claimant.add_tactical_sprints(attacks);
    assert(attacks.empty()); // never spend a length-20 grower on a head trade
    ct.length=3;game.round_num=450;
    while(claimant.selected_grower())++ct.head.dragon_id;
    ct.get_tile(Position(6,5))->dragon_part.reset();
    ct.get_tile(Position(7,5))->dragon_part=unswbc::DragonPart(Position(7,5),777,unswbc::Team::B,Direction::WEST,true);
    claimant.add_tactical_sprints(attacks);assert(!attacks.empty());
    for(auto const& attack:attacks)assert(attack.steps.size()==2 && attack.head_trade);
    ct.get_tile(Position(7,5))->dragon_part->team=ct.get_team();
    attacks.clear();claimant.add_tactical_sprints(attacks);assert(attacks.empty());
#endif
    std::cout.rdbuf(old);std::cout<<"v10 coordination safety fixtures passed\n";
}
