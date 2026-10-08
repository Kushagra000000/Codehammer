// Included inside ReplayDerivedBot (before scoring.hpp).
// v21 survival search: after a candidate move, is there a way to keep moving for
// the next few turns? A depth-first search over our own next single moves, played
// as the engine plays them:
//  - the head steps first; a tile holding any segment of ours is fatal, the tail
//    included (the collision check runs before the tail advances);
//  - a pearl we can see on the tile is eaten: we grow and the tail stays this step;
//    otherwise the tail leaves its tile;
//  - kelp blocks, the map wraps, known portals are followed. v26: a portal we have
//    never looked through is a way on (was: a wall), used only when nothing else gets
//    us to the target; the result says so (via_portal) and the move pays the crossing's
//    risk (position_value). On Maze the queen, 4 long in a pocket whose only other exit
//    was such a portal, took it for certain death, split, and died in the dead end;
//  - other dragons' segments are walls until they must have moved on: segment i of
//    a dragon whose head we can see frees after (its length - i + 1) of its moves
//    (each dragon moves once between two of ours; the +1 in case it eats), and never
//    before our second move; a segment whose head we cannot see stays a wall; a body
//    remembered out of sight blocks for a few steps;
//  - a tile another head could step into before our next move is not counted on
//    for our first step (where heads go after that is for the fight search); v21: any
//    tile within that head's free steps.
// The search stops at the target depth (our length + DFS_EXTRA_DEPTH, capped at
// DFS_MAX_DEPTH) or when its node budget runs out. The more open tile is tried
// first, so in open water the first path already reaches the target and the
// search costs little; the budget only bites in tight spots.
// Our own moves are searched one step a turn: we may always go that slowly (free
// steps are optional), and it is the timing that gives other bodies the most turns
// to move on. v21: the same search also checks where a planned pearl route ends
// (survival_from, route.hpp), with a time offset and a budget of its own.

struct SurvivalResult
{
    int depth = -1; // most moves survived on any path (-1: not searched)
    int target = 0;
    bool complete = false; // every path was searched (the budget did not cut it short)
    // v23: a move that fails the search is searched again with only real walls (kelp,
    // bodies, our own body), not the tiles other heads could reach next. Still short:
    // `walled`, certain death. Through then: `contested` (the result is that search's).
    bool walled = false;
    bool contested = false;
    // v26: the way on it found goes through a portal we have never looked through (it
    // cannot see where that leads, so it stops there and counts it as a way on).
    bool via_portal = false;
    bool ok() const { return depth >= 0 && depth >= target; }
    // Searched in full and still short of most of the target: a trap.
    bool dead() const { return depth >= 0 && complete && depth * 4 < target * 3; }
};

// v22: the searched body as a ring buffer (a deque's every push and pop cost more than
// the rest of a search step together).
struct SurvRing
{
    std::vector<int> buf;
    unsigned mask = 0, head = 0, n = 0;
    void reset(std::size_t cap)
    {
        std::size_t p = 64;
        while (p < cap)
            p <<= 1;
        if (buf.size() < p)
            buf.assign(p, 0);
        mask = static_cast<unsigned>(buf.size() - 1);
        head = 0;
        n = 0;
    }
    int front() const { return buf[head]; }
    int back() const { return buf[(head + n - 1) & mask]; }
    void push_front(int c)
    {
        head = (head - 1) & mask;
        buf[head] = c;
        ++n;
    }
    void pop_front()
    {
        head = (head + 1) & mask;
        --n;
    }
    void push_back(int c)
    {
        buf[(head + n) & mask] = c;
        ++n;
    }
    void pop_back() { --n; }
    std::size_t size() const { return n; }
};
mutable SurvRing surv_ring_;

std::vector<int> other_free_at_;                      // per tile: our move from which another dragon's segment has gone (INF: never)
std::vector<int> body_free_at_;                       // v21: the same for bodies only (no head reach, not ours), in their moves
std::vector<int> wall_free_at_;                       // v23: the same without other heads' reach (real walls only)
std::vector<int> plan_free_at_;                       // v21: the king's plans: a tile another head can reach may hold its body
                                                      // for its length (not just for our next move)
