// Included in ReplayDerivedBot. Bounded virtual-body search, not a global
// Hamiltonian guarantee: enemies and unobserved occupancy remain uncertain.
std::vector<Position> pending_path_;
std::vector<bool> body_occupancy_;
mutable bool center_done_ = false;

void update_body_memory() {
    Position here = controller_.get_position();
    if (!pending_path_.empty()) {
        if (pending_path_.back() == here) {
            for (auto p : pending_path_) own_trail_.push_front(p);
        } else own_trail_.clear(); // unexpected/unknown portal; never invent a body
        pending_path_.clear();
    }
    if (own_trail_.empty() || !(own_trail_.front() == here)) own_trail_.push_front(here);
    while (own_trail_.size() > static_cast<size_t>(controller_.get_length())) own_trail_.pop_back();
    // Split children start without history. Reconstruct the connected visible
    // prefix using each segment's direction toward its predecessor, including portals.
    while (own_trail_.size() < static_cast<size_t>(controller_.get_length())) {
        std::optional<Position> next;
        for (auto const& t : controller_.get_tiles()) {
            auto part=t.get_dragon();
            if (!part || part->is_head() || part->get_id()!=controller_.get_id()) continue;
            auto p=t.get_position();
            if (std::find(own_trail_.begin(),own_trail_.end(),p)!=own_trail_.end()) continue;
            auto s=step_from(p,part->get_dir());
            if (s.passable && s.destination_known && s.destination==own_trail_.back()) {next=p;break;}
        }
        if (!next) break;
        own_trail_.push_back(*next);
    }
    body_occupancy_.assign(memory_.size(),false);
    for(auto p:own_trail_)body_occupancy_[index(p)]=true;
}
void remember_action(std::vector<Direction> const& moves) {
    pending_path_.clear();
    auto p=controller_.get_position();
    for (auto d:moves) {
        auto s=step_from(p,d);
        if (!s.passable || !s.destination_known) {pending_path_.clear();own_trail_.clear();return;}
        pending_path_.push_back(s.destination);p=s.destination;
    }
}

int distribution_score(Position from, Position to, int food_distance) const {
    auto center=Position(width_/2,height_/2);
    int sampled=0,productive=0,allies=0;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x) {
        auto const& m=cell(Position(wrap(center.x+x,width_),wrap(center.y+y,height_)));
        if(m.seen) {++sampled;if(m.pearl_time>=0 || m.pearl) ++productive;}
    }
    for(auto const& h:heads_) if(h.team==controller_.get_team() && distance(h.position,center)<=3) ++allies;
    if(game_.get_round_num()>=Config::CENTER_UNTIL || (sampled==9 && !productive) || allies>=2) center_done_=true;
    if(retaining_length())return 0;
    // Food and splitting win over exploration. Delta avoids a permanent basin.
    if(!center_done_ && food_distance>4) return Config::CENTER_WEIGHT*(distance(from,center)-distance(to,center));
    if(food_distance<=6)return 0;
    auto seed=mix(controller_.get_id() ^ (std::uint64_t(game_.get_round_num()/60)<<32));
    Position sector(int(seed%width_),int((seed>>16)%height_));
    return Config::SECTOR_WEIGHT*(distance(from,sector)-distance(to,sector));
}

bool cycle_ordered() const {
    if(patrol_.size()<static_cast<size_t>(controller_.get_length()+4) ||
       own_trail_.size()!=static_cast<size_t>(controller_.get_length()))return false;
    std::vector<int> rank(memory_.size(),-1);
    for(size_t i=0;i<patrol_.size();++i)rank[index(patrol_[i])]=int(i);
    int head=rank[index(own_trail_.front())],last=-1,n=int(patrol_.size());
    if(head<0)return false;
    for(auto p:own_trail_) {
        int r=rank[index(p)];if(r<0)return false;
        int gap=(head-r+n)%n;
        if(gap<=last)return false;
        last=gap;
    }
    return true;
}
bool cycle_step(Position next) const {
    if(!cycle_ordered())return false;
    auto rank=[&](Position p) {auto it=std::find(patrol_.begin(),patrol_.end(),p);return it==patrol_.end()?-1:int(it-patrol_.begin());};
    int h=rank(own_trail_.front()),t=rank(own_trail_.back()),q=rank(next),n=int(patrol_.size());
    if(q<0)return false;
    int advance=(q-h+n)%n,room=(t-h+n)%n;
    return advance>0 && advance<room && (Config::CYCLE_SHORTCUTS || advance==1);
}

