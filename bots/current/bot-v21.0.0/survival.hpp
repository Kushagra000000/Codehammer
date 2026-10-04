// Included inside ReplayDerivedBot (before scoring.hpp).
// v21 survival search: after a candidate move, is there a way to keep moving for
// the next few turns? A depth-first search over our own next single moves, played
// as the engine plays them:
//  - the head steps first; a tile holding any segment of ours is fatal, the tail
//    included (the collision check runs before the tail advances);
//  - a pearl we can see on the tile is eaten: we grow and the tail stays this step;
//    otherwise the tail leaves its tile;
//  - kelp blocks, the map wraps, known portals are followed (unknown ones are not
//    taken: we cannot see where they lead);
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
    bool ok() const { return depth >= 0 && depth >= target; }
    // Searched in full and still short of most of the target: a trap.
    bool dead() const { return depth >= 0 && complete && depth * 4 < target * 3; }
};

std::vector<int> other_free_at_;                      // per tile: our move from which another dragon's segment has gone (INF: never)
std::vector<int> body_free_at_;                       // v21: the same for bodies only (no head reach, not ours), in their moves
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
    return survival_from(body, unknown, heading, pm.length, 0, 0, deep_dragon());
}

// v21: from any body (head first; `unknown` more segments we cannot place), with the
// other dragons `offset` moves further on than now. `pool`: whose per-turn budget it
// spends (0 the moves', 1 the route checks', 2 the king's plans'). `deep` (the king and
// growers): the target is the whole length + 2 (at most KING_DFS_MAX_DEPTH), with the
// deep budgets.
SurvivalResult survival_from(std::deque<int> body, int unknown, Direction heading, int length, int offset, int pool,
                             bool deep = false) const
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
    surv_free_ = &other_free_at_;
    surv_anchor_ = deep && unknown > 0 && !own_trail_.empty() ? index(own_trail_.back()) : -1;
    surv_offset_ = offset;
    if (*surv_counter_ >= surv_cap_)
        return r; // out of time this turn: not searched
    for (int c : body)
        ++surv_occ_[static_cast<std::size_t>(c)];
    surv_nodes_ = 0;
    int best = 0;
    bool cut = false;
    surv_dfs(body, unknown, heading, 0, r.target, best, cut);
    for (int c : body)
        --surv_occ_[static_cast<std::size_t>(c)];
    r.depth = best;
    r.complete = !cut;
    return r;
}

// True once a path reaches the target.
bool surv_dfs(std::deque<int> &body, int &unknown, Direction heading, int depth, int target, int &best, bool &cut) const
{
    best = std::max(best, depth);
    if (depth >= target)
        return true;
    if (surv_nodes_ >= surv_node_cap_ || *surv_counter_ >= surv_cap_)
    {
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
    Position const head = position_of(body.front());
    for (Direction d : Direction::get_direction_list())
    {
        if (d == heading.get_opposite())
            continue;
        Step const s = step_from(head, d);
        if (!s.passable || !s.destination_known)
            continue;
        int const c = index(s.destination);
        if (surv_occ_[static_cast<std::size_t>(c)] > 0 || std::min((*surv_free_)[static_cast<std::size_t>(c)], surv_inf_) > when)
            continue;
        // Never seen, near where our unknown body is attached: perhaps our own body.
        if (surv_anchor_ >= 0 && unknown > 0 && !memory_[static_cast<std::size_t>(c)].seen &&
            distance(s.destination, position_of(surv_anchor_)) <= std::min(unknown, Config::UNKNOWN_BODY_RADIUS))
            continue;
        int open = 0;
        for (Direction e : Direction::get_direction_list())
        {
            Step const s2 = step_from(s.destination, e);
            if (!s2.passable || !s2.destination_known)
                continue;
            std::size_t const c2 = static_cast<std::size_t>(index(s2.destination));
            if (surv_occ_[c2] == 0 && std::min((*surv_free_)[c2], surv_inf_) <= when + 1)
                ++open;
        }
        kids[static_cast<std::size_t>(n++)] = Child{d, c, open};
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
std::vector<PlanPath> plan_options(PostMove const &pm, Direction heading, int turn) const
{
    int const free = std::min(free_moves(pm.length), Config::PLAN_MAX_STEPS);
    std::size_t const cells = memory_.size();
    std::vector<int> dist(cells, INF), parent(cells, -1), via(cells, 0);
    std::vector<int> order;
    int const start = index(pm.head);
    dist[static_cast<std::size_t>(start)] = 0;
    std::vector<int> queue{start};
    for (std::size_t q = 0; q < queue.size(); ++q)
    {
        int const c = queue[q];
        int const r = dist[static_cast<std::size_t>(c)];
        if (r >= free)
            continue;
        for (Direction d : Direction::get_direction_list())
        {
            if (r == 0 && d == heading.get_opposite())
                continue;
            Step const st = step_from(position_of(c), d);
            if (!st.passable || !st.destination_known || st.portal)
                continue;
            int const n = index(st.destination);
            std::size_t const ni = static_cast<std::size_t>(n);
            if (dist[ni] < INF)
                continue;
            int const j = post_index(pm, n);
            if (j >= 0 && r + 1 < pm.length - j + 1)
                continue; // our own body, still there
            if (std::min(plan_free_at_[ni], Config::DEEP_UNPLACED_BODY) > turn - 1)
                continue;
            dist[ni] = r + 1;
            parent[ni] = c;
            via[ni] = direction_index(d);
            queue.push_back(n);
            order.push_back(n);
        }
    }
    int const before = plan_remaining(pm);
    std::vector<std::pair<double, int>> scored;
    for (int e : order)
    {
        double food = 0.0;
        PostMove probe = pm;
        for (int at = e; at != start; at = parent[static_cast<std::size_t>(at)])
        {
            food += plan_pearl(at);
            probe.path.push_back(position_of(at)); // (order does not matter for "eaten")
        }
        probe.head = position_of(e);
        int const after = plan_remaining(probe);
        int const progress = before >= INF || after >= INF ? 0 : before - after;
        scored.push_back({food + progress * Config::STEP_EAT, e});
    }
    std::sort(scored.begin(), scored.end(), [](auto const &a, auto const &b)
              { return a.first > b.first; });
    std::vector<PlanPath> out;
    for (std::size_t k = 0; k < scored.size() && static_cast<int>(out.size()) < Config::PLAN_BRANCH; ++k)
    {
        PlanPath path;
        for (int at = scored[k].second; at != start; at = parent[static_cast<std::size_t>(at)])
            path.push_back({at, Direction::get_direction_list()[static_cast<std::size_t>(via[static_cast<std::size_t>(at)])]});
        std::reverse(path.begin(), path.end());
        out.push_back(std::move(path));
    }
    return out;
}

// Can we keep going for our whole length from where the plan leaves us (after `turns`
// turns)? Proven by the deep survival search; not searched (budget) counts as no.
bool plan_safe(PostMove const &pm, Direction heading, int turns) const
{
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
    if (!deep_dragon() || free_moves(controller_.get_length()) < 2)
        return;
    std::vector<Candidate *> pick;
    for (Candidate &c : candidates)
        if (!c.head_trade && c.destination_known && !c.unknown_portal && !c.steps.empty())
            pick.push_back(&c);
    std::sort(pick.begin(), pick.end(), [](Candidate const *a, Candidate const *b)
              { return a->safe != b->safe ? a->safe : a->score > b->score; });
    if (static_cast<int>(pick.size()) > Config::PLAN_CANDIDATES)
        pick.resize(static_cast<std::size_t>(Config::PLAN_CANDIDATES));
    for (Candidate *c : pick)
    {
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