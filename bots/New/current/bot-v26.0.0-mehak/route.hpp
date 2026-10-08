// Included inside ReplayDerivedBot (after explore.hpp).
// v15 pearl routing. Instead of walking to whichever pearl is nearest, plan a
// route through the nearby pearls we know of:
//  - candidates: the closest known pearls (path distance), each with its own
//    distance map;
//  - search over visiting orders, scoring a route by the pearls it eats with each
//    worth less the later it is reached (so a cluster beats a lone pearl a little
//    closer, and a lone pearl on the way is picked up);
//  - replay the best routes step by step with our body growing and trailing behind
//    us, and drop any where we would run into ourselves (the king used to eat in
//    an order that walled off the rest);
//  - head for the first pearl of the best route that survives; replan every turn,
//    with a small bonus for keeping the same target so we do not dither.
// With no pearl in reach, the old objective map (timers, exploring) takes over.
//
// v21:
//  - Time-aware distances. A body is not a wall forever: our own segment frees once
//    our tail has passed it, another dragon's once that dragon has moved on (see
//    body_free_at_ in survival.hpp). Paths may go through tiles that will be free by
//    the time we get there (timed_bfs).
//  - Arrival in turns. We take free_moves(L) steps a turn for free, so a pearl is
//    reached in turns_for(steps) turns; the discount is per turn.
//  - Who gets there first (prepare_etas): the same time-aware search from every
//    visible head. A pearl an enemy reaches in fewer turns than we do is worth
//    ROUTE_CONTEST_LOST of itself; an ally that gets there first takes it.
//  - The route must leave us room: the replay walks each leg by a shortest time-aware
//    path, and from where the route ends the survival search must find a way on
//    (for the king and growers: all the way, or nearly so when cut short by budget).
//  - Free moves: besides single steps, moves of several free steps (add_free_moves).

std::vector<std::vector<int>> route_fields_; // per candidate: steps to it from anywhere (legs between pearls)
std::vector<int> route_cells_;               // candidate pearl tiles
std::vector<double> route_weight_;
std::vector<int> route_enemy_turn_; // v21: per candidate: fewest turns an enemy head needs to be on it
std::vector<int> route_head_dist_;  // v21: time-aware steps from our head (prepare_etas)
std::vector<int> route_seq_;        // v21: the chosen route, in visiting order (candidate indices)
std::vector<int> route_path_;       // v21: the tiles of its checked replay, in order
int route_remaining_now_ = INF;     // v21: steps to go along the chosen route from here
int route_end_depth_ = -1;          // v21 (debug): survival depth where the chosen route ends
int route_horizon_ = 0;
int route_first_ = -1;     // candidate we are heading for, or -1
int last_route_cell_ = -1; // last turn's target (hysteresis)
int route_planned_ = 0;    // pearls in the chosen route (debug)
int route_next_cell_ = -1; // first tile of the path we checked (the move to prefer)
std::vector<int> sim_occ_; // scratch for replays
std::vector<int> sim_idx_; // scratch for replays: segment index of each tile of the replayed body

// v21: who gets where first.
std::vector<int> enemy_eta_turn_;  // per tile: fewest turns any visible enemy head needs to be on it
std::vector<int> enemy_eta_steps_; // per tile: fewest steps
struct AllyEta
{
    int id = -1;
    int length = 0;
    std::vector<int> steps;
};
std::vector<AllyEta> ally_etas_;
mutable std::vector<int> tb_stamp_; // scratch for timed_bfs
mutable int tb_gen_ = 0;

struct RouteState
{
    int pos = 0;
    int t = 0;
    double value = 0.0;
    std::uint32_t mask = 0;
    std::array<std::int8_t, 8> seq{};
    int len = 0;
};

// ---------------------------------------------------------------------------
// v21: time-aware distances
// ---------------------------------------------------------------------------

// Has another dragon's body left tile `c` once the other dragons have made
// `moves_made` moves (body_free_at_, survival.hpp)? Each of them moves once between
// two of our turns, so before our t-th turn from now they have made t - 1.
bool others_gone_by(int c, int moves_made) const
{
    int const f = body_free_at_[static_cast<std::size_t>(c)];
    return f == 0 || f <= moves_made;
}

// Fewest steps for our head to reach each tile from where `pm` leaves us (pm.steps
// already taken this turn; none: from where we are now), with every body where it
// will be by then: our own segment j (after pm) is passed once our tail has moved
// past it (pm.length - j + 1 more steps, eating nothing on the way), another
// dragon's once that dragon has made body_free_at_ moves (one a turn, while we go at
// full free speed). Kelp, tiles we know nothing of and marked traps block.
// With `out`: the whole map out to `limit` steps (returns 0). Otherwise: the steps
// to `target`, INF if it is further than `limit` or out of reach.
// v22: post_index for a move of no steps (our body as it is): no path to search.
int post_index_here(std::size_t c, PostMove const &pm) const
{
    int i = own_idx_[c];
    if (i == -1)
        return -1;
    if (i == -2)
        i = known_body_;
    i += pm.steps;
    return i < pm.length ? i : -1;
}

