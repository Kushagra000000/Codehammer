// bot-v15.0 layer 3: forward simulation, time-aware space, budgeted DFS lookahead.
// Class-body fragment: included inside ReplayDerivedBot (like endgame.hpp).

        // Segment index of a cell after the move in pm (-1 when free).
        int post_index(PostMove const &pm, int c) const
        {
            int const k = static_cast<int>(pm.path.size());
            for (int m = k; m >= 1; --m)
                if (index(pm.path[static_cast<std::size_t>(m - 1)]) == c)
                {
                    int const i = k - m;
                    return i < pm.length ? i : -1;
                }
            int i = own_idx_[static_cast<std::size_t>(c)];
            if (i == -1)
                return -1;
            if (i == -2)
                i = known_body_; // earliest index it could have: frees last
            i += pm.steps;
            return i < pm.length ? i : -1;
        }

        // Replays a multi-step move exactly as the engine applies it: pay check,
        // collision against the body as the previous step left it, pearl growth,
        // and the extra tail segment per sprint step.
        bool simulate_path(std::vector<Direction> const &steps, PostMove &pm, bool attack_final) const
        {
            pm = PostMove{};
            Position current = controller_.get_position();
            int const L = controller_.get_length();
            int eaten = 0;
            for (std::size_t j = 0; j < steps.size(); ++j)
            {
                int const s = static_cast<int>(j);
                int const length_now = s == 0 ? L : L + eaten - (s - 1);
                if (s >= 1 && length_now <= 2)
                    return false;
                Step const step = step_from(current, steps[j]);
                if (!step.passable || !step.destination_known)
                    return false;
                Position const q = step.destination;
                int const c = index(q);
                bool const last = j + 1 == steps.size();
                if (!controller_.get_tile(q) && s >= 1)
                    return false; // sprints stay inside what we can see
                if (other_occ_[static_cast<std::size_t>(c)])
                {
                    if (!attack_final || !last)
                        return false;
                    unswbc::DragonPart const *part = visible_part(q);
                    if (!part || !part->is_head() || part->get_team() == controller_.get_team())
                        return false;
                    pm.path.push_back(q);
                    pm.head = q;
                    pm.steps = static_cast<int>(steps.size());
                    pm.eaten = eaten;
                    pm.length = L + eaten - (pm.steps - 1);
                    return true;
                }
                if (attack_final && last)
                    return false; // the target head is gone
                // Own body as the previous step left it.
                for (int m = 1; m <= s; ++m)
                    if (pm.path[static_cast<std::size_t>(m - 1)] == q && s - m < length_now)
                        return false;
                int oi = own_idx_[static_cast<std::size_t>(c)];
                if (oi == -2)
                    oi = known_body_;
                if (oi >= 0 && oi + s < length_now)
                {
                    bool revisited = false;
                    for (int m = 1; m <= s; ++m)
                        if (pm.path[static_cast<std::size_t>(m - 1)] == q)
                            revisited = true;
                    if (!revisited)
                        return false;
                }
                bool already = false;
                for (Position const &p : pm.path)
                    if (p == q)
                        already = true;
                if (!already && visible_pearl(q))
                    ++eaten;
                pm.path.push_back(q);
                current = q;
            }
            pm.head = current;
            pm.steps = static_cast<int>(steps.size());
            pm.eaten = eaten;
            pm.length = L + eaten - (pm.steps - 1);
            return true;
        }

        // Cells reachable from the post-move head when our own segments free up as
        // the tail moves. Segment i may be entered from move L-i+1 on. Waiting for a
        // segment is only allowed when there is already room to wander meanwhile.
        template <class IndexFn>
        int timed_space_generic(Position head, int length, IndexFn const &body_index, int cap, bool known_only) const
        {
            int const now = game_.get_round_num();
            ++generation_;
            int const gen = generation_;
            // Item: (arrival, cell << 7 | eaten << 1 | waited).
            //  - waited: reached through a wait. Only unwaited cells count as room to
            //    stall in, otherwise the estimate snowballs through the body it waits for.
            //  - eaten: pearls on the path. Each one stalls the tail a turn, so our
            //    segments free that much later.
            using Item = std::pair<int, int>;
            std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
            int const start = index(head);
            stamp_[static_cast<std::size_t>(start)] = gen;
            best_[static_cast<std::size_t>(start)] = 0;
            pq.push({0, start << 7});
            int settled = 0;
            int stall_room = 0;
            while (!pq.empty() && settled < cap)
            {
                auto const [t, packed] = pq.top();
                pq.pop();
                int const c = packed >> 7;
                int const eaten = (packed >> 1) & 63;
                bool const waited = (packed & 1) != 0;
                if (done_[static_cast<std::size_t>(c)] == gen || best_[static_cast<std::size_t>(c)] != t)
                    continue;
                done_[static_cast<std::size_t>(c)] = gen;
                ++settled;
                if (!waited)
                    ++stall_room;
                Position const p = position_of(c);
                for (Direction d : Direction::get_direction_list())
                {
                    Step const s = step_from(p, d);
                    if (!s.passable || !s.destination_known)
                        continue;
                    int const n = index(s.destination);
                    if (done_[static_cast<std::size_t>(n)] == gen || other_occ_[static_cast<std::size_t>(n)])
                        continue;
                    int arrive = t + 1;
                    bool wait_here = waited;
                    int const oi = body_index(n);
                    if (oi == 0 || oi == -3) // -3: static part of us
                        continue;
                    CellMemory const &m = memory_[static_cast<std::size_t>(n)];
                    if (oi < 0 && m.last_seen != now)
                    {
                        // Out of sight: a body seen there just now is probably still there.
                        if (m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY)
                            continue;
                        if (known_only && !(m.seen && !m.other_seen && now - m.last_seen <= Config::KNOWN_FREE_MEMORY))
                            continue;
                    }
                    if (oi > 0)
                    {
                        int const free_at = length - oi + 1 + eaten;
                        if (free_at > arrive)
                        {
                            if (free_at - arrive > stall_room / 2)
                                continue;
                            arrive = free_at;
                            wait_here = true;
                        }
                    }
                    int const next_eaten = std::min(63, eaten + (m.pearl && m.last_seen == now ? 1 : 0));
                    if (stamp_[static_cast<std::size_t>(n)] != gen || arrive < best_[static_cast<std::size_t>(n)])
                    {
                        stamp_[static_cast<std::size_t>(n)] = gen;
                        best_[static_cast<std::size_t>(n)] = arrive;
                        pq.push({arrive, (n << 7) | (next_eaten << 1) | (wait_here ? 1 : 0)});
                    }
                }
            }
            return settled;
        }

        int timed_space(PostMove const &pm, int cap, bool known_only = false) const
        {
            return timed_space_generic(pm.head, pm.length, [&](int c)
                                       { return post_index(pm, c); }, cap, known_only);
        }

        // Free neighbours of the post-move head at the next move (every segment still blocks).
        int post_exits(PostMove const &pm) const
        {
            int exits = 0;
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(pm.head, d);
                if (!s.passable)
                    continue;
                if (!s.destination_known)
                {
                    ++exits;
                    continue;
                }
                int const n = index(s.destination);
                if (other_occ_[static_cast<std::size_t>(n)] || post_index(pm, n) >= 0)
                    continue;
                ++exits;
            }
            return exits;
        }

        // Crowd risk for a valuable dragon standing at pm.head after the move.
        int crowd_penalty(PostMove const &pm) const
        {
            int robust = 0;
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(pm.head, d);
                if (!s.passable || !s.destination_known)
                    continue;
                std::size_t const n = static_cast<std::size_t>(index(s.destination));
                if (other_occ_[n] || post_index(pm, static_cast<int>(n)) >= 0)
                    continue;
                if (enemy_threats_[n] == 0 && ally_threats_[n] == 0)
                    ++robust;
            }
            int penalty = robust == 0 ? Config::NO_ROBUST_EXIT_PENALTY : robust == 1 ? Config::ONE_ROBUST_EXIT_PENALTY
                                                                                     : 0;
            int crowd = 0;
            for (int dy = -Config::CROWD_RADIUS; dy <= Config::CROWD_RADIUS; ++dy)
                for (int dx = -Config::CROWD_RADIUS; dx <= Config::CROWD_RADIUS; ++dx)
                {
                    Position const q(wrap(pm.head.x + dx, width_), wrap(pm.head.y + dy, height_));
                    if (other_occ_[static_cast<std::size_t>(index(q))])
                        ++crowd;
                }
            return penalty + crowd * Config::CROWD_PENALTY_PER_SEGMENT;
        }

        int space_cap(int length) const
        {
            return std::max(64, length + 20);
        }

        // Valuable dragons: prefer moves that keep the tail reachable through tiles we can
        // vouch for. Scored relative to the best option, so it still ranks moves when no
        // option reaches the full body length (long kings rarely see their whole route).
        void apply_known_space_penalties(std::vector<Candidate> &candidates) const
        {
            if (!valuable_now_)
                return;
            int best = -1;
            for (Candidate const &c : candidates)
                if (!c.head_trade && c.destination_known && c.known_space >= 0)
                    best = std::max(best, std::min(c.known_space, c.new_length + 2));
            if (best < 0)
                return;
            for (Candidate &c : candidates)
            {
                if (c.head_trade || !c.destination_known || c.known_space < 0)
                    continue;
                int const deficit = std::max(0, best - c.known_space);
                c.score -= std::min(Config::KNOWN_SPACE_PENALTY_CAP, deficit * Config::KNOWN_SPACE_WEIGHT);
            }
        }