struct VirtualBody {std::deque<Position> body;std::vector<int> eaten;};
bool virtual_step(VirtualBody& v, Direction d) const {
    auto s=step_from(v.body.front(),d);
    if(!s.passable || !s.destination_known || !controller_.get_tile(s.destination))return false;
    // The engine checks collisions BEFORE advancing the tail. In particular,
    // stepping onto today's tail is illegal even when it would otherwise vacate.
    if(std::find(v.body.begin(),v.body.end(),s.destination)!=v.body.end())return false;
    auto part=visible_part(s.destination);
    bool complete=own_trail_.size()==static_cast<size_t>(controller_.get_length());
    if(part && (part->get_id()!=controller_.get_id() || !complete))return false;
    int i=index(s.destination);
    bool pearl=visible_pearl(s.destination) && std::find(v.eaten.begin(),v.eaten.end(),i)==v.eaten.end();
    if(pearl)v.eaten.push_back(i);
    v.body.push_front(s.destination);
    if(!pearl)v.body.pop_back();
    // Sprint's extra tail cost is deliberately NOT used to clear a route. This
    // conservative superset is safe under either point at which cost is charged.
    return true;
}
int continuation(VirtualBody const& v,int remaining,int& budget) const {
    if(!remaining || budget--<=0)return 0;
    int best=0;
    for(auto d:Direction::get_direction_list()) {
        VirtualBody next=v;
        if(!virtual_step(next,d))continue;
        best=std::max(best,1+continuation(next,remaining-1,budget));
        if(best==remaining)break;
    }
    return best;
}
void improve_navigation(std::vector<Candidate>& candidates) {
    if(controller_.get_length()<8)return;
    if(!cycle_ordered() && !patrol_.empty()) {
        std::reverse(patrol_.begin(),patrol_.end());
        if(!cycle_ordered())std::reverse(patrol_.begin(),patrol_.end());
    }
    bool ordered=cycle_ordered();
    int best_horizon=0;
    for(auto& c:candidates) {
        if(c.head_trade || !c.destination_known)continue;
        VirtualBody v{own_trail_,{}};
        int budget=100,horizon=0;
        if(virtual_step(v,c.steps[0])) horizon=1+continuation(v,Config::LOOKAHEAD_DEPTH-1,budget);
        best_horizon=std::max(best_horizon,horizon);
        c.score-= (Config::LOOKAHEAD_DEPTH-horizon)*6000;
        if(horizon<Config::LOOKAHEAD_DEPTH)c.safe=false;
        if(ordered && cycle_step(c.destination) && c.enemy_threats==0 && c.ally_threats==0 && horizon==Config::LOOKAHEAD_DEPTH) {
            c.score=30000 + (c.direct_pearl ? 1000 : 0);c.safe=true; // cycle invariant overrides misleading static flood space
        }
    }
    bool ordinary_safe=std::any_of(candidates.begin(),candidates.end(),[](auto const& c){return c.safe;});
    if(ordinary_safe || own_trail_.size()!=static_cast<size_t>(controller_.get_length()))return;
    // Search only when ordinary navigation has no safe answer. At most 4^5
    // short paths, bounded independently of map size and dragon count.
    std::vector<Direction> path;int budget=300;
    auto search=[&](auto&& self,VirtualBody const& v,int depth)->void {
        if(depth>=Config::MAX_DODGE_STEPS || budget--<=0)return;
        for(auto d:Direction::get_direction_list()) {
            VirtualBody next=v;if(!virtual_step(next,d))continue;
            path.push_back(d);int moves=int(path.size());auto dest=next.body.front();
            if(moves>=2 && controller_.get_length()-moves+1>=4 &&
               enemy_threats_[index(dest)]==0 && ally_threats_[index(dest)]==0) {
                int continuation_budget=35;
                int horizon=continuation(next,Config::LOOKAHEAD_DEPTH,continuation_budget);
                if(horizon==Config::LOOKAHEAD_DEPTH) {
                    Candidate c;c.steps=path;c.destination=dest;c.safe=true;c.exits=1;c.space=controller_.get_length()+4;
                    c.score=12000-900*(moves-1);c.direct_pearl=!next.eaten.empty();
                    candidates.push_back(c);
                }
            }
            if(moves<Config::MAX_DODGE_STEPS)self(self,next,depth+1);
            path.pop_back();
        }
    };
    search(search,VirtualBody{own_trail_,{}},0);
}

std::uint64_t terrain_message() const {
    auto const& tiles=controller_.get_tiles();
    if(tiles.empty())return 0;
    auto const& t=tiles[(game_.get_round_num()*17+controller_.get_id())%tiles.size()];
    auto p=t.get_position();
    std::uint64_t m=0x7ULL<<60;
    m|=std::uint64_t(controller_.get_team().value==unswbc::Team::B)<<59;
    m|=std::uint64_t(p.x)<<53;m|=std::uint64_t(p.y)<<47;
    m|=std::uint64_t(game_.get_round_num()&511)<<38;
    for(auto d:Direction::get_direction_list()) {
        auto const& e=t.get_edge(d);int kind=e.is_portal()?2:(e.is_passable()?0:1);
        m|=std::uint64_t(kind)<<(30+2*direction_index(d));
        if(e.is_portal() && e.get_portal_id()>=0 && e.get_portal_id()<65536 && !(m&(1ULL<<18))) {
            m|=1ULL<<18;m|=std::uint64_t(direction_index(d))<<16;m|=e.get_portal_id();
        }
    }
    m|=std::uint64_t(t.has_pearl())<<29;
    m|=std::uint64_t(std::clamp(t.get_pearl_time()+1,0,1023))<<19;
    return m;
}
void receive_terrain(std::uint64_t m) {
    Position p(int((m>>53)&63),int((m>>47)&63));
    int round=int((m>>38)&511),now=game_.get_round_num();
    if(!p.is_in_map() || round>now || now-round>12)return;
    auto& c=cell(p);
    for(auto d:Direction::get_direction_list()) {
        int di=direction_index(d),kind=int((m>>(30+2*di))&3);
        auto& edge=c.edges[di];
        if(edge.known)continue; // direct or earlier static knowledge wins
        if(kind==0)edge={true,EdgeType::EMPTY,-1};
        if(kind==1)edge={true,EdgeType::KELP,-1};
        if(kind==2 && (m&(1ULL<<18)) && int((m>>16)&3)==di) {
            edge={true,EdgeType::PORTAL,int(m&65535)};remember_portal(edge.portal_id,canonical_edge(p,d));
        }
        auto& opposite=cell(p.add_dir(d)).edges[direction_index(d.get_opposite())];
        if(edge.known && !opposite.known)opposite=edge;
    }
    if(c.pearl_sample_round<round) {
        c.pearl=bool((m>>29)&1);c.pearl_seen=c.pearl?round:-1;
        c.pearl_time=int((m>>19)&1023)-1;c.pearl_sample_round=round;
    }
}