int timed_bfs(PostMove const &pm, int limit, int target, std::vector<int> *out) const
{
    int const L = controller_.get_length();
    int const now = game_.get_round_num();
    if (out)
        out->assign(memory_.size(), INF);
    if (tb_stamp_.size() != memory_.size())
        tb_stamp_.assign(memory_.size(), 0);
    ++tb_gen_;
    int const start = index(pm.head);
    if (start == target)
        return 0;
    int const turn_base = pm.steps > 0 ? 1 : 0; // a move this turn uses the whole turn
    std::vector<std::pair<int, int>> queue{{start, 0}};
    tb_stamp_[static_cast<std::size_t>(start)] = tb_gen_;
    if (out)
        (*out)[static_cast<std::size_t>(start)] = 0;
    for (std::size_t q = 0; q < queue.size(); ++q)
    {
        auto const [c, r] = queue[q];
        if (r >= limit)
            continue;
        for (int di = 0; di < 4; ++di)
        {
            int const n = nb(c, di); // v22
            if (n < 0)
                continue;
            std::size_t const ni = static_cast<std::size_t>(n);
            if (tb_stamp_[ni] == tb_gen_)
                continue;
            int const r1 = r + 1;
            int const j = pm.path.empty() ? post_index_here(ni, pm) : post_index(pm, n);
            if (j >= 0 && r1 < pm.length - j + 1)
                continue; // our own segment, still there
            if (!others_gone_by(n, turn_base + turns_for(r1, L) - 1))
                continue; // another body, still there
            if (trap_mark_[ni] > now && n != target)
                continue;
            tb_stamp_[ni] = tb_gen_; // (only once entered: a body may free it later by another way)
            if (out)
                (*out)[ni] = r1;
            if (n == target)
                return r1;
            queue.push_back({n, r1});
        }
    }
    return out ? 0 : INF;
}

// The same for another dragon's head: its own segments free as it moves on, ours
// once we have made that many moves (we move before each of its turns), the rest
// as above. Fewest steps to each tile, out to `limit`.
mutable std::vector<int> he_stamp_, he_idx_;
mutable int he_gen_ = 0;
void head_eta_bfs(SeenHead const &h, int limit, std::vector<int> &out) const
{
    int const len = h.estimated_length();
    int const L = controller_.get_length();
    BodyTrace const trace = trace_body(h.position, h.id);
    // v22: its segments' indices in a stamped table (was a hash map)
    if (he_stamp_.size() != memory_.size())
    {
        he_stamp_.assign(memory_.size(), 0);
        he_idx_.assign(memory_.size(), 0);
    }
    int const gen = ++he_gen_;
    for (std::size_t i = 0; i < trace.segments.size(); ++i)
    {
        std::size_t const c = static_cast<std::size_t>(index(trace.segments[i]));
        if (he_stamp_[c] != gen)
        {
            he_stamp_[c] = gen;
            he_idx_[c] = static_cast<int>(i);
        }
    }
    out.assign(memory_.size(), INF);
    int const start = index(h.position);
    out[static_cast<std::size_t>(start)] = 0;
    std::vector<std::pair<int, int>> queue{{start, 0}};
    for (std::size_t q = 0; q < queue.size(); ++q)
    {
        auto const [c, r] = queue[q];
        if (r >= limit)
            continue;
        int const back = direction_index(h.facing.get_opposite());
        for (int di = 0; di < 4; ++di)
        {
            if (r == 0 && di == back)
                continue;
            int const n = nb(c, di); // v22
            if (n < 0)
                continue;
            std::size_t const ni = static_cast<std::size_t>(n);
            if (out[ni] < INF)
                continue;
            int const r1 = r + 1;
            int const turn = turns_for(r1, len);
            if (he_stamp_[ni] == gen)
            {
                if (r1 < len - he_idx_[ni] + 1)
                    continue;
            }
            else if (own_idx_[ni] != -1)
            {
                int const oi = own_idx_[ni];
                if (turn < (oi == -2 ? L - known_body_ : L - oi))
                    continue;
            }
            else if (!others_gone_by(n, turn))
                continue;
            out[ni] = r1;
            queue.push_back({n, r1});
        }
    }
}

// v21: how far routing looks, in steps: the fixed radius and horizon, or that many
// turns at our free speed, whichever is further (capped).
int route_radius() const
{
    bool const keeper = retaining_length();
    int const speed = free_moves(controller_.get_length());
    return std::min(Config::ROUTE_MAX_RADIUS,
                    keeper ? std::max(Config::KEEPER_ROUTE_RADIUS, Config::KEEPER_ROUTE_RADIUS_TURNS * speed)
                           : std::max(Config::WORKER_ROUTE_RADIUS, Config::WORKER_ROUTE_RADIUS_TURNS * speed));
}
int route_horizon() const
{
    bool const keeper = retaining_length();
    int const speed = free_moves(controller_.get_length());
    return std::min(Config::ROUTE_MAX_HORIZON,
                    keeper ? std::max(Config::KEEPER_ROUTE_HORIZON, Config::KEEPER_ROUTE_HORIZON_TURNS * speed)
                           : std::max(Config::WORKER_ROUTE_HORIZON, Config::WORKER_ROUTE_HORIZON_TURNS * speed));
}

