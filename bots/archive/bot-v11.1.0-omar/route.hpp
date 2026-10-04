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

std::vector<std::vector<int>> route_fields_; // per candidate: path distance to it
std::vector<int> route_cells_;               // candidate pearl tiles
std::vector<double> route_weight_;
std::vector<int> route_head_dist_;
int route_first_ = -1;     // candidate we are heading for, or -1
int last_route_cell_ = -1; // last turn's target (hysteresis)
int route_planned_ = 0;    // pearls in the chosen route (debug)
int route_next_cell_ = -1; // first tile of the path we checked (the move to prefer)
std::vector<int> sim_occ_; // scratch for replays

struct RouteState
{
    int pos = 0;
    int t = 0;
    double value = 0.0;
    std::uint32_t mask = 0;
    std::array<std::int8_t, 8> seq{};
    int len = 0;
};

// Path distances from `src` (kelp, other dragons and our body block; portals count),
// out to max_depth steps.
void route_bfs(Position src, int max_depth, std::vector<int> &out) const
{
    int const now = game_.get_round_num();
    out.assign(memory_.size(), INF);
    std::deque<int> queue{index(src)};
    out[static_cast<std::size_t>(index(src))] = 0;
    while (!queue.empty())
    {
        int const c = queue.front();
        queue.pop_front();
        int const d = out[static_cast<std::size_t>(c)];
        if (d >= max_depth)
            continue;
        Position const p = position_of(c);
        for (Direction dir : Direction::get_direction_list())
        {
            Step const st = step_from(p, dir);
            if (!st.passable || !st.destination_known)
                continue;
            int const n = index(st.destination);
            std::size_t const ni = static_cast<std::size_t>(n);
            if (out[ni] <= d + 1 || blocks_pearl_path(n) || trap_mark_[ni] > now)
                continue;
            out[ni] = d + 1;
            queue.push_back(n);
        }
    }
}

