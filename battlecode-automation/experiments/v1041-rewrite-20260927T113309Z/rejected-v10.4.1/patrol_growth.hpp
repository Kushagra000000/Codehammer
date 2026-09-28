// Included inside ReplayDerivedBot. Ordinary-turn detours, never paid sprints.
std::vector<Position> pearl_detour_;
int detour_round_ = -1;
bool patrol_rejoin_pending_ = false;

int patrol_productivity(Claim const &c) const
{
    int const area = c.w * c.h, length = controller_.get_length();
    if (area < std::max(80, length + Config::PATROL_GROWTH_ROOM))
        return 0;
    int rate = 0;
    for (int y = c.y; y < c.y + c.h; ++y)
        for (int x = c.x; x < c.x + c.w; ++x)
        {
            auto const &m = cell(Position(x, y));
            if (m.pearl_time < 0)
                continue; // a dropped pearl does not make this a recurring food source
            // Before observing a reset, count spawn capability conservatively.
            rate += m.spawn_gap_x16 > 0 ? std::max(1, 16384 / m.spawn_gap_x16) : 16;
        }
    // Production density discounted by the fraction not covered by our body.
    return static_cast<int>(1000LL * rate * (area - length) / (area * area));
}

bool ordered_on_cycle(std::deque<Position> const &body,
                      std::vector<Position> const &cycle, bool allow_outside) const
{
    if (body.empty() || cycle.size() < body.size() + Config::PATROL_GROWTH_ROOM)
        return false;
    std::vector<int> order(memory_.size(), -1);
    int const n = static_cast<int>(cycle.size());
    for (int i = 0; i < n; ++i)
        order[index(cycle[i])] = i;
    int const head = order[index(body.front())];
    if (head < 0)
        return false;
    // Visible future pearls can delay release. Unknown future spawns still require
    // the normal per-turn safety checks; this is not a global survival proof.
    std::vector<int> growth(n, 0);
    for (int d = 1; d < n; ++d)
        growth[d] = growth[d - 1] + (visible_pearl(cycle[(head + d) % n]) ? 1 : 0);
    int behind = 0, previous = head;
    for (int i = 1; i < static_cast<int>(body.size()); ++i)
    {
        int const at = order[index(body[i])];
        if (at < 0)
        {
            if (!allow_outside)
                return false;
            continue;
        }
        int const delta = (previous - at + n) % n;
        if (delta == 0 || (behind += delta) >= n)
            return false;
        previous = at;
        int const forward = (at - head + n) % n;
        // Collision is checked BEFORE the tail moves. Off-cycle body segments
        // also consume release time, hence body.size()-i, not a cycle-only count.
        if (forward <= static_cast<int>(body.size()) - i + growth[forward])
            return false;
    }
    return true;
}

bool detour_returns_safely(std::vector<Position> const &path, int &pearls) const
{
    pearls = 0;
    if (!body_complete() || path.empty())
        return false;
    std::deque<Position> body = own_trail_;
    Position from = controller_.get_position();
    std::vector<Position> visited;
    for (Position to : path)
    {
        bool edge_ok = false;
        for (Direction d : Direction::get_direction_list())
            if (known_clear_edge(from, d) && from.add_dir(d) == to)
                edge_ok = true;
        if (!edge_ok || !controller_.get_tile(to) ||
            other_occ_[index(to)] || hard_threat_[index(to)] > 0 || enemy_threats_[index(to)] > 0 ||
            std::find(body.begin(), body.end(), to) != body.end() ||
            std::find(visited.begin(), visited.end(), to) != visited.end())
            return false;
        // Separate ordinary moves: pearls stall the tail; no sprint segment cost.
        bool const eats = visible_pearl(to);
        body.push_front(to);
        if (eats)
            ++pearls;
        else
            body.pop_back();
        visited.push_back(to);
        from = to;
    }
    if (!ordered_on_cycle(body, patrol_, true))
        return false;
    auto const last = std::find(patrol_.begin(), patrol_.end(), body.front());
    Position const next = patrol_[(last - patrol_.begin() + 1) % patrol_.size()];
    // A return is not useful if the next cycle tile is already occupied by an ally
    // or enemy. Own occupancy/release was checked against the projected body above.
    return !other_occ_[index(next)] && hard_threat_[index(next)] == 0;
}

