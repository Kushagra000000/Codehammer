// Focused real-bot feeding/army-escape regression fixtures.
// clang++ -std=c++20 -O2 battlecode-automation/scripts/test_v28_feeding.cpp -o /tmp/test_v28_feeding
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
#include <sstream>
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

int checks=0, failures=0;
void check(bool pass, char const *name) {
    ++checks; failures += !pass;
    std::cout << (pass?"PASS ":"FAIL ") << name << '\n';
}
struct Fixture {
    unswbc::Game game{16,16,64};
    unswbc::Controller ct;
    ReplayDerivedBot bot;
    Fixture(bool team_b=false, Position head=Position(8,8), Position king=Position(8,6))
      : ct(team_b?101:100,team_b?unswbc::Team::B:unswbc::Team::A,Direction::EAST,{},64),bot(ct,game) {
        unswbc::game=&game; unswbc::ct=&ct;
        game.round_num=450; ct.head.position=head; ct.length=2; ct.unit_count=8;
        bot.retaining_now_=false; bot.first_round_=1;
        for(auto &m:bot.memory_) {m.seen=true; m.last_seen=450; for(auto&e:m.edges)e={true,EdgeType::EMPTY,-1};}
        bot.nbr_all_dirty_=true; bot.refresh_neighbours();
        bot.own_trail_={head,head.add_dir(Direction::WEST)}; bot.known_body_=2;
        bot.own_idx_.assign(256,-1); bot.other_occ_.assign(256,0); bot.hard_threat_.assign(256,0);
        bot.own_idx_[bot.index(head)]=0; bot.own_idx_[bot.index(bot.own_trail_[1])]=1;
        int kid=team_b?1:0;
        bot.king_={kid,16,king,450,true}; bot.king_head_estimate_=king;
        bot.feeder_target_=TeamSignal{kid,king,16,450,true};
        bot.heads_.push_back(SeenHead{kid,ct.get_team(),king,Direction::EAST,16,16,false,true});
        bot.other_occ_[bot.index(king)]=1;
        std::vector<unswbc::Tile> tiles;
        for(int dy=-3;dy<=3;++dy) for(int dx=-3;dx<=3;++dx) {
            Position p(bot.wrap(head.x+dx,16),bot.wrap(head.y+dy,16));
            tiles.emplace_back(p,std::nullopt,-1,false);
            if(p==head)tiles.back().dragon_part=ct.head;
            else if(p==bot.own_trail_[1])tiles.back().dragon_part=unswbc::DragonPart(p,ct.get_id(),ct.get_team(),Direction::EAST,false);
            else if(p==king)tiles.back().dragon_part=unswbc::DragonPart(p,kid,ct.get_team(),Direction::EAST,true);
        }
        ct.vision=unswbc::Vision(std::move(tiles));
    }
    void wall(Position p,Direction d) {
        bot.cell(p).edges[direction_index(d)]={true,EdgeType::KELP,-1};
        bot.cell(p.add_dir(d)).edges[direction_index(d.get_opposite())]={true,EdgeType::KELP,-1};
        bot.nbr_all_dirty_=true;bot.refresh_neighbours();
    }
    bool feed() {
        std::ostringstream output; auto*old=std::cout.rdbuf(output.rdbuf());
        bool result=bot.play_feeder_strategy(); std::cout.rdbuf(old); return result;
    }
};
int main() {
    for(bool b:{false,true}) {
        Fixture f(b);
        check(f.bot.collectible_feed_drops()==1,"length-two donor has one collectible drop, both teams");
        check(f.feed(),"length-two donor feeds elected queen, both teams");
        f.ct.unit_count=3; check(!f.feed(),"three units retain donor");
        f.ct.unit_count=4; check(f.feed(),"four units permit useful donation");
        f.ct.head.dragon_id=b?1:0;check(!f.feed(),"queen never becomes donor");
    }
    {
        Fixture f; f.game.round_num=498; check(f.bot.collectible_feed_drops()==0&&!f.feed(),"no time to collect distant death drop");
        f.game.round_num=497;check(f.feed(),"two remaining collection turns suffice");
        f.game.round_num=399;check(!f.feed(),"no sacrifice before sacrifice phase");
    }
    {
        Fixture f;f.wall(Position(8,6),Direction::SOUTH);f.game.round_num=497;
        check(f.bot.good_sacrifice_spot(),"legacy proximity test permits a longer kelp detour");
        check(f.bot.collectible_feed_drops()==0&&!f.feed(),"actual route length rejects donation beyond final-round deadline");
        f.game.round_num=495;check(f.feed(),"same detour is useful with enough collection time");
    }
    {
        Fixture f;f.ct.length=4; f.bot.own_trail_.push_back(Position(6,8));f.bot.own_trail_.push_back(Position(5,8));f.bot.known_body_=4;
        check(f.bot.collectible_feed_drops()==2,"death drops use even body indices");
        f.ct.get_tile(f.ct.get_position())->pearl=true;
        check(f.bot.collectible_feed_drops()==1,"already existing pearl does not count as new gain");
        f.bot.known_body_=2;
        check(f.bot.collectible_feed_drops()==0&&!f.feed(),"unknown body and odd neck cannot manufacture collectible drops");
    }
    {
        Fixture f(false,Position(0,8),Position(14,8));
        check(f.feed(),"wrapped map path reaches donor's actual drop");
    }
    {
        Fixture f; for(Direction d:Direction::get_direction_list())f.wall(f.ct.get_position(),d);
        check(f.bot.collectible_feed_drops()==0&&!f.feed(),"sealed drop is not collectible");
        int const c=f.bot.index(f.ct.get_position());
        int const north=f.bot.index(f.ct.get_position().add_dir(Direction::NORTH));
        f.bot.nbr_[4*north+direction_index(Direction::SOUTH)]=c;
        f.bot.nbr_portal_[4*north+direction_index(Direction::SOUTH)]=1;
        check(f.bot.collectible_feed_drops()==0,"portal-only donation is rejected consistently with queen policy");
    }
    {
        Fixture f;f.bot.heads_.push_back(SeenHead{201,unswbc::Team::B,Position(8,9),Direction::NORTH,3,3,false,false});
        check(!f.feed(),"nearby enemy veto remains intact");
        f.bot.heads_.pop_back();f.bot.retaining_now_=true;check(!f.feed(),"grower or king does not feed");
    }
    {
        Fixture f;f.game.round_num=420;f.ct.length=8; f.bot.known_body_=0;f.bot.own_trail_.clear();
        for(int id=2;id<200;++id) {f.ct.head.dragon_id=id;if(f.bot.army_worker())break;}
        check(f.bot.army_worker(),"fixture selects real late-army worker");
        check(!f.feed(),"late-army worker never feeds");
        std::ostringstream output;auto*old=std::cout.rdbuf(output.rdbuf());
        bool escaped=f.bot.play_split_escape({});std::cout.rdbuf(old);
        check(escaped&&output.str().find("SPLIT 6")!=std::string::npos,"army worker uses blind life-saving escape despite nearby king");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