// ---------------------------------------------------------------------------
// v15 layer 3: forward DFS lookahead with an explicit operations budget
// ---------------------------------------------------------------------------
// Walks future head paths from the post-move state. Our own segments clear as the
// tail moves (segment i of the post-move body frees on move L - i + 1), and every
// pearl eaten on the simulated path delays that by one move. Other dragons' visible
// segments, and recently seen ones out of sight, are walls. An unknown portal exit
// counts as success (optimistic). Returns the deepest depth reached.
int dfs_rec(PostMove const &pm, int depth, int eaten, int required,
            std::array<int, Config::LOOKAHEAD_MAX_DEPTH + 1> &path,
            int &ops, int budget, bool &cut) const
{
    if (depth >= required)
        return depth;
    if (ops >= budget)
    {
        cut = true;
        return depth;
    }
    ++ops;
    int const now = game_.get_round_num();
    int best = depth;
    Position const p = position_of(path[static_cast<std::size_t>(depth)]);
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(p, d);
        if (!s.passable)
            continue;
        if (!s.destination_known)
            return required;
        int const n = index(s.destination);
        int const arrive = depth + 1;
        // v15 rev2 fix 2: another dragon's segment clears tail-first; it blocks only
        // until the ply at which that dragon has moved past it.
        if (other_occ_[static_cast<std::size_t>(n)] && arrive < other_clear_[static_cast<std::size_t>(n)])
            continue;
        CellMemory const &m = memory_[static_cast<std::size_t>(n)];
        if (m.last_seen != now && m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY)
            continue;
        bool body = false;
        int oi = 0;
        for (int k = depth; k >= 0; --k)
            if (path[static_cast<std::size_t>(k)] == n)
            {
                body = true;
                oi = -k; // entered k moves after pm: index -k relative to pm
                break;
            }
        if (!body)
        {
            oi = post_index(pm, n);
            body = oi >= 0;
        }
        if (body && arrive < pm.length - oi + 1 + eaten)
            continue;
        path[static_cast<std::size_t>(arrive)] = n;
        int const next_eaten = eaten + (m.pearl && m.last_seen == now ? 1 : 0);
        best = std::max(best, dfs_rec(pm, arrive, next_eaten, required, path, ops, budget, cut));
        if (best >= required)
            return best;
    }
    return best;
}