mutable std::vector<int> const *surv_free_ = nullptr; // the timings the current search uses
mutable int surv_anchor_ = -1;                        // deep searches: where our known body ends (the unknown rest is attached there)
mutable int surv_offset_ = 0;                         // v21: other dragons' moves already made when the search starts
mutable int *surv_counter_ = nullptr;                 // v21: the per-turn node count in use (moves or routes)
mutable int surv_cap_ = Config::DFS_TURN_BUDGET;
mutable int surv_route_nodes_ = 0;
mutable int surv_plan_nodes_ = 0;
mutable int surv_node_cap_ = Config::DFS_NODE_BUDGET; // v21: per search (deep searches get more)
mutable int surv_inf_ = INF;                          // v21: an unplaceable body frees after this many moves
mutable std::vector<int> surv_occ_;                   // our body during the search
mutable std::vector<int> surv_eaten_;                 // pearls eaten on the current path
mutable int surv_nodes_ = 0;
mutable int surv_turn_nodes_ = 0;
mutable Position surv_anchor_pos_{};
mutable long long surv_deadline_ = 0; // v22: this search's own deadline

// Once per turn, before any candidate is searched.
void prepare_survival()
{
    surv_turn_nodes_ = 0;
    surv_route_nodes_ = 0;
    surv_plan_nodes_ = 0;
    std::size_t const cells = memory_.size();
    other_free_at_.assign(cells, 0);
    body_free_at_.assign(cells, 0);
    if (surv_occ_.size() != cells)
    {
        surv_occ_.assign(cells, 0);
        surv_eaten_.assign(cells, 0);
    }
    for (std::size_t c = 0; c < cells; ++c)
    {
        // v21: a body remembered out of sight, for as long as it will be there.
        if (remembered_occupied(static_cast<int>(c)))
            other_free_at_[c] = body_free_at_[c] = std::max(1, body_rounds_left(static_cast<int>(c)));
        // Our own segments we cannot place in order (never seen since birth): they are
        // in the unknown part of the body, which has gone within this many moves.
        if (own_idx_[c] == -2)
            other_free_at_[c] = std::max(1, controller_.get_length() - known_body_);
    }
    for (unswbc::Tile const &tile : controller_.get_tiles())
    {
        unswbc::DragonPart const *part = tile.get_dragon();
        if (part && part->get_id() != controller_.get_id())
            other_free_at_[static_cast<std::size_t>(index(tile.get_position()))] =
                body_free_at_[static_cast<std::size_t>(index(tile.get_position()))] = INF;
    }
    for (SeenHead const &h : heads_)
    {
        // Segment i has gone after (length - i) of its moves; one more in case it eats
        // (then its tail stays), and never before our second move.
        BodyTrace const trace = trace_body(h.position, h.id);
        int const length = known_length(h); // v21: an ally's reported length, if longer
        for (std::size_t i = 0; i < trace.segments.size(); ++i)
        {
            std::size_t const c = static_cast<std::size_t>(index(trace.segments[i]));
            other_free_at_[c] = body_free_at_[c] = std::max(Config::DFS_OTHER_MIN_WAIT, length - static_cast<int>(i) + 1);
        }
    }
    // Where a head may be before our next move: not ours to count on then (v21:
    // anywhere within its free steps; its body follows it there). Our allies never
    // move more than FREE_MOVE_MAX_STEPS; an ally out of sight that told us where it
    // is (status packet, this round or the last) counts too.
    auto reach_of = [&](bool ally, int length)
    {
        return ally ? std::min(free_moves(length), Config::FREE_MOVE_MAX_STEPS) : free_moves(length);
    };
    plan_free_at_ = other_free_at_;
    wall_free_at_ = other_free_at_; // v23: before any head's reach is marked
    auto mark = [&](std::vector<int> const &tiles, int length)
    {
        int const stay = std::min(Config::BODY_MEMORY_MAX, length + 1);
        for (int c : tiles)
        {
            std::size_t const t = static_cast<std::size_t>(c);
            other_free_at_[t] = std::max(other_free_at_[t], 2);
            plan_free_at_[t] = std::max(plan_free_at_[t], stay);
        }
    };
    for (SeenHead const &h : heads_)
        mark(tiles_within(h.position, h.facing, reach_of(h.team == controller_.get_team(), known_length(h))), known_length(h));
    int const now = game_.get_round_num();
    int const me = my_id();
    for (TeamSignal const &sig : signals_)
    {
        if (sig.id == me || now - sig.round > 1 || seen_head(sig.id))
            continue;
        // Sent last round by a dragon that moves before us: it has moved once since.
        int const moves = sig.round < now && sig.id < me ? 2 : 1;
        mark(tiles_within(sig.position, Direction::NORTH, moves * reach_of(true, sig.length), true), sig.length);
        // Where it was: its body is there until its tail has passed.
        std::size_t const at = static_cast<std::size_t>(index(sig.position));
        int const stay = std::min(Config::BODY_MEMORY_MAX, std::max(2, sig.length - 1));
        other_free_at_[at] = std::max(other_free_at_[at], stay);
        wall_free_at_[at] = std::max(wall_free_at_[at], stay);
        plan_free_at_[at] = std::max(plan_free_at_[at], stay);
        body_free_at_[at] = std::max(body_free_at_[at], stay);
    }
}