// Once per turn, before the objective map and routing: our time-aware map and one
// per visible head (enemies merged into the fewest turns any of them needs).
void prepare_etas()
{
    std::size_t const cells = memory_.size();
    PostMove here;
    here.head = controller_.get_position();
    here.length = controller_.get_length();
    here.steps = 0;
    timed_bfs(here, std::max(Config::ETA_RADIUS, route_radius()), -1, &route_head_dist_);
    enemy_eta_turn_.assign(cells, INF);
    enemy_eta_steps_.assign(cells, INF);
    ally_etas_.clear();
    for (SeenHead const &h : heads_)
    {
        std::vector<int> steps;
        head_eta_bfs(h, Config::ETA_RADIUS, steps);
        if (h.team == controller_.get_team())
        {
            ally_etas_.push_back(AllyEta{h.id, h.estimated_length(), std::move(steps)});
            continue;
        }
        int const len = h.estimated_length();
        for (std::size_t c = 0; c < cells; ++c)
            if (steps[c] < INF)
            {
                enemy_eta_steps_[c] = std::min(enemy_eta_steps_[c], steps[c]);
                enemy_eta_turn_[c] = std::min(enemy_eta_turn_[c], turns_for(steps[c], len));
            }
    }
}

// Fewest turns any visible enemy head needs to be on tile `c` (INF: none can, as far
// as we searched).
int enemy_turns_to(int c) const
{
    return enemy_eta_turn_.empty() ? INF : enemy_eta_turn_[static_cast<std::size_t>(c)];
}

// ---------------------------------------------------------------------------
// Planning
// ---------------------------------------------------------------------------

// Steps to `src` from anywhere (legs between pearls), out to max_depth. A tile
// holding a body counts once our time-aware map reaches it (it will have moved on
// by then); kelp and marked traps block, portals count.
mutable std::vector<int> rb_queue_; // v22: scratch
void route_bfs(Position src, int max_depth, std::vector<int> &out) const
{
    int const now = game_.get_round_num();
    out.assign(memory_.size(), INF);
    rb_queue_.clear();
    rb_queue_.push_back(index(src));
    out[static_cast<std::size_t>(index(src))] = 0;
    for (std::size_t q = 0; q < rb_queue_.size(); ++q)
    {
        int const c = rb_queue_[q];
        int const d = out[static_cast<std::size_t>(c)];
        if (d >= max_depth)
            continue;
        for (int di = 0; di < 4; ++di)
        {
            int const n = nb(c, di); // v22
            if (n < 0)
                continue;
            std::size_t const ni = static_cast<std::size_t>(n);
            if (out[ni] <= d + 1 || trap_mark_[ni] > now)
                continue;
            if (blocks_pearl_path(n) && route_head_dist_[ni] >= INF)
                continue;
            out[ni] = d + 1;
            rb_queue_.push_back(n);
        }
    }
}