// Fractional dead-end penalty: a fully searched shortfall is a fatal trap and costs
// DEAD_END_PENALTY scaled by the missing depth; a shortfall caused by the budget is a
// temporary jam and costs only JAM_PENALTY scaled the same way.
int lookahead_penalty(PostMove const &pm, int budget, int &reached, bool &dead_end) const
{
    int const required = std::min(Config::LOOKAHEAD_MAX_DEPTH,
                                  std::max(Config::LOOKAHEAD_MIN_DEPTH, pm.length));
    std::array<int, Config::LOOKAHEAD_MAX_DEPTH + 1> path{};
    path[0] = index(pm.head);
    int ops = 0;
    bool cut = false;
    reached = dfs_rec(pm, 0, 0, required, path, ops, budget, cut);
    dead_end = false;
    if (reached >= required)
        return 0;
    int const missing = required - reached;
    if (cut)
        return Config::JAM_PENALTY * missing / required;
    dead_end = true;
    return Config::DEAD_END_PENALTY * missing / required;
}

// ---------------------------------------------------------------------------
// v15 rev2 fix 2: when other dragons' visible segments clear
// ---------------------------------------------------------------------------
// For each visible segment of another dragon, count the segments behind it (towards
// its tail) by following the chain of segments whose facing points at it. Other
// dragons move once between our plies, so a segment with b segments behind it is
// free from ply b + 2 (same rule as our own body). A head is a segment like any
// other: it blocks at ply 1 and then becomes the neck. A chain that runs out of
// sight gets OTHER_HIDDEN_TAIL extra segments.
std::vector<int> other_clear_;