// v21: the dragons that search all the way: the king (also before growers start at
// round 300) and growers.
bool deep_dragon() const
{
    return retaining_length() || (king_fresh() && king_.id == my_id());
}

// From where a move leaves us (`pm`, arriving heading `heading`).
SurvivalResult survival(PostMove const &pm, Direction heading) const
{
    // Our body after the move, head first: the new head tiles, then the old body.
    std::deque<int> body;
    for (std::size_t k = pm.path.size(); k-- > 0;)
        body.push_back(index(pm.path[k]));
    for (Position const &p : own_trail_)
        body.push_back(index(p));
    while (static_cast<int>(body.size()) > pm.length)
        body.pop_back();
    int const unknown = pm.length - static_cast<int>(body.size()); // tail segments we cannot place
    SurvivalResult r = survival_from(body, unknown, heading, pm.length, 0, 0, deep_dragon());
    if (!r.dead())
        return r;
    // v23: dead with other heads' next moves counted as taken. Is it walled in, or only
    // contested (a head that may not come)?
    SurvivalResult w = survival_from(body, unknown, heading, pm.length, 0, 0, deep_dragon(), true);
    if (w.depth < 0)
        return r; // not searched (out of nodes or points): as it was
    if (w.dead())
    {
        r.walled = true;
        return r;
    }
    w.contested = true;
    return w;
}

// v21: from any body (head first; `unknown` more segments we cannot place), with the
// other dragons `offset` moves further on than now. `pool`: whose per-turn budget it
// spends (0 the moves', 1 the route checks', 2 the king's plans'). `deep` (the king and
// growers): the target is the whole length + 2 (at most KING_DFS_MAX_DEPTH), with the
// deep budgets.
SurvivalResult survival_from(std::deque<int> body, int unknown, Direction heading, int length, int offset, int pool,
                             bool deep = false, bool walls_only = false) const
{
    SurvivalResult r;
    r.target = std::min(length + Config::DFS_EXTRA_DEPTH, deep ? Config::KING_DFS_MAX_DEPTH : Config::DFS_MAX_DEPTH);
    surv_counter_ = pool == 2 ? &surv_plan_nodes_ : pool == 1 ? &surv_route_nodes_
                                                              : &surv_turn_nodes_;
    surv_cap_ = pool == 2   ? Config::PLAN_DFS_TURN_BUDGET
                : pool == 1 ? (deep ? Config::KING_ROUTE_DFS_TURN_BUDGET : Config::ROUTE_DFS_TURN_BUDGET)
                            : (deep ? Config::KING_DFS_TURN_BUDGET : Config::DFS_TURN_BUDGET);
    surv_node_cap_ = deep ? Config::KING_DFS_NODE_BUDGET : Config::DFS_NODE_BUDGET;
    surv_inf_ = deep ? Config::DEEP_UNPLACED_BODY : INF;
    surv_free_ = walls_only ? &wall_free_at_ : &other_free_at_;
    surv_anchor_ = deep && unknown > 0 && !own_trail_.empty() ? index(own_trail_.back()) : -1;
    if (surv_anchor_ >= 0)
        surv_anchor_pos_ = position_of(surv_anchor_);
    surv_offset_ = offset;
    if (*surv_counter_ >= surv_cap_ || out_of_time())
        return r; // out of nodes or points this turn: not searched
    // v22: one search may spend at most a quarter of what its phase has left (at least
    // SEARCH_MIN_POINTS), so the ones after it (an escape sprint, say) get searched too.
    {
        long long const now = clock_now();
        surv_deadline_ = std::min(search_deadline_, now + std::max(Config::SEARCH_MIN_POINTS, (search_deadline_ - now) / 4));
    }
    for (int c : body)
        ++surv_occ_[static_cast<std::size_t>(c)];
    surv_nodes_ = 0;
    surv_via_portal_ = false; // v26
    int best = 0;
    bool cut = false;
    // v22: searched on a ring buffer (it grows by at most one tile a step).
    surv_ring_.reset(body.size() + static_cast<std::size_t>(r.target) + 8);
    for (int c : body)
        surv_ring_.push_back(c);
#ifdef SURV_CHECK
    {
        // The deque search must agree exactly (no clock cut-offs while checking).
        surv_deadline_ = std::numeric_limits<long long>::max();
        int const counter0 = *surv_counter_;
        int unknown2 = unknown, best2 = 0;
        bool cut2 = false;
        bool const done2 = surv_dfs(body, unknown2, heading, 0, r.target, best2, cut2);
        int const nodes2 = surv_nodes_;
        *surv_counter_ = counter0;
        surv_nodes_ = 0;
        bool const done1 = surv_dfs(surv_ring_, unknown, heading, 0, r.target, best, cut);
        if (done1 != done2 || best != best2 || cut != cut2 || nodes2 != surv_nodes_)
            controller_.output_log("SURVBAD", best, best2, cut, cut2, surv_nodes_, nodes2);
    }
#else
    surv_dfs(surv_ring_, unknown, heading, 0, r.target, best, cut);
#endif
    for (int c : body)
        --surv_occ_[static_cast<std::size_t>(c)];
    r.depth = best;
    r.complete = !cut;
    r.via_portal = surv_via_portal_ && r.ok();
    return r;
}
mutable bool surv_via_portal_ = false; // v26: the search succeeded through an unexplored portal