// One leg of a replay: a shortest time-aware path from the replayed head `cur` to
// `target`, given the replayed body `snake` (head first; `length` segments in all,
// the ones not in `snake` cannot be placed), `t` steps into the route. Of equally
// short ways, the first step with the most free tiles around it. Empty if none
// within `limit` steps.
mutable std::vector<int> rl_parent_, rl_dir_; // v22: scratch
std::vector<std::pair<int, Direction>> replay_leg(std::deque<int> const &snake, int length, int t, int cur, int target,
                                                  int limit, int unknown_free)
{
    std::vector<std::pair<int, Direction>> path;
    if (limit <= 0)
        return path;
    int const L0 = controller_.get_length();
    int const now = game_.get_round_num();
    std::size_t const cells = memory_.size();
    if (sim_idx_.size() != cells)
        sim_idx_.assign(cells, -1);
    std::vector<int> &idx = sim_idx_;
    for (std::size_t i = 0; i < snake.size(); ++i)
        if (idx[static_cast<std::size_t>(snake[i])] < 0)
            idx[static_cast<std::size_t>(snake[i])] = static_cast<int>(i);
    auto enterable = [&](int n, int r1)
    {
        std::size_t const ni = static_cast<std::size_t>(n);
        int const i = idx[ni];
        if (i >= 0 && r1 < length - i + 1)
            return false; // our replayed body, still there
        if (i < 0 && own_idx_[ni] == -2 && t + r1 < unknown_free)
            return false; // a segment of ours we cannot place, perhaps still there
        if (!others_gone_by(n, turns_for(t + r1, L0) - 1))
            return false;
        return !(trap_mark_[ni] > now && n != target);
    };
    if (tb_stamp_.size() != cells)
        tb_stamp_.assign(cells, 0);
    ++tb_gen_;
    // v22: tile -> previous tile and direction into it, in stamped tables (was a hash map)
    if (rl_parent_.size() != cells)
    {
        rl_parent_.assign(cells, -1);
        rl_dir_.assign(cells, 0);
    }
    auto set_parent = [&](int n, int from, int di)
    {
        rl_parent_[static_cast<std::size_t>(n)] = from;
        rl_dir_[static_cast<std::size_t>(n)] = di;
    };
    std::vector<std::pair<int, int>> queue;
    tb_stamp_[static_cast<std::size_t>(cur)] = tb_gen_;
    // First steps, most open first.
    std::vector<std::tuple<int, int, int>> first; // (free around, tile, direction)
    for (int di = 0; di < 4; ++di)
    {
        int const n = nb(cur, di);
        if (n < 0)
            continue;
        if (!enterable(n, 1))
            continue;
        int open = 0;
        for (int e = 0; e < 4; ++e)
        {
            int const n2 = nb(n, e);
            if (n2 >= 0 && idx[static_cast<std::size_t>(n2)] < 0 && !other_occ_[static_cast<std::size_t>(n2)])
                ++open;
        }
        first.push_back({open, n, di});
    }
    std::stable_sort(first.begin(), first.end(), [](auto const &a, auto const &b)
                     { return std::get<0>(a) > std::get<0>(b); });
    int found = -1;
    for (auto const &[open, n, d] : first)
    {
        (void)open;
        if (tb_stamp_[static_cast<std::size_t>(n)] == tb_gen_)
            continue;
        tb_stamp_[static_cast<std::size_t>(n)] = tb_gen_;
        set_parent(n, cur, d);
        if (n == target)
        {
            found = n;
            break;
        }
        queue.push_back({n, 1});
    }
    for (std::size_t q = 0; q < queue.size() && found < 0; ++q)
    {
        auto const [c, r] = queue[q];
        if (r >= limit)
            continue;
        for (int di = 0; di < 4; ++di)
        {
            int const n = nb(c, di);
            if (n < 0)
                continue;
            if (tb_stamp_[static_cast<std::size_t>(n)] == tb_gen_ || !enterable(n, r + 1))
                continue;
            tb_stamp_[static_cast<std::size_t>(n)] = tb_gen_;
            set_parent(n, c, di);
            if (n == target)
            {
                found = n;
                break;
            }
            queue.push_back({n, r + 1});
        }
    }
    for (int c : snake)
        idx[static_cast<std::size_t>(c)] = -1;
    if (found < 0)
        return path;
    for (int at = found; at != cur;)
    {
        path.push_back({at, Direction::get_direction_list()[static_cast<std::size_t>(rl_dir_[static_cast<std::size_t>(at)])]});
        at = rl_parent_[static_cast<std::size_t>(at)];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// Walk the route as the engine would move us, a step at a time, our body trailing
// behind and growing with every pearl. Each leg follows a shortest time-aware path
// from wherever the replay has got to (searched again if our growing body gets in
// its way). False if we would hit ourselves or get stuck, or (v21) if the route
// ends where we cannot keep moving: from the end, the survival search must find a
// way on for our length + 2 moves. `path`: the tiles walked; `end_depth`: what the
// survival search found (-1: not run).
bool replay_route(RouteState const &s, std::vector<int> &path, int &end_depth)
{
    SUBPROF(3);
    path.clear();
    end_depth = -1;
    std::size_t const cells = memory_.size();
    if (sim_occ_.size() != cells)
        sim_occ_.assign(cells, 0);
    int const L0 = controller_.get_length();
    std::deque<int> snake;
    for (Position const &p : own_trail_)
        snake.push_back(index(p));
    for (int c : snake)
        ++sim_occ_[static_cast<std::size_t>(c)];
    int length = L0;
    int const unknown_free = L0 - known_body_ + 1; // segments we cannot place are gone by this step
    std::vector<int> eaten;
    bool ok = true;
    int cur = index(controller_.get_position());
    Direction heading = controller_.get_dir();
    int t = 0;
    int const limit = route_horizon_ + Config::ROUTE_REPLAY_SLACK;
    for (int k = 0; k < s.len && ok; ++k)
    {
        int const target = route_cells_[static_cast<std::size_t>(s.seq[static_cast<std::size_t>(k)])];
        if (std::find(eaten.begin(), eaten.end(), target) != eaten.end())
            continue; // eaten on the way
        std::vector<std::pair<int, Direction>> leg;
        std::size_t at = 0;
        int searches = 0;
        while (ok && cur != target)
        {
            if (at >= leg.size())
            {
                if (++searches > 3)
                {
                    ok = false;
                    break;
                }
                leg = replay_leg(snake, length, t, cur, target, limit - t, unknown_free + static_cast<int>(eaten.size()));
                at = 0;
                if (leg.empty())
                {
                    ok = false;
                    break;
                }
            }
            auto const [next, dir] = leg[at++];
            std::size_t const ni = static_cast<std::size_t>(next);
            bool const blocked = sim_occ_[ni] > 0 || !others_gone_by(next, turns_for(t + 1, L0) - 1) ||
                                 (own_idx_[ni] == -2 && t + 1 < unknown_free + static_cast<int>(eaten.size()));
            if (blocked)
            {
                leg.clear(); // our body grew into the way: search again from here
                at = 0;
                continue;
            }
            ++t;
            if (t > limit)
            {
                ok = false;
                break;
            }
            heading = dir;
            cur = next;
            path.push_back(cur);
            snake.push_front(cur);
            ++sim_occ_[ni];
            if (memory_[ni].pearl && std::find(eaten.begin(), eaten.end(), cur) == eaten.end())
            {
                eaten.push_back(cur);
                ++length; // the tail stays this step
            }
            while (static_cast<int>(snake.size()) > length)
            {
                --sim_occ_[static_cast<std::size_t>(snake.back())];
                snake.pop_back();
            }
        }
    }
    if (ok)
    {
        // v21: from where the route ends, can we keep going? The route takes T turns
        // at our free speed, so before the search's first move the others have moved
        // T times (one more than in a search from this turn's move); the pearls it ate
        // are gone.
        for (int c : eaten)
            ++surv_eaten_[static_cast<std::size_t>(c)];
        SUBPROF(4);
        SurvivalResult const sr = survival_from(snake, length - static_cast<int>(snake.size()), heading, length,
                                                std::max(0, turns_for(t, L0) - 1), 1, deep_dragon());
        for (int c : eaten)
            --surv_eaten_[static_cast<std::size_t>(c)];
        end_depth = sr.depth;
        if (sr.depth < 0)
        {
            // Not searched (its budget is spent): at least not boxed in by our own body.
            int free_nb = 0;
            for (Direction d : Direction::get_direction_list())
            {
                Step const st = step_from(position_of(cur), d);
                if (st.passable && st.destination_known && sim_occ_[static_cast<std::size_t>(index(st.destination))] == 0 &&
                    !other_occ_[static_cast<std::size_t>(index(st.destination))])
                    ++free_nb;
            }
            ok = free_nb > 0;
        }
        else if (retaining_length() || valuable_now_)
            // The king and growers: a way on all the way, or nearly so when the
            // search was cut short by its budget (in open water it rarely is).
            ok = sr.ok() || (!sr.complete && sr.depth * 4 >= sr.target * 3);
        else
            ok = !sr.dead();
    }
    for (int c : snake)
        --sim_occ_[static_cast<std::size_t>(c)];
    return ok;
}

void plan_route()
{
    route_first_ = -1;
    route_planned_ = 0;
    route_next_cell_ = -1;
    route_seq_.clear();
    route_path_.clear();
    route_remaining_now_ = INF;
    route_end_depth_ = -1;
    if (!Config::ROUTING_ENABLED || (feeder_target_ && !retaining_length()))
        return; // feeders walk to the king
    if (queen_cycling())
        return; // v22: the queen walks her cycle
    int const now = game_.get_round_num();
    int const L = controller_.get_length();
    bool const keeper = retaining_length();
    bool const late = now >= SPECIALISE_ROUND;
    int const radius = route_radius(); // v21: grows with our free steps
    int const horizon = route_horizon();
    route_horizon_ = horizon;
    int const max_age = keeper ? Config::KEEPER_REMEMBERED_PEARL_MAX_AGE : Config::REMEMBERED_PEARL_MAX_AGE;

    // Candidates: the closest pearls we can still win (v21: time-aware steps).
    std::vector<std::pair<int, int>> found; // (distance, cell)
    for (std::size_t c = 0; c < memory_.size(); ++c)
    {
        CellMemory const &m = memory_[c];
        if (!m.pearl || route_head_dist_[c] > radius || route_head_dist_[c] == 0 || now - m.pearl_seen > max_age)
            continue;
        Position const p = position_of(static_cast<int>(c));
        if ((late && reserved_food(p)) || pearl_taken_by_ally(p))
            continue;
        found.push_back({route_head_dist_[c], static_cast<int>(c)});
    }
    if (found.empty())
        return;
    std::sort(found.begin(), found.end());
    if (static_cast<int>(found.size()) > Config::ROUTE_MAX_PEARLS)
        found.resize(static_cast<std::size_t>(Config::ROUTE_MAX_PEARLS));
    // v22: as many pearls' maps as the route's points allow (one whole-map search each).
    std::size_t k = found.size();
    route_cells_.assign(k, 0);
    route_weight_.assign(k, 1.0);
    route_enemy_turn_.assign(k, INF);
    route_fields_.resize(k);
    for (std::size_t i = 0; i < k; ++i)
    {
        int const c = found[i].second;
        route_cells_[i] = c;
        CellMemory const &m = memory_[static_cast<std::size_t>(c)];
        double w = m.last_seen == now ? 1.0 : std::max(0.15, 1.0 - (now - m.pearl_seen) / 40.0);
        Position const pp = position_of(c);
        // v21: who gets there first is judged per route, at our arrival (below). For the
        // king and growers an enemy head that can get near it is a fight there.
        route_enemy_turn_[i] = enemy_turns_to(c);
        if (keeper && enemy_eta_steps_[static_cast<std::size_t>(c)] <= Config::ROUTE_ENEMY_NEAR)
            w *= Config::ROUTE_KEEPER_ENEMY_FACTOR;
        // Piles of pearls are often where dragons died: a crowd. Other dragons' segments
        // around it (seen now, or a moment ago) make it worth less.
        int crowd = 0;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            {
                std::size_t const q = static_cast<std::size_t>(index(Position(wrap(pp.x + dx, width_), wrap(pp.y + dy, height_))));
                CellMemory const &qm = memory_[q];
                if (other_occ_[q] || (qm.other_seen && now - qm.last_seen <= Config::OCCUPIED_MEMORY)) // (a crowd: recent sightings only)
                    ++crowd;
            }
        w /= 1.0 + crowd * (keeper ? Config::ROUTE_KEEPER_CROWD : Config::ROUTE_WORKER_CROWD);
        route_weight_[i] = w;
        {
            SUBPROF(2);
            route_bfs(position_of(c), horizon, route_fields_[i]); // no leg can be longer
        }
        if (i + 1 < k && out_of_time())
        {
            k = i + 1;
            route_cells_.resize(k);
            route_weight_.resize(k);
            route_enemy_turn_.resize(k);
            route_fields_.resize(k);
        }
    }

    // Beam search over visiting orders. v21: a pearl reached at step t is reached in
    // turns_for(t) turns at our free speed; the discount is per turn, and a pearl an
    // enemy reaches in fewer turns is worth ROUTE_CONTEST_LOST of itself. Of orders
    // arriving in the same turns, fewer steps win (a tie-break only).
    double const gamma = keeper ? Config::KEEPER_ROUTE_DISCOUNT : Config::WORKER_ROUTE_DISCOUNT;
    std::vector<double> disc(static_cast<std::size_t>(horizon + 1), 1.0);
    for (int t = 1; t <= horizon; ++t)
        disc[static_cast<std::size_t>(t)] = disc[static_cast<std::size_t>(t - 1)] * gamma;
    std::vector<RouteState> frontier(1);
    frontier[0].pos = index(controller_.get_position());
    std::vector<RouteState> all;
    for (int depth = 0; depth < 8 && !frontier.empty(); ++depth)
    {
        std::vector<RouteState> next;
        for (RouteState const &s : frontier)
            for (std::size_t j = 0; j < k; ++j)
            {
                if (s.mask & (1U << j))
                    continue;
                // The first leg from here is time-aware; later ones use the pearls' maps.
                int const d = depth == 0 ? route_head_dist_[static_cast<std::size_t>(route_cells_[j])]
                                         : route_fields_[j][static_cast<std::size_t>(s.pos)];
                if (d >= INF || d == 0 || s.t + d > horizon)
                    continue;
                RouteState n = s;
                n.pos = route_cells_[j];
                n.t = s.t + d;
                int const turn = turns_for(n.t, L);
                double w = route_weight_[j] * disc[static_cast<std::size_t>(turn)];
                if (route_enemy_turn_[j] < turn)
                    w *= Config::ROUTE_CONTEST_LOST;
                n.value += w - 1e-4 * d;
                if (depth == 0 && route_cells_[j] == last_route_cell_)
                    n.value += Config::ROUTE_KEEP_BONUS; // keep last turn's target
                n.mask |= 1U << j;
                n.seq[static_cast<std::size_t>(n.len++)] = static_cast<std::int8_t>(j);
                next.push_back(n);
            }
        std::sort(next.begin(), next.end(), [](RouteState const &a, RouteState const &b)
                  { return a.value > b.value; });
        if (static_cast<int>(next.size()) > Config::ROUTE_BEAM)
            next.resize(static_cast<std::size_t>(Config::ROUTE_BEAM));
        all.insert(all.end(), next.begin(), next.end());
        frontier = std::move(next);
    }
    std::sort(all.begin(), all.end(), [](RouteState const &a, RouteState const &b)
              { return a.value > b.value; });
    auto choose = [&](RouteState const &s, std::vector<int> &path, int end_depth)
    {
        route_first_ = s.seq[0];
        route_planned_ = s.len;
        last_route_cell_ = route_cells_[static_cast<std::size_t>(route_first_)];
        route_seq_.assign(s.seq.begin(), s.seq.begin() + s.len);
        route_path_ = std::move(path);
        route_next_cell_ = route_path_.empty() ? -1 : route_path_.front();
        route_end_depth_ = end_depth;
        PostMove here;
        here.head = controller_.get_position();
        here.length = L;
        here.steps = 0;
        route_remaining_now_ = route_remaining(here, horizon + Config::ROUTE_REPLAY_SLACK);
    };
    int checked = 0;
    std::vector<int> path;
    int end_depth = -1;
    for (RouteState const &s : all)
    {
        if (checked++ >= Config::ROUTE_REPLAYS || out_of_time())
            break;
        if (replay_route(s, path, end_depth))
        {
            choose(s, path, end_depth);
            return;
        }
    }
    // Every good route runs into ourselves or ends boxed in: fall back to the
    // nearest pearl we can at least reach (and leave) safely on its own.
    for (RouteState const &s : all)
        if (out_of_time())
            break;
        else if (s.len == 1 && replay_route(s, path, end_depth))
        {
            choose(s, path, end_depth);
            return;
        }
}

// v21: steps still to go along the chosen route after the move `pm` (none: from
// here): to the first route pearl the move has not eaten (time-aware, from where it
// leaves us, up to `limit` steps), then the legs between the rest. INF when that
// first pearl is out of reach.
int route_remaining(PostMove const &pm, int limit) const
{
    if (route_seq_.empty())
        return INF;
    int total = 0;
    int prev = -1;
    for (int j : route_seq_)
    {
        int const cell = route_cells_[static_cast<std::size_t>(j)];
        bool eaten = false;
        for (Position const &p : pm.path)
            if (index(p) == cell)
                eaten = true;
        if (eaten)
            continue;
        if (prev < 0)
        {
            int const d = pm.steps == 0 ? route_head_dist_[static_cast<std::size_t>(cell)] : timed_bfs(pm, limit, cell, nullptr);
            if (d >= INF)
                return INF;
            total += d;
        }
        else
        {
            int const d = route_fields_[static_cast<std::size_t>(j)][static_cast<std::size_t>(prev)];
            if (d >= INF)
                break;
            total += d;
        }
        prev = cell;
    }
    return total;
}

// v21: does the move follow the tiles the route was checked on?
bool follows_route(PostMove const &pm) const
{
    if (pm.path.empty() || pm.path.size() > route_path_.size())
        return false;
    for (std::size_t i = 0; i < pm.path.size(); ++i)
        if (index(pm.path[i]) != route_path_[i])
            return false;
    return true;
}

// ---------------------------------------------------------------------------
// v21: free moves. A dragon of length L may take ceil(L/4) steps a turn at no cost
// (game update). Besides the single steps, weigh moves of 2 up to that many steps
// (at most FREE_MOVE_MAX_STEPS), each through tiles we can see now and no portal,
// played out exactly as the engine would (simulate_path). The most promising end
// tiles (a quick look: food on the way and goal progress) are then evaluated in full,
// like a single step.
// ---------------------------------------------------------------------------
void add_free_moves(std::vector<Candidate> &candidates) const
{
    int const L = controller_.get_length();
    int const free = std::min(free_moves(L), Config::FREE_MOVE_MAX_STEPS);
    if (free < 2)
        return;
    // Late game: the swarm gathers on the king and feeds it, and feeders (short: one
    // step a turn) cannot catch a king that roams at several. The king and growers
    // take free moves then only to eat (a route), fight, or get out of trouble.
    if (retaining_length() && game_.get_round_num() >= Config::GATHER_ROUND && base_goal_ == Goal::EXPLORE &&
        goal_ != Goal::FIGHT)
        return;
    struct Option
    {
        std::vector<Direction> steps;
        PostMove pm;
        int quick = 0;
    };
    std::vector<Option> options;
    std::unordered_map<int, std::size_t> by_end; // (end tile, last direction) -> option
    std::vector<Direction> steps;
    std::function<void(Position, Direction)> walk = [&](Position at, Direction heading)
    {
        for (Direction d : Direction::get_direction_list())
        {
            if (d == heading.get_opposite())
                continue;
            Step const st = step_from(at, d);
            if (!st.passable || !st.destination_known || st.portal || !controller_.get_tile(st.destination))
                continue;
            steps.push_back(d);
            PostMove pm;
            if (simulate_path(steps, pm, false))
            {
                if (steps.size() >= 2)
                {
                    int const key = index(pm.head) * 4 + direction_index(d);
                    int const q = quick_value(pm);
                    auto const it = by_end.find(key);
                    if (it == by_end.end())
                    {
                        by_end[key] = options.size();
                        options.push_back(Option{steps, pm, q});
                    }
                    else if (q > options[it->second].quick)
                        options[it->second] = Option{steps, pm, q};
                }
                if (static_cast<int>(steps.size()) < free)
                    walk(st.destination, d);
            }
            steps.pop_back();
        }
    };
    walk(controller_.get_position(), controller_.get_dir());
    std::sort(options.begin(), options.end(), [](Option const &a, Option const &b)
              { return a.quick > b.quick; });
    if (static_cast<int>(options.size()) > Config::FREE_MOVE_EVALS)
        options.resize(static_cast<std::size_t>(Config::FREE_MOVE_EVALS));
    for (Option const &o : options)
        candidates.push_back(evaluate_path(o.steps, o.pm));
}

// ---------------------------------------------------------------------------
// v21: rush. A long dragon has many free steps (a king of 80: 20 a turn), but a free
// move only crosses tiles we can see, so it gains at most 3 tiles a turn towards a
// pearl outside our window. When we have a route, follow its checked path for all our
// free steps, also over tiles we cannot see now but have seen free: an edge we know,
// no body remembered there. A body may have moved onto such a tile since: one seen k
// rounds ago is taken with chance k x the fill rate we measure around us, but no more
// than the share of tiles we see taken (an empty part of the map stays safe however
// long ago we saw it); running into it kills us, so the chance of that is priced at
// our death and a rush riskier than RUSH_MAX_RISK is not safe. Weighed: all of it,
// stopping on each route pearl, and the part we can see.
// ---------------------------------------------------------------------------
double blind_tile_risk(int c) const
{
    // A tile our own body is on now: nobody else can be there, and nothing else moves
    // while we do (following our own body round is safe however far it goes).
    if (own_idx_[static_cast<std::size_t>(c)] != -1)
        return 0.0;
    CellMemory const &m = memory_[static_cast<std::size_t>(c)];
    int const age = std::max(1, game_.get_round_num() - m.last_seen);
    return std::min({Config::P_BLIND_TILE_CAP, age * fill_rate_x1e4_ / 10000.0, occupancy_x1e4_ / 10000.0});
}

void add_route_rush(std::vector<Candidate> &candidates) const
{
    if (route_path_.empty() || goal_ != Goal::EAT)
        return;
    int const L = controller_.get_length();
    int const free = free_moves(L);
    if (free < 2)
        return;
    // The route path as steps, as far as our free steps go (not through portals).
    std::vector<Direction> dirs;
    Position at = controller_.get_position();
    for (std::size_t k = 0; k < route_path_.size() && static_cast<int>(dirs.size()) < free; ++k)
    {
        bool found = false;
        for (Direction d : Direction::get_direction_list())
        {
            Step const st = step_from(at, d);
            if (st.passable && st.destination_known && !st.portal && index(st.destination) == route_path_[k])
            {
                dirs.push_back(d);
                at = st.destination;
                found = true;
                break;
            }
        }
        if (!found)
            break;
    }
    if (dirs.size() < 2)
        return;
    std::vector<std::size_t> lengths{dirs.size()};
    std::size_t seen = 0; // steps through tiles we can see
    while (seen < dirs.size() && controller_.get_tile(position_of(route_path_[seen])))
        ++seen;
    if (seen >= 2)
        lengths.push_back(seen);
    for (std::size_t k = 2; k < dirs.size(); ++k)
        for (int j : route_seq_)
            if (route_cells_[static_cast<std::size_t>(j)] == route_path_[k - 1])
                lengths.push_back(k);
    std::sort(lengths.begin(), lengths.end());
    lengths.erase(std::unique(lengths.begin(), lengths.end()), lengths.end());
    for (std::size_t k : lengths)
    {
        // A fully visible move this short is already among the free moves.
        if (k <= seen && static_cast<int>(k) <= Config::FREE_MOVE_MAX_STEPS)
            continue;
        std::vector<Direction> steps(dirs.begin(), dirs.begin() + static_cast<std::ptrdiff_t>(k));
        PostMove pm;
        std::vector<int> blind;
        if (!simulate_path(steps, pm, false, true, &blind))
            continue;
        Candidate c = evaluate_path(steps, pm);
        c.rush = true;
        c.known_space = -1; // what we cannot see is priced below, not by the known-space rule
        // Pearls remembered on tiles we cannot see: worth what is left of them.
        int const now = game_.get_round_num();
        for (int b : blind)
        {
            CellMemory const &m = memory_[static_cast<std::size_t>(b)];
            if (m.pearl)
                c.score += static_cast<int>(food_value(position_of(b)) * std::max(0.15, 1.0 - (now - m.pearl_seen) / 40.0));
        }
        double clear = 1.0;
        for (int b : blind)
            clear *= 1.0 - blind_tile_risk(b);
        double const p_hit = 1.0 - clear;
        c.score -= risk(p_hit, death_cost());
        if (p_hit > (valuable_now_ ? Config::RUSH_MAX_RISK_VALUABLE : Config::RUSH_MAX_RISK))
            c.critical_ok = c.safe = false;
        candidates.push_back(std::move(c));
    }
}

// Pearls eaten on the way (each counted once) and what they are worth.
int path_food(PostMove const &pm) const
{
    int v = 0;
    for (std::size_t i = 0; i < pm.path.size(); ++i)
    {
        bool again = false;
        for (std::size_t j = 0; j < i; ++j)
            if (pm.path[j] == pm.path[i])
                again = true;
        if (!again && visible_pearl(pm.path[i]))
            v += food_value(pm.path[i]);
    }
    return v;
}

// A quick look at a free move, to choose which to evaluate in full: food on the
// way, goal progress (for pearls along the route: by the pearls' own maps rather
// than the time-aware search), and a way on from where it ends.
int quick_value(PostMove const &pm) const
{
    Goal const g = goal_ == Goal::FIGHT ? base_goal_ : goal_;
    int v = path_food(pm) + post_exits(pm) * Config::EXIT_VALUE;
    if (g == Goal::EAT)
    {
        // As route_remaining, every leg by the pearls' maps.
        int after = 0;
        int prev = index(pm.head);
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
                if (prev == index(pm.head))
                    after = route_remaining_now_ + pm.steps; // the next pearl is out of reach from there
                break;
            }
            after += d;
            prev = cell;
        }
        v += std::clamp(route_remaining_now_ - after, -pm.steps, pm.steps) * Config::STEP_EAT;
    }
    else
    {
        Candidate c;
        v += goal_progress(g, c, pm);
    }
    return v;
}