void build_other_clear()
{
    if (other_clear_.size() != memory_.size())
        other_clear_.assign(memory_.size(), 0);
    std::fill(other_clear_.begin(), other_clear_.end(), 0);
    std::vector<int> chain;
    for (unswbc::Tile const &tile : controller_.get_tiles())
    {
        unswbc::DragonPart const *start = tile.get_dragon();
        if (!start || start->get_id() == controller_.get_id())
            continue;
        int const c0 = index(tile.get_position());
        if (other_clear_[static_cast<std::size_t>(c0)] > 0)
            continue;
        chain.clear();
        Position cur = tile.get_position();
        int behind_end = 0; // segments behind the last chain cell
        for (int guard = 0; guard < width_ * height_; ++guard)
        {
            int const c = index(cur);
            if (other_clear_[static_cast<std::size_t>(c)] > 0)
            {
                behind_end = other_clear_[static_cast<std::size_t>(c)] - 2 + 1;
                break;
            }
            chain.push_back(c);
            bool found = false, hidden = false;
            Position next{};
            for (Direction d : Direction::get_direction_list())
            {
                Step const s = step_from(cur, d);
                if (!s.passable)
                    continue;
                if (!s.destination_known || !controller_.get_tile(s.destination))
                {
                    hidden = true;
                    continue;
                }
                unswbc::DragonPart const *part = visible_part(s.destination);
                if (!part || part->is_head() || part->get_id() != start->get_id())
                    continue;
                Step const back = step_from(s.destination, part->get_dir());
                if (back.passable && back.destination_known && back.destination == cur)
                {
                    next = s.destination;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                behind_end = hidden ? Config::OTHER_HIDDEN_TAIL : 0;
                chain.push_back(-1); // marker: last real cell is chain[size-2]
                break;
            }
            cur = next;
        }
        if (!chain.empty() && chain.back() == -1)
            chain.pop_back();
        // (If we stopped at an already-labelled cell, behind_end already counts it.)
        // chain[k] has (chain.size() - 1 - k) chain cells behind it, plus behind_end.
        int const k_last = static_cast<int>(chain.size()) - 1;
        for (int k = k_last; k >= 0; --k)
        {
            int const behind = (k_last - k) + behind_end;
            other_clear_[static_cast<std::size_t>(chain[static_cast<std::size_t>(k)])] = behind + 2;
        }
    }
}