bool detour_improves_food(std::vector<Position> const &path) const
{
    auto const start = std::find(patrol_.begin(), patrol_.end(), controller_.get_position());
    if (start == patrol_.end())
        return false;
    int detour_food = 0, patrol_food = 0, detour_earliness = 0, patrol_earliness = 0;
    for (int i = 0; i < static_cast<int>(path.size()); ++i)
    {
        int const weight = static_cast<int>(path.size()) - i;
        if (visible_pearl(path[i]))
        {
            ++detour_food;
            detour_earliness += weight;
        }
        Position const normal = patrol_[(start - patrol_.begin() + i + 1) % patrol_.size()];
        if (visible_pearl(normal))
        {
            ++patrol_food;
            patrol_earliness += weight;
        }
    }
    // Spend late turns only for extra food, or the same amount sooner than the
    // normal cycle. Do not lure the king away from a richer run of cycle pearls.
    return detour_food > patrol_food ||
           (detour_food == patrol_food && detour_earliness > patrol_earliness);
}

void update_pearl_detour()
{
    if (game_.get_round_num() < Config::PATROL_FORAGE_ROUND ||
        !retaining_length() || feeder_target_ || controller_.get_length() < 16 ||
        !Config::PATROL_ENABLED || !body_complete() || patrol_.empty())
    {
        pearl_detour_.clear();
        patrol_rejoin_pending_ = false;
        return;
    }
    Position const here = controller_.get_position();
    if (!pearl_detour_.empty())
    {
        // Only continue if last turn actually took the requested single step.
        if (detour_round_ + 1 == game_.get_round_num() && here == pearl_detour_.front())
            pearl_detour_.erase(pearl_detour_.begin());
        else
            pearl_detour_.clear();
    }
    int pearls = 0;
    if (!pearl_detour_.empty() && detour_returns_safely(pearl_detour_, pearls))
    {
        detour_round_ = game_.get_round_num();
        return; // still rejoin after eating the last pearl on this excursion
    }
    pearl_detour_.clear();
    bool const on_cycle = std::find(patrol_.begin(), patrol_.end(), here) != patrol_.end();
    bool const recovering = patrol_rejoin_pending_ && !on_cycle;
    if (on_cycle)
        patrol_rejoin_pending_ = false;
    if (!recovering && !ordered_on_cycle(own_trail_, patrol_, true))
        return;
    auto preferred = patrol_direction();
    if (!preferred && !recovering)
        return;
    std::vector<Position> path, best;
    int best_value = 0;
    int const max_steps = std::min(Config::PATROL_DETOUR_STEPS,
                                   std::max(0, 500 - game_.get_round_num()));
    auto search = [&](auto &&self, Position from, int food) -> void
    {
        if (!path.empty() && (food > 0 || recovering))
        {
            int count = 0;
            int const value = (recovering ? 1000 : food * 100) - static_cast<int>(path.size()) * 10;
            if (value > best_value && (recovering || detour_improves_food(path)) &&
                detour_returns_safely(path, count))
            {
                best_value = value;
                best = path;
            }
        }
        if (static_cast<int>(path.size()) >= max_steps)
            return;
        for (Direction d : Direction::get_direction_list())
        {
            if (path.empty() && preferred && d == *preferred)
                continue; // ordinary patrol already gets these pearls
            if (!known_clear_edge(from, d))
                continue;
            Position to = from.add_dir(d);
            // No speculative occupancy, portals, reserved food or planned tail crossings.
            if (!controller_.get_tile(to) || visible_occupied(to) ||
                hard_threat_[index(to)] > 0 || enemy_threats_[index(to)] > 0 ||
                reserved_food(to) || std::find(path.begin(), path.end(), to) != path.end())
                continue;
            path.push_back(to);
            self(self, to, food + (visible_pearl(to) ? 1 : 0));
            path.pop_back();
        }
    };
    search(search, here, 0);
    pearl_detour_ = std::move(best);
    if (!pearl_detour_.empty())
        patrol_rejoin_pending_ = true;
    detour_round_ = game_.get_round_num();
}