// The whole value of a free move, gathered and priced as for a single step
// (evaluate_single, move_value): goal progress, food on the way, room and risks
// where it ends (the survival search too), the fight from there, small habits.
Candidate evaluate_path(std::vector<Direction> const &steps, PostMove const &pm) const
{
    int const L = controller_.get_length();
    Candidate c;
    c.steps = steps;
    c.free_move = true;
    c.destination = pm.head;
    c.destination_known = true;
    c.new_length = pm.length;
    c.direct_pearl = pm.eaten > 0;
    std::size_t const hc = static_cast<std::size_t>(index(pm.head));
    c.enemy_threats = enemy_threats_[hc];
    c.ally_threats = ally_threats_[hc];
    c.hard_threats = hard_threat_[hc];
    c.soft_threats = soft_threat_[hc];
    c.exits = post_exits(pm);
    c.space = timed_space(pm, space_cap(pm.length), false, &c.region);
    int const need = pm.length + unknown_body();
    bool const trap = c.region.trap(need);
    if (valuable_now_)
        c.known_space = timed_space(pm, pm.length + 3, true);
    SurvivalResult const sr = survival(pm, steps.back());
    c.dfs_depth = sr.depth;
    c.dfs_target = sr.target;
    c.dfs_complete = sr.complete;
    c.walled = sr.walled; // v23

    bool const keeper = retaining_length();
    bool const fight = goal_ == Goal::FIGHT;
    int v = deterministic_jitter(steps.front()) / 5;
    int const progress = goal_progress(fight ? base_goal_ : goal_, c, pm);
    v += fight ? static_cast<int>(progress * Config::FIGHT_BASE_PULL) : progress;
    v += path_food(pm);
    v -= paid_steps(L, pm.steps) * SEG; // none within our free steps
    v += position_value(c, pm, fight);
    if (fight)
        v += fight_value(pm, steps.back());
    if (!good_bed(cell(pm.head))) // v23: farming a good bed is not wandering in circles
        v -= cell(pm.head).visits * (keeper ? Config::KEEPER_VISIT_COST : Config::VISIT_COST);
    if (!c.direct_pearl)
        v += teammate_spacing_score(pm.head);
    for (Position const &p : pm.path)
        if (unswbc::Tile const *tile = controller_.get_tile(p))
        {
            int const countdown = tile->get_pearl_time();
            if (!tile->has_pearl() && countdown >= 1 && countdown <= c.new_length - 1)
                v -= Config::BLOCKED_SPAWN; // our body would sit on it when it spawns
        }
    if (!fight)
    {
        auto const [enemy_before, enemy_after] = nearest_visible_enemy_delta(controller_.get_position(), pm.head);
        if (enemy_before < INF)
            v += (enemy_after - enemy_before) * enemy_distance_step();
    }
    c.score = v;
    bool const room_ok = c.dfs_ok() || (!c.dfs_dead() && c.exits > 0 && c.space >= need + 2 && !trap);
    c.critical_ok = room_ok && (!valuable_now_ || c.hard_threats == 0);
    c.safe = c.critical_ok && (c.enemy_threats == 0 || fight);
    return c;
}