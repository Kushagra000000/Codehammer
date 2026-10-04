// bot-v15.0 layer 2: pearl objectives, gravity field, rival ETA contest.
// Class-body fragment: included inside ReplayDerivedBot (like endgame.hpp).

        bool blocks_pearl_path(int c) const
        {
            std::size_t const i = static_cast<std::size_t>(c);
            if (other_occ_[i] || (own_idx_[i] != -1 && own_idx_[i] != 0))
                return true;
            CellMemory const &m = memory_[i];
            int const now = game_.get_round_num();
            return m.last_seen != now && m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY;
        }

        // An ally that reaches the pearl first will take it; chasing it too wastes a
        // worker. Ties go to the lower id, which moves first. Growers never yield.
        bool pearl_taken_by_ally(Position p) const
        {
            if (retaining_length())
                return false;
            int const mine = distance(controller_.get_position(), p);
            int const my_id = controller_.get_id();
            for (SeenHead const &h : heads_)
            {
                if (h.team != controller_.get_team())
                    continue;
                int const d = distance(h.position, p);
                if (d < mine || (d == mine && h.id < my_id))
                    return true;
            }
            int const now = game_.get_round_num();
            for (TeamSignal const &sig : signals_)
            {
                if (sig.id == my_id || now - sig.round > 1 || seen_head(sig.id))
                    continue;
                int const d = distance(sig.position, p);
                if (d < mine || (d == mine && sig.id < my_id))
                    return true;
            }
            return false;
        }

        // One multi-source search per turn from every pearl objective outwards.
        // Objectives: visible pearls (cost 0), remembered pearls (fading with age),
        // tiles about to spawn, and, weakly, unseen tiles whose countdown ran out.
        void build_objective_map()
        {
            std::fill(objective_dist_.begin(), objective_dist_.end(), INF);
            int const now = game_.get_round_num();
            int const max_cost = Config::PEARL_PULL_RADIUS + 1;
            bool const late = now >= SPECIALISE_ROUND; // no pearl is reserved before this
            std::vector<std::vector<int>> buckets(static_cast<std::size_t>(max_cost + 1));
            auto add = [&](int c, int cost)
            {
                if (cost > max_cost || cost >= objective_dist_[static_cast<std::size_t>(c)])
                    return;
                objective_dist_[static_cast<std::size_t>(c)] = cost;
                buckets[static_cast<std::size_t>(cost)].push_back(c);
            };
            // v15: every objective seed is also a gravity source.
            food_objectives_.clear();
            auto seed = [&](int c, int cost)
            {
                if (cost > max_cost)
                    return;
                add(c, cost);
                food_objectives_.push_back({c, cost});
            };
            int const cells = width_ * height_;
            for (int c = 0; c < cells; ++c)
            {
                CellMemory const &m = memory_[static_cast<std::size_t>(c)];
                if (!m.seen || blocks_pearl_path(c) || own_idx_[static_cast<std::size_t>(c)] == 0)
                    continue;
                Position const p = position_of(c);
                bool const visible = m.last_seen == now;
                if (m.pearl)
                {
                    int const age = now - m.pearl_seen;
                    if (age > Config::REMEMBERED_PEARL_MAX_AGE || (late && reserved_food(p)) || pearl_taken_by_ally(p))
                        continue;
                    seed(c, age / Config::REMEMBERED_PEARL_AGE_DIV);
                    continue;
                }
                if (m.pearl_time < 0 || m.pearl_sample_round < 0)
                    continue; // never spawns
                int const remaining = m.pearl_sample_round + m.pearl_time - now;
                if (remaining > 0)
                {
                    // Arrive the move after it spawns, not before (we would block it).
                    if (remaining <= Config::SPAWN_SOON_ROUNDS && !(late && reserved_food(p)) && !pearl_taken_by_ally(p))
                        seed(c, remaining + 1);
                }
                else if (!visible && -remaining <= Config::EXPIRED_TIMER_MAX_AGE && !(late && reserved_food(p)))
                {
                    seed(c, Config::EXPIRED_TIMER_BASE_COST + (-remaining) / Config::EXPIRED_TIMER_AGE_DIV);
                }
            }
            for (int d = 0; d < max_cost; ++d)
            {
                auto &bucket = buckets[static_cast<std::size_t>(d)];
                for (std::size_t k = 0; k < bucket.size(); ++k)
                {
                    int const c = bucket[k];
                    if (objective_dist_[static_cast<std::size_t>(c)] != d)
                        continue;
                    Position const p = position_of(c);
                    for (Direction dir : Direction::get_direction_list())
                    {
                        Step const st = step_from(p, dir);
                        if (!st.passable || !st.destination_known)
                            continue;
                        int const n = index(st.destination);
                        if (blocks_pearl_path(n))
                            continue;
                        add(n, d + 1);
                    }
                }
            }
        }

// ---------------------------------------------------------------------------
// v15 layer 2: inverse-square gravity field with a rival ETA contest
// ---------------------------------------------------------------------------
struct FoodObjective
{
    int cell = 0;
    int cost = 0; // extra steps charged for age or spawn wait
};
std::vector<FoodObjective> food_objectives_;
std::vector<int> food_src_cost_;            // per cell: cheapest objective cost here, INF if none
std::vector<int> enemy_eta_, ally_eta_;     // per cell: rival heads' path distance
mutable std::vector<int> grav_stamp_, grav_dist_;
mutable int grav_gen_ = 0;
mutable int best_gravity_ = 0;              // best candidate gravity this turn (layer 4 trigger)