// True once a path reaches the target.
template <class Body>
bool surv_dfs(Body &body, int &unknown, Direction heading, int depth, int target, int &best, bool &cut) const
{
    best = std::max(best, depth);
    if (depth >= target)
        return true;
    // v22: and the turn's points (the clock, every 128 positions).
    if (surv_nodes_ >= surv_node_cap_ || *surv_counter_ >= surv_cap_ ||
        ((surv_nodes_ & 127) == 127 && clock_now() > surv_deadline_))
    {
        if (surv_nodes_ < surv_node_cap_ && *surv_counter_ < surv_cap_)
            cut_flags_ |= surv_deadline_ < search_deadline_ ? 16 : phase_bit_;
        cut = true;
        return false;
    }
    ++surv_nodes_;
    ++*surv_counter_;
    int const now = game_.get_round_num();
    int const step = depth + 1;
    int const when = step + surv_offset_; // other dragons' moves made by then
    struct Child
    {
        Direction dir;
        int cell;
        int open;
    };
    std::array<Child, 3> kids{Child{heading, 0, 0}, Child{heading, 0, 0}, Child{heading, 0, 0}};
    int n = 0;
    int const head = body.front();
    int const back = direction_index(heading.get_opposite());
    std::vector<int> const &free_at = *surv_free_;
    bool door = false; // v26: a portal we have never looked through, next to the head
    for (int di = 0; di < 4; ++di)
    {
        if (di == back)
            continue;
        int const c = nb(head, di); // v22: the neighbour table (kelp and blind portals: < 0)
        if (c == NBR_BLIND && nbr_portal_[static_cast<std::size_t>(4 * head + di)])
            door = true;
        if (c < 0)
            continue;
        if (surv_occ_[static_cast<std::size_t>(c)] > 0 || std::min(free_at[static_cast<std::size_t>(c)], surv_inf_) > when)
            continue;
        // Never seen, near where our unknown body is attached: perhaps our own body.
        if (surv_anchor_ >= 0 && unknown > 0 && !memory_[static_cast<std::size_t>(c)].seen &&
            distance(position_of(c), surv_anchor_pos_) <= std::min(unknown, Config::UNKNOWN_BODY_RADIUS))
            continue;
        int open = 0;
        for (int e = 0; e < 4; ++e)
        {
            int const c2 = nb(c, e);
            if (c2 >= 0 && surv_occ_[static_cast<std::size_t>(c2)] == 0 &&
                std::min(free_at[static_cast<std::size_t>(c2)], surv_inf_) <= when + 1)
                ++open;
        }
        kids[static_cast<std::size_t>(n++)] = Child{Direction::get_direction_list()[static_cast<std::size_t>(di)], c, open};
    }
    for (int i = 1; i < n; ++i) // most open first (at most three: a tiny insertion sort)
        for (int j = i; j > 0 && kids[static_cast<std::size_t>(j)].open > kids[static_cast<std::size_t>(j - 1)].open; --j)
            std::swap(kids[static_cast<std::size_t>(j)], kids[static_cast<std::size_t>(j - 1)]);
    for (int k = 0; k < n; ++k)
    {
        Child const &kid = kids[static_cast<std::size_t>(k)];
        std::size_t const c = static_cast<std::size_t>(kid.cell);
        CellMemory const &m = memory_[c];
        bool const ate = m.pearl && m.last_seen == now && surv_eaten_[c] == 0;
        body.push_front(kid.cell);
        ++surv_occ_[c];
        int popped = -1;
        bool used_unknown = false;
        if (ate)
            ++surv_eaten_[c];
        else if (unknown > 0)
        {
            --unknown;
            used_unknown = true;
        }
        else
        {
            popped = body.back();
            body.pop_back();
            --surv_occ_[static_cast<std::size_t>(popped)];
        }
        bool const done = surv_dfs(body, unknown, kid.dir, step, target, best, cut);
        if (popped >= 0)
        {
            body.push_back(popped);
            ++surv_occ_[static_cast<std::size_t>(popped)];
        }
        if (used_unknown)
            ++unknown;
        if (ate)
            --surv_eaten_[c];
        body.pop_front();
        --surv_occ_[c];
        if (done)
            return true;
        if (cut)
            return false;
    }
    if (door)
    {
        // v26: nothing else got us there, but this portal may: count it as a way on.
        surv_via_portal_ = true;
        best = std::max(best, target);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// v21: the king's plan. With 10-20 free steps a turn, the order of the next few
// multi-step moves matters: eat here now and be turned away from the cluster, or line
// up a rush. For the king and growers, each of the best few moves this turn is followed
// by a small search over our own moves for the next turns (nobody else's: other bodies
// are timed as the deep survival search times them). Each turn's options: shortest
// paths (time-aware against our own moving body) to every tile within our free steps,
// the PLAN_BRANCH best by food on the way and progress along the route, each then
// played out exactly. A plan is only worth anything if, where it ends, the deep
// survival search proves a way on for our whole length: the king must not die to itself.
// ---------------------------------------------------------------------------
using PlanPath = std::vector<std::pair<int, Direction>>; // tiles entered, with the direction into each

// Pearls on `c` and what is left of them (one we can see: all of it).
double plan_pearl(int c) const
{
    CellMemory const &m = memory_[static_cast<std::size_t>(c)];
    if (!m.pearl)
        return 0.0;
    double const fresh = m.last_seen == game_.get_round_num() ? 1.0 : std::max(0.15, 1.0 - (game_.get_round_num() - m.pearl_seen) / 40.0);
    return food_value(position_of(c)) * fresh;
}

// Steps still to go after `pm`: along the route by the pearls' maps (pearls on pm.path
// eaten), or to the nearest objective when we have no route.
int plan_remaining(PostMove const &pm) const
{
    int const at = index(pm.head);
    if (route_seq_.empty())
        return std::min(objective_radius_, objective_dist_[static_cast<std::size_t>(at)]);
    int total = 0, prev = at;
    for (int j : route_seq_)
    {
        int const cell = route_cells_[static_cast<std::size_t>(j)];
        bool eaten = false;
        for (Position const &p : pm.path)
            if (index(p) == cell)
                eaten = true;
        if (eaten)
            continue;
        int const d = route_fields_[static_cast<std::size_t>(j)][static_cast<std::size_t>(prev)];
        if (d >= INF)
        {
            if (prev == at)
                return INF;
            break;
        }
        total += d;
        prev = cell;
    }
    return total;
}

// One more turn of our own steps applied to `pm` exactly: each step checked against our
// body as the step before left it (tail included: the check comes before the tail
// moves), a pearl (seen, or remembered) eaten keeps the tail, other bodies timed
// cautiously (plan_free_at_: a tile another head can reach may hold its body for its
// length; the others have moved turn - 1 times; one we cannot place gone after
// DEEP_UNPLACED_BODY). False on a collision.
bool plan_extend(PostMove &pm, Direction &heading, PlanPath const &path, int turn, double &food) const
{
    for (auto const &[c, d] : path)
    {
        std::size_t const ci = static_cast<std::size_t>(c);
        if (post_index(pm, c) >= 0)
            return false;
        if (std::min(plan_free_at_[ci], Config::DEEP_UNPLACED_BODY) > turn - 1)
            return false;
        bool again = false;
        for (Position const &p : pm.path)
            if (index(p) == c)
                again = true;
        pm.path.push_back(position_of(c));
        ++pm.steps;
        if (!again && memory_[ci].pearl)
        {
            ++pm.length;
            ++pm.eaten;
            food += plan_pearl(c);
        }
        heading = d;
    }
    if (!path.empty())
        pm.head = position_of(path.back().first);
    return true;
}

// The options for turn `turn` from `pm`: a shortest time-aware path to every tile within
// our free steps (no portals), the PLAN_BRANCH best by food on the way and progress.
// v22: one pass. Our body's tiles are looked up in a stamped table (not searched for
// along the body per tile), food and the route pearls eaten (a bit each) are carried
// along the search tree, progress is the route's remaining length from each tile with
// those pearls skipped, and only the best PLAN_BRANCH are kept (no sort of them all).
mutable std::vector<int> po_stamp_, po_dist_, po_parent_, po_via_, po_body_, po_body_stamp_, po_queue_;
mutable std::vector<unsigned> po_mask_;
mutable std::vector<unsigned> po_route_bit_; // per tile: its route pearl's bit (plan_king_moves)
mutable std::vector<double> po_food_;
mutable int po_gen_ = 0;

std::vector<PlanPath> plan_options(PostMove const &pm, Direction heading, int turn) const
{
    SUBPROF(0);
    int const free = std::min(free_moves(pm.length), Config::PLAN_MAX_STEPS);
    std::size_t const cells = memory_.size();
    if (po_stamp_.size() != cells)
    {
        po_stamp_.assign(cells, 0);
        po_body_stamp_.assign(cells, 0);
        po_dist_.assign(cells, 0);
        po_parent_.assign(cells, -1);
        po_via_.assign(cells, 0);
        po_body_.assign(cells, 0);
        po_mask_.assign(cells, 0);
        po_food_.assign(cells, 0.0);
    }
    int const gen = ++po_gen_;
    // Our tiles after pm (as post_index: the newest visit of a tile counts).
    int const k = static_cast<int>(pm.path.size());
    for (int m = 1; m <= k; ++m)
    {
        std::size_t const c = static_cast<std::size_t>(index(pm.path[static_cast<std::size_t>(m - 1)]));
        po_body_stamp_[c] = gen;
        po_body_[c] = k - m;
    }
    auto body_index = [&](std::size_t c)
    {
        int i;
        if (po_body_stamp_[c] == gen)
            i = po_body_[c];
        else
        {
            i = own_idx_[c];
            if (i == -1)
                return -1;
            if (i == -2)
                i = known_body_; // earliest index it could have: frees last
            i += pm.steps;
        }
        return i < pm.length ? i : -1;
    };
    // The route's pearls by position in route_seq_ (at most 8: the beam's depth): a bit
    // each (po_route_bit_, set up by plan_king_moves).
    std::size_t const nr = std::min<std::size_t>(route_seq_.size(), 31);
    auto route_bit = [&](int c)
    { return po_route_bit_[static_cast<std::size_t>(c)]; };
    // Steps still to go from `at` with the pearls in `eaten` gone: to the first pearl
    // left, then the legs between the rest. All but the first leg depend on `eaten`
    // alone, so they are worked out once per set of pearls eaten (a call sees few).
    struct Rest
    {
        unsigned eaten;
        int first; // route_seq_ position of the first pearl left (-1: none)
        int tail;  // the legs after it, up to where they break off
    };
    std::array<Rest, 8> rests{};
    int nrests = 0;
    auto rest_of = [&](unsigned eaten) -> Rest const &
    {
        for (int i = 0; i < nrests; ++i)
            if (rests[static_cast<std::size_t>(i)].eaten == eaten)
                return rests[static_cast<std::size_t>(i)];
        Rest r{eaten, -1, 0};
        int prev = -1;
        for (std::size_t q = 0; q < nr; ++q)
        {
            if (eaten >> q & 1U)
                continue;
            std::size_t const j = static_cast<std::size_t>(route_seq_[q]);
            if (r.first < 0)
                r.first = static_cast<int>(q);
            else
            {
                int const d = route_fields_[j][static_cast<std::size_t>(prev)];
                if (d >= INF)
                    break;
                r.tail += d;
            }
            prev = route_cells_[j];
        }
        std::size_t const slot = static_cast<std::size_t>(nrests < 8 ? nrests++ : 7);
        rests[slot] = r;
        return rests[slot];
    };
    auto remaining = [&](int at, unsigned eaten)
    {
        if (route_seq_.empty())
            return std::min(objective_radius_, objective_dist_[static_cast<std::size_t>(at)]);
        Rest const &r = rest_of(eaten);
        if (r.first < 0)
            return 0;
        int const d = route_fields_[static_cast<std::size_t>(route_seq_[static_cast<std::size_t>(r.first)])][static_cast<std::size_t>(at)];
        return d >= INF ? INF : d + r.tail;
    };
    unsigned base = 0;
    for (Position const &p : pm.path)
        base |= route_bit(index(p));
    int const start = index(pm.head);
    std::size_t const si = static_cast<std::size_t>(start);
    po_stamp_[si] = gen;
    po_dist_[si] = 0;
    po_food_[si] = 0.0;
    po_mask_[si] = base;
    po_queue_.clear();
    po_queue_.push_back(start);
    int const before = remaining(start, base);
    int const back = direction_index(heading.get_opposite());
    std::array<std::pair<double, int>, static_cast<std::size_t>(Config::PLAN_BRANCH)> best{};
    int nbest = 0;
    for (std::size_t q = 0; q < po_queue_.size(); ++q)
    {
        int const c = po_queue_[q];
        std::size_t const ci = static_cast<std::size_t>(c);
        int const r = po_dist_[ci];
        if (r >= free)
            continue;
        for (int di = 0; di < 4; ++di)
        {
            if (r == 0 && di == back)
                continue;
            int const n = nb(c, di);
            if (n < 0 || nbr_portal_[static_cast<std::size_t>(4 * c + di)])
                continue;
            std::size_t const ni = static_cast<std::size_t>(n);
            if (po_stamp_[ni] == gen)
                continue;
            int const j = body_index(ni);
            if (j >= 0 && r + 1 < pm.length - j + 1)
                continue; // our own body, still there
            if (std::min(plan_free_at_[ni], Config::DEEP_UNPLACED_BODY) > turn - 1)
                continue;
            po_stamp_[ni] = gen;
            po_dist_[ni] = r + 1;
            po_parent_[ni] = c;
            po_via_[ni] = di;
            po_food_[ni] = po_food_[ci] + (po_body_stamp_[ni] == gen ? 0.0 : plan_pearl(n));
            po_mask_[ni] = po_mask_[ci] | route_bit(n);
            po_queue_.push_back(n);
            int const after = remaining(n, po_mask_[ni]);
            int const progress = before >= INF || after >= INF ? 0 : before - after;
            double const score = po_food_[ni] + progress * Config::STEP_EAT;
            // Keep the best PLAN_BRANCH (ties: the one found first).
            if (nbest < Config::PLAN_BRANCH || score > best[static_cast<std::size_t>(nbest - 1)].first)
            {
                int at = nbest < Config::PLAN_BRANCH ? nbest++ : nbest - 1;
                while (at > 0 && best[static_cast<std::size_t>(at - 1)].first < score)
                {
                    best[static_cast<std::size_t>(at)] = best[static_cast<std::size_t>(at - 1)];
                    --at;
                }
                best[static_cast<std::size_t>(at)] = {score, n};
            }
        }
    }
    std::vector<PlanPath> out;
    for (int b = 0; b < nbest; ++b)
    {
        PlanPath path;
        for (int at = best[static_cast<std::size_t>(b)].second; at != start; at = po_parent_[static_cast<std::size_t>(at)])
            path.push_back({at, Direction::get_direction_list()[static_cast<std::size_t>(po_via_[static_cast<std::size_t>(at)])]});
        std::reverse(path.begin(), path.end());
        out.push_back(std::move(path));
    }
    return out;
}

// Can we keep going for our whole length from where the plan leaves us (after `turns`
// turns)? Proven by the deep survival search; not searched (budget) counts as no.
bool plan_safe(PostMove const &pm, Direction heading, int turns) const
{
    SUBPROF(1);
    std::deque<int> body;
    for (std::size_t k = pm.path.size(); k-- > 0;)
        body.push_back(index(pm.path[k]));
    for (Position const &p : own_trail_)
        body.push_back(index(p));
    while (static_cast<int>(body.size()) > pm.length)
        body.pop_back();
    int const unknown = pm.length - static_cast<int>(body.size());
    SurvivalResult const sr = survival_from(body, unknown, heading, pm.length, turns - 1, 2, true);
    return sr.ok();
}

// For each of the king's best moves: the best plan for the turns after it (the food its
// later turns reach, discounted; progress is already in the move's own score) among
// those that end where the deep search finds a way on.
void plan_king_moves(std::vector<Candidate> &candidates)
{
    if (!deep_dragon() || free_moves(controller_.get_length()) < 2 || queen_cycling()) // v22: not on her cycle
        return;
    std::vector<Candidate *> pick;
    for (Candidate &c : candidates)
        if (!c.head_trade && c.destination_known && !c.unknown_portal && !c.steps.empty())
            pick.push_back(&c);
    std::sort(pick.begin(), pick.end(), [](Candidate const *a, Candidate const *b)
              { return a->safe != b->safe ? a->safe : a->score > b->score; });
    if (static_cast<int>(pick.size()) > Config::PLAN_CANDIDATES)
        pick.resize(static_cast<std::size_t>(Config::PLAN_CANDIDATES));
    // v22: each route pearl's bit, by tile, for plan_options (cleared again below).
    if (po_route_bit_.size() != memory_.size())
        po_route_bit_.assign(memory_.size(), 0);
    std::size_t const nr = std::min<std::size_t>(route_seq_.size(), 31);
    for (std::size_t q = 0; q < nr; ++q)
        po_route_bit_[static_cast<std::size_t>(route_cells_[static_cast<std::size_t>(route_seq_[q])])] |= 1U << q;
    struct ClearBits
    {
        std::vector<unsigned> &bits;
        std::vector<int> const &cells, &seq;
        std::size_t n;
        ~ClearBits()
        {
            for (std::size_t q = 0; q < n; ++q)
                bits[static_cast<std::size_t>(cells[static_cast<std::size_t>(seq[q])])] = 0;
        }
    } clear_bits{po_route_bit_, route_cells_, route_seq_, nr};
    for (Candidate *c : pick)
    {
        if (out_of_time())
            break; // v22: out of points this turn (the rest keep their scores as they are)
        PostMove pm1;
        if (!simulate_path(c->steps, pm1, false, c->rush))
            continue;
        struct Leaf
        {
            double value;
            PostMove pm;
            Direction heading;
            int turns;
        };
        std::vector<Leaf> leaves;
        // Turns 2..PLAN_TURNS, breadth first: each state's options, kept as leaves too.
        std::vector<Leaf> frontier{{0.0, pm1, c->steps.back(), 1}};
        double weight = 1.0;
        for (int turn = 2; turn <= Config::PLAN_TURNS; ++turn)
        {
            weight *= Config::PLAN_DISCOUNT;
            std::vector<Leaf> next;
            for (Leaf const &s : frontier)
            {
                if (out_of_time())
                    break;
                for (PlanPath const &path : plan_options(s.pm, s.heading, turn))
                {
                    Leaf n{s.value, s.pm, s.heading, turn};
                    double food = 0.0;
                    if (!plan_extend(n.pm, n.heading, path, turn, food))
                        continue;
                    // Progress is the move's own business (its score has it, measured
                    // better): a plan adds the food its later turns reach, discounted.
                    n.value += weight * food;
                    next.push_back(n);
                    leaves.push_back(n);
                }
            }
            frontier = std::move(next);
        }
        std::sort(leaves.begin(), leaves.end(), [](Leaf const &a, Leaf const &b)
                  { return a.value > b.value; });
        c->plan_value = -1;
        for (Leaf const &leaf : leaves)
            if (plan_safe(leaf.pm, leaf.heading, leaf.turns))
            {
                c->plan_value = std::max(0, static_cast<int>(std::lround(leaf.value)));
                c->score += c->plan_value;
                break;
            }
    }
}