// Walk the route as the engine would move us: each leg follows its distance map
// downhill (of equally short steps, the one with the most free tiles around it: no
// preference for going straight), our body trails behind and grows with every pearl.
// False if we would hit ourselves or get stuck.
bool replay_route(RouteState const &s, int *first_cell = nullptr)
{
    if (sim_occ_.size() != memory_.size())
        sim_occ_.assign(memory_.size(), 0);
    std::deque<int> snake;
    for (Position const &p : own_trail_)
        snake.push_back(index(p));
    for (int c : snake)
        ++sim_occ_[static_cast<std::size_t>(c)];
    int length = controller_.get_length();
    int const unknown_free = length - known_body_ + 1; // segments we cannot place free by then
    std::vector<int> eaten;
    bool ok = true;
    int cur = index(controller_.get_position());
    Direction heading = controller_.get_dir();
    int t = 0;
    for (int k = 0; k < s.len && ok; ++k)
    {
        std::vector<int> const &f = route_fields_[static_cast<std::size_t>(s.seq[static_cast<std::size_t>(k)])];
        while (ok && f[static_cast<std::size_t>(cur)] > 0)
        {
            int const want = f[static_cast<std::size_t>(cur)] - 1;
            int next = -1;
            int next_free = -1;
            Direction next_dir = heading;
            for (Direction d : Direction::get_direction_list())
            {
                if (d == heading.get_opposite())
                    continue;
                Step const st = step_from(position_of(cur), d);
                if (!st.passable || !st.destination_known)
                    continue;
                std::size_t const n = static_cast<std::size_t>(index(st.destination));
                if (f[n] != want || sim_occ_[n] > 0)
                    continue;
                if (own_idx_[n] == -2 && t + 1 < unknown_free + static_cast<int>(eaten.size()))
                    continue;
                int free_nb = 0;
                for (Direction e : Direction::get_direction_list())
                {
                    Step const st2 = step_from(st.destination, e);
                    if (st2.passable && st2.destination_known &&
                        sim_occ_[static_cast<std::size_t>(index(st2.destination))] == 0 &&
                        !other_occ_[static_cast<std::size_t>(index(st2.destination))])
                        ++free_nb;
                }
                if (free_nb > next_free)
                {
                    next = static_cast<int>(n);
                    next_dir = d;
                    next_free = free_nb;
                }
            }
            if (next < 0)
            {
                ok = false;
                break;
            }
            ++t;
            if (t == 1 && first_cell)
                *first_cell = next;
            heading = next_dir;
            cur = next;
            snake.push_front(cur);
            ++sim_occ_[static_cast<std::size_t>(cur)];
            bool ate = false;
            if (memory_[static_cast<std::size_t>(cur)].pearl &&
                std::find(eaten.begin(), eaten.end(), cur) == eaten.end())
            {
                eaten.push_back(cur);
                ate = true;
                ++length;
            }
            if (!ate)
                while (static_cast<int>(snake.size()) > length)
                {
                    --sim_occ_[static_cast<std::size_t>(snake.back())];
                    snake.pop_back();
                }
        }
    }
    // The route must not end with our head boxed in by our own body.
    if (ok)
    {
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
    for (int c : snake)
        --sim_occ_[static_cast<std::size_t>(c)];
    return ok;
}

void plan_route()
{
    route_first_ = -1;
    route_planned_ = 0;
    route_next_cell_ = -1;
    if (!Config::ROUTING_ENABLED || (feeder_target_ && !retaining_length()))
        return; // feeders walk to the king
    int const now = game_.get_round_num();
    bool const keeper = retaining_length();
    bool const late = now >= SPECIALISE_ROUND;
    int const radius = keeper ? Config::KEEPER_ROUTE_RADIUS : Config::WORKER_ROUTE_RADIUS;
    int const horizon = keeper ? Config::KEEPER_ROUTE_HORIZON : Config::WORKER_ROUTE_HORIZON;
    int const max_age = keeper ? Config::KEEPER_REMEMBERED_PEARL_MAX_AGE : Config::REMEMBERED_PEARL_MAX_AGE;
    Position const head = controller_.get_position();
    route_bfs(head, radius, route_head_dist_);

    // Candidates: the closest pearls we can still win.
    std::vector<std::pair<int, int>> found; // (distance, cell)
    for (std::size_t c = 0; c < memory_.size(); ++c)
    {
        CellMemory const &m = memory_[c];
        if (!m.pearl || route_head_dist_[c] >= INF || route_head_dist_[c] == 0 || now - m.pearl_seen > max_age)
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
    std::size_t const k = found.size();
    route_cells_.assign(k, 0);
    route_weight_.assign(k, 1.0);
    route_fields_.resize(k);
    for (std::size_t i = 0; i < k; ++i)
    {
        int const c = found[i].second;
        route_cells_[i] = c;
        CellMemory const &m = memory_[static_cast<std::size_t>(c)];
        double w = m.last_seen == now ? 1.0 : std::max(0.15, 1.0 - (now - m.pearl_seen) / 40.0);
        Position const pp = position_of(c);
        // An enemy head closer to it than we are will probably get there first; for
        // the king and growers an enemy head anywhere near it is a fight.
        for (SeenHead const &h : heads_)
            if (h.team != controller_.get_team())
            {
                int const de = distance(h.position, pp);
                if (de < found[i].first)
                    w *= 0.5;
                else if (keeper && de <= Config::ROUTE_ENEMY_NEAR)
                    w *= Config::ROUTE_KEEPER_ENEMY_FACTOR;
            }
        // Piles of pearls are often where dragons died: a crowd. Other dragons' segments
        // around it (seen now, or a moment ago) make it worth less.
        int crowd = 0;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            {
                std::size_t const q = static_cast<std::size_t>(index(Position(wrap(pp.x + dx, width_), wrap(pp.y + dy, height_))));
                CellMemory const &qm = memory_[q];
                if (other_occ_[q] || (qm.other_seen && now - qm.last_seen <= Config::OCCUPIED_MEMORY))
                    ++crowd;
            }
        w /= 1.0 + crowd * (keeper ? Config::ROUTE_KEEPER_CROWD : Config::ROUTE_WORKER_CROWD);
        route_weight_[i] = w;
        route_bfs(position_of(c), horizon, route_fields_[i]); // no leg can be longer
    }

    // Beam search over visiting orders.
    double const gamma = keeper ? Config::KEEPER_ROUTE_DISCOUNT : Config::WORKER_ROUTE_DISCOUNT;
    std::vector<double> disc(static_cast<std::size_t>(horizon + 1), 1.0);
    for (int t = 1; t <= horizon; ++t)
        disc[static_cast<std::size_t>(t)] = disc[static_cast<std::size_t>(t - 1)] * gamma;
    std::vector<RouteState> frontier(1);
    frontier[0].pos = index(head);
    std::vector<RouteState> all;
    for (int depth = 0; depth < 8 && !frontier.empty(); ++depth)
    {
        std::vector<RouteState> next;
        for (RouteState const &s : frontier)
            for (std::size_t j = 0; j < k; ++j)
            {
                if (s.mask & (1U << j))
                    continue;
                int const d = route_fields_[j][static_cast<std::size_t>(s.pos)];
                if (d >= INF || d == 0 || s.t + d > horizon)
                    continue;
                RouteState n = s;
                n.pos = route_cells_[j];
                n.t = s.t + d;
                n.value += route_weight_[j] * disc[static_cast<std::size_t>(n.t)];
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
    int checked = 0;
    for (RouteState const &s : all)
    {
        if (checked++ >= Config::ROUTE_REPLAYS)
            break;
        if (replay_route(s, &route_next_cell_))
        {
            route_first_ = s.seq[0];
            route_planned_ = s.len;
            last_route_cell_ = route_cells_[static_cast<std::size_t>(route_first_)];
            return;
        }
    }
    // Every good route runs into ourselves: fall back to the nearest pearl we can
    // at least reach safely on its own.
    for (RouteState const &s : all)
        if (s.len == 1 && replay_route(s, &route_next_cell_))
        {
            route_first_ = s.seq[0];
            route_planned_ = 1;
            last_route_cell_ = route_cells_[static_cast<std::size_t>(route_first_)];
            return;
        }
}

// Steps from p to the pearl we are heading for (INF when not on its map).
int route_dist(Position p) const
{
    return route_fields_[static_cast<std::size_t>(route_first_)][static_cast<std::size_t>(index(p))];
}