void ensure_food_buffers()
{
    std::size_t const n = memory_.size();
    if (food_src_cost_.size() == n)
        return;
    food_src_cost_.assign(n, INF);
    enemy_eta_.assign(n, INF);
    ally_eta_.assign(n, INF);
    grav_stamp_.assign(n, 0);
    grav_dist_.assign(n, INF);
}

// Multi-source BFS from rival heads. Allies only count when we would yield to them
// (retaining dragons and the queen never yield to allies, as in v10.3.3).
void multi_source_eta(std::vector<int> &out, std::vector<Position> const &sources)
{
    std::fill(out.begin(), out.end(), INF);
    std::deque<int> queue;
    for (Position const &p : sources)
    {
        int const c = index(p);
        if (out[static_cast<std::size_t>(c)] == 0)
            continue;
        out[static_cast<std::size_t>(c)] = 0;
        queue.push_back(c);
    }
    while (!queue.empty())
    {
        int const c = queue.front();
        queue.pop_front();
        int const d = out[static_cast<std::size_t>(c)];
        if (d >= Config::RIVAL_ETA_RADIUS)
            continue;
        Position const p = position_of(c);
        for (Direction dir : Direction::get_direction_list())
        {
            Step const s = step_from(p, dir);
            if (!s.passable || !s.destination_known)
                continue;
            int const n = index(s.destination);
            if (out[static_cast<std::size_t>(n)] <= d + 1 || blocks_pearl_path(n))
                continue;
            out[static_cast<std::size_t>(n)] = d + 1;
            queue.push_back(n);
        }
    }
}

void build_food_field()
{
    ensure_food_buffers();
    std::fill(food_src_cost_.begin(), food_src_cost_.end(), INF);
    for (FoodObjective const &o : food_objectives_)
        food_src_cost_[static_cast<std::size_t>(o.cell)] = std::min(food_src_cost_[static_cast<std::size_t>(o.cell)], o.cost);
    std::vector<Position> enemies, allies;
    for (SeenHead const &h : heads_)
        (h.team == controller_.get_team() ? allies : enemies).push_back(h.position);
    int const now = game_.get_round_num();
    for (TeamSignal const &sig : signals_)
        if (sig.id != controller_.get_id() && now - sig.round <= 1 && !seen_head(sig.id))
            allies.push_back(sig.position);
    multi_source_eta(enemy_eta_, enemies);
    if (retaining_length() || is_queen())
        allies.clear();
    multi_source_eta(ally_eta_, allies);
    best_gravity_ = 0;
}

// Sum over objectives within GRAVITY_RADIUS of weight / (eta + 1)^2, where eta counts
// this move, the path from the destination and the objective's cost. A pearl a rival
// reaches first is damped hard; a tie is damped mildly.
int food_gravity(Position destination, bool grow) const
{
    if (food_src_cost_.size() != memory_.size())
        return 0;
    int const weight = grow ? Config::GROWER_GRAVITY_WEIGHT : Config::GRAVITY_WEIGHT;
    ++grav_gen_;
    int const gen = grav_gen_;
    int const start = index(destination);
    if (blocks_pearl_path(start))
        return 0;
    grav_stamp_[static_cast<std::size_t>(start)] = gen;
    grav_dist_[static_cast<std::size_t>(start)] = 0;
    std::deque<int> queue{start};
    long long total = 0;
    while (!queue.empty())
    {
        int const c = queue.front();
        queue.pop_front();
        int const d = grav_dist_[static_cast<std::size_t>(c)];
        int const src = food_src_cost_[static_cast<std::size_t>(c)];
        if (src < INF)
        {
            int const arrival = 1 + d; // moves until our head is on it
            int const eta = arrival + src;
            long long pull = weight / ((eta + 1) * (eta + 1));
            int const enemy = enemy_eta_[static_cast<std::size_t>(c)];
            int const ally = ally_eta_[static_cast<std::size_t>(c)];
            int const rival = std::min(enemy, ally);
            if (rival < arrival)
                pull /= Config::GRAVITY_CONTEST_DIV;
            else if (rival == arrival)
                pull /= Config::GRAVITY_TIE_DIV;
            total += pull;
        }
        if (d >= Config::GRAVITY_RADIUS)
            continue;
        Position const p = position_of(c);
        for (Direction dir : Direction::get_direction_list())
        {
            Step const s = step_from(p, dir);
            if (!s.passable || !s.destination_known)
                continue;
            int const n = index(s.destination);
            if (grav_stamp_[static_cast<std::size_t>(n)] == gen || blocks_pearl_path(n))
                continue;
            grav_stamp_[static_cast<std::size_t>(n)] = gen;
            grav_dist_[static_cast<std::size_t>(n)] = d + 1;
            queue.push_back(n);
        }
    }
    int const g = static_cast<int>(std::min<long long>(total, Config::GRAVITY_CAP));
    best_gravity_ = std::max(best_gravity_, g);
    return g;
}
