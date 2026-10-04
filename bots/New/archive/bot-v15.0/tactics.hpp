// bot-v15.0 layers 1 and 5: priority cascade (assassination, feeding, mitosis) and panic escapes.
// Class-body fragment: included inside ReplayDerivedBot (like endgame.hpp).

        // ---------------------------------------------------------------
        // v11: aggression
        // ---------------------------------------------------------------

        bool eligible_target(SeenHead const &h) const
        {
            int const round = game_.get_round_num();
            if (h.team == controller_.get_team() || round < Config::AGGRESSION_START_ROUND ||
                retaining_length() || is_queen())
                return false;
            int const est = h.estimated_length();
            // Endgame: their long dragon decides the tiebreak; anything that is not
            // one of our top two may trade into it.
            if (round >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                return true;
            if (valuable_now_ || controller_.get_unit_count() < Config::MIN_UNITS_FOR_TRADE)
                return false;
            int const L = controller_.get_length();
#if AGGR_MODE == 1
            return L <= Config::HEAD_TRADE_MAX_LENGTH;
#elif AGGR_MODE == 2
            return L <= Config::HEAD_TRADE_MAX_LENGTH || est >= L;
#else
            return L <= Config::HEAD_TRADE_MAX_LENGTH || est >= L + Config::FAVOURABLE_MARGIN;
#endif
        }

        // Worth walking towards, not just trading with when adjacent.
        bool profitable_target(SeenHead const &h) const
        {
            int const est = h.estimated_length();
            if (game_.get_round_num() >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                return true;
#if AGGR_MODE == 1
            return false;
#elif AGGR_MODE == 2
            return est >= std::max(controller_.get_length() + 2, Config::ENEMY_TARGET_MIN_LENGTH);
#else
            return controller_.get_unit_count() >= Config::HUNT_MIN_UNITS &&
                   est >= std::max(controller_.get_length() + Config::HUNT_MARGIN, Config::HUNT_MIN_TARGET_LENGTH);
#endif
        }

        // Multi-step trades spend nothing extra (we die either way), but only chase
        // targets that are worth more than we are, or equals within the old 2-step reach.
        bool sprint_target(SeenHead const &h, int steps) const
        {
            int const est = h.estimated_length();
            if (game_.get_round_num() >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                return true;
#if AGGR_MODE == 1
            return steps <= 2 && controller_.get_length() <= 4;
#elif AGGR_MODE == 2
            (void)steps;
            return true;
#else
            return est >= controller_.get_length() + Config::FAVOURABLE_MARGIN ||
                   (steps <= 2 && controller_.get_length() <= 4);
#endif
        }

        int trade_value(SeenHead const &h) const
        {
            int const est = h.estimated_length();
            int value = Config::TRADE_BASE + Config::TRADE_PER_LENGTH * std::min(est, Config::TRADE_LENGTH_CAP);
            if (game_.get_round_num() >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
                value += Config::ENDGAME_TRADE_BONUS;
            return value;
        }

        // Multi-step head trades: any eligible enemy head within sprint reach through
        // free tiles. Never before AGGRESSION_START_ROUND.
        void add_tactical_sprints(std::vector<Candidate> &candidates) const
        {
            int const L = controller_.get_length();
            if (retaining_length() || is_queen() || L < 3 || game_.get_round_num() < Config::AGGRESSION_START_ROUND)
                return;
            int const max_steps = std::min(L - 1, Config::MAX_ATTACK_STEPS);
            if (max_steps < 2)
                return;
            bool any = false;
            for (SeenHead const &h : heads_)
                if (eligible_target(h))
                    any = true;
            if (!any)
                return;

            Position const origin = controller_.get_position();
            Direction const facing = controller_.get_dir();
            ++generation_;
            int const gen = generation_;
            int const start = index(origin);
            stamp_[static_cast<std::size_t>(start)] = gen;
            best_[static_cast<std::size_t>(start)] = 0;
            parent_[static_cast<std::size_t>(start)] = -1;
            std::deque<int> queue{start};
            std::unordered_set<int> targeted;
            while (!queue.empty())
            {
                int const c = queue.front();
                queue.pop_front();
                int const depth = best_[static_cast<std::size_t>(c)];
                Position const p = position_of(c);
                for (Direction d : Direction::get_direction_list())
                {
                    if (depth == 0 && d == facing.get_opposite())
                        continue;
                    Step const s = step_from(p, d);
                    if (!s.passable || !s.destination_known || !controller_.get_tile(s.destination))
                        continue;
                    int const n = index(s.destination);
                    unswbc::DragonPart const *part = visible_part(s.destination);
                    if (part)
                    {
                        if (depth + 1 < 2 || !part->is_head() || targeted.count(part->get_id()))
                            continue;
                        SeenHead const *h = seen_head(part->get_id());
                        if (!h || !eligible_target(*h) || !sprint_target(*h, depth + 1))
                            continue;
                        std::vector<Direction> steps{d};
                        for (int at = c; parent_[static_cast<std::size_t>(at)] != -1; at = parent_[static_cast<std::size_t>(at)])
                            steps.push_back(Direction::get_direction_list()[static_cast<std::size_t>(parent_dir_[static_cast<std::size_t>(at)])]);
                        std::reverse(steps.begin(), steps.end());
                        PostMove pm;
                        if (!simulate_path(steps, pm, true))
                            continue;
                        targeted.insert(part->get_id());
                        Candidate attack;
                        attack.steps = steps;
                        attack.destination = s.destination;
                        attack.head_trade = true;
                        attack.new_length = pm.length;
                        attack.score = trade_value(*h) - 60 * static_cast<int>(steps.size() - 1) +
                                       deterministic_jitter(steps.front());
                        attack.safe = false;
                        candidates.push_back(std::move(attack));
                        continue;
                    }
                    if (depth + 1 >= max_steps || stamp_[static_cast<std::size_t>(n)] == gen ||
                        own_idx_[static_cast<std::size_t>(n)] != -1)
                        continue;
                    stamp_[static_cast<std::size_t>(n)] = gen;
                    best_[static_cast<std::size_t>(n)] = depth + 1;
                    parent_[static_cast<std::size_t>(n)] = c;
                    parent_dir_[static_cast<std::size_t>(n)] = direction_index(d);
                    queue.push_back(n);
                }
            }
        }

        // When no single step is critically safe for a valuable dragon, pay segments
        // to sprint out of reach or out of a pocket.
        // v15: force skips the gating so layer 5 can ask for escapes on demand.
        void add_escape_sprints(std::vector<Candidate> &candidates, bool force = false) const
        {
            if (!valuable_now_ && !force)
                return;
            int const L = controller_.get_length();
            int const max_steps = std::min(L - 2, Config::MAX_ESCAPE_STEPS);
            if (max_steps < 2)
                return;
            if (!force && std::any_of(candidates.begin(), candidates.end(),
                                      [](Candidate const &c)
                                      { return c.critical_ok; }))
                return;
            // Growers also sprint out of tight space; other long dragons only out of
            // a certain kill zone.
            bool const hard_everywhere = std::all_of(candidates.begin(), candidates.end(),
                                                     [](Candidate const &c)
                                                     { return c.head_trade || c.hard_threats > 0; });
            bool const trapped = std::all_of(candidates.begin(), candidates.end(),
                                             [&](Candidate const &c)
                                             { return c.head_trade || c.space < controller_.get_length(); });
            if (!force && !retaining_length() && !hard_everywhere && !trapped)
                return;

            // Breadth-first over step sequences, one sequence per reached tile.
            std::vector<std::vector<Direction>> frontier{{}};
            std::unordered_set<int> reached{index(controller_.get_position())};
            std::vector<Candidate> found;
            for (int depth = 1; depth <= max_steps && !frontier.empty(); ++depth)
            {
                std::vector<std::vector<Direction>> next;
                for (auto const &prefix : frontier)
                {
                    for (Direction d : Direction::get_direction_list())
                    {
                        if (prefix.empty() && d == controller_.get_dir().get_opposite())
                            continue;
                        std::vector<Direction> steps = prefix;
                        steps.push_back(d);
                        PostMove pm;
                        if (!simulate_path(steps, pm, false))
                            continue;
                        int const cell_index = index(pm.head);
                        if (!reached.insert(cell_index).second)
                            continue;
                        next.push_back(steps);
                        if (depth < 2)
                            continue;
                        Candidate c;
                        c.steps = steps;
                        c.destination = pm.head;
                        c.new_length = pm.length;
                        c.direct_pearl = pm.eaten > 0;
                        c.hard_threats = hard_threat_[static_cast<std::size_t>(cell_index)];
                        c.soft_threats = soft_threat_[static_cast<std::size_t>(cell_index)];
                        c.enemy_threats = enemy_threats_[static_cast<std::size_t>(cell_index)];
                        c.ally_threats = ally_threats_[static_cast<std::size_t>(cell_index)];
                        c.exits = post_exits(pm);
                        c.space = timed_space(pm, space_cap(pm.length));
                        int score = deterministic_jitter(steps.front());
                        score -= (pm.steps - 1) * Config::ESCAPE_SEGMENT_COST;
                        score += pm.eaten * 3800;
                        score -= c.hard_threats * Config::HARD_THREAT_PENALTY +
                                 c.soft_threats * Config::SOFT_THREAT_PENALTY;
                        score -= c.enemy_threats * Config::GROWER_ENEMY_THREAT_PENALTY;
                        score -= c.ally_threats * Config::ALLY_CONGESTION_PENALTY;
                        score += c.exits * 55 + std::min(pm.length + 5, c.space) * 13;
                        if (c.exits == 0)
                            score -= 5000;
                        else if (c.exits == 1)
                            score -= 1300;
                        if (c.space < pm.length + 3)
                            score -= (pm.length + 3 - c.space) * 800;
                        c.known_space = timed_space(pm, pm.length + 3, true);
                        score -= crowd_penalty(pm);
                        score += teammate_spacing_score(pm.head);
                        if (territory_.w && territory_.contains(controller_.get_position()) && !territory_.contains(pm.head))
                            score -= 900;
                        c.critical_ok = c.hard_threats == 0 && c.exits > 0 && c.space >= pm.length + 2;
                        c.safe = c.critical_ok;
                        c.score = score;
                        found.push_back(std::move(c));
                    }
                }
                frontier = std::move(next);
            }
            std::sort(found.begin(), found.end(), [](Candidate const &a, Candidate const &b)
                      { return a.score > b.score; });
            if (found.size() > 4)
                found.resize(4);
            for (Candidate &c : found)
                candidates.push_back(std::move(c));
        }

// ---------------------------------------------------------------------------
// v15 layer 1: priority cascade (fast exits). Order: assassination, sacrificial
// feeding, mitosis. The first that fires acts and ends the turn.
// ---------------------------------------------------------------------------

// Exact sprint to ram the enemy queen's head (ID 0 or 1 on the other team). Killing
// it zeroes their queen for the round-500 tiebreak. Our own queen never does this.
bool cascade_assassinate()
{
    if (!Config::ASSASSINATION_ENABLED || is_queen())
        return false;
    bool visible = false;
    for (SeenHead const &h : heads_)
        if (h.team != controller_.get_team() && h.id <= 1)
            visible = true;
    if (!visible)
        return false;
    int const L = controller_.get_length();
    int const max_steps = std::min(std::max(1, L - 1), Config::ASSASSIN_MAX_STEPS);
    Position const origin = controller_.get_position();
    Direction const facing = controller_.get_dir();
    ++generation_;
    int const gen = generation_;
    int const start = index(origin);
    stamp_[static_cast<std::size_t>(start)] = gen;
    best_[static_cast<std::size_t>(start)] = 0;
    parent_[static_cast<std::size_t>(start)] = -1;
    std::deque<int> queue{start};
    while (!queue.empty())
    {
        int const c = queue.front();
        queue.pop_front();
        int const depth = best_[static_cast<std::size_t>(c)];
        Position const p = position_of(c);
        for (Direction d : Direction::get_direction_list())
        {
            if (depth == 0 && d == facing.get_opposite())
                continue;
            Step const s = step_from(p, d);
            if (!s.passable || !s.destination_known || !controller_.get_tile(s.destination))
                continue;
            int const n = index(s.destination);
            unswbc::DragonPart const *part = visible_part(s.destination);
            if (part)
            {
                if (!part->is_head() || part->get_team() == controller_.get_team() || part->get_id() > 1)
                    continue;
                std::vector<Direction> steps{d};
                for (int at = c; parent_[static_cast<std::size_t>(at)] != -1; at = parent_[static_cast<std::size_t>(at)])
                    steps.push_back(Direction::get_direction_list()[static_cast<std::size_t>(parent_dir_[static_cast<std::size_t>(at)])]);
                std::reverse(steps.begin(), steps.end());
                PostMove pm;
                if (!simulate_path(steps, pm, true))
                    continue;
                controller_.output_log("ASSASSINATE", part->get_id(), static_cast<int>(steps.size()));
                if (steps.size() == 1)
                    controller_.make_move(steps.front());
                else
                    controller_.make_moves(steps);
                record_pending_path(steps);
                send_status(s.destination, std::max(2, pm.length));
                return true;
            }
            if (depth + 1 >= max_steps || stamp_[static_cast<std::size_t>(n)] == gen ||
                own_idx_[static_cast<std::size_t>(n)] != -1)
                continue;
            stamp_[static_cast<std::size_t>(n)] = gen;
            best_[static_cast<std::size_t>(n)] = depth + 1;
            parent_[static_cast<std::size_t>(n)] = c;
            parent_dir_[static_cast<std::size_t>(n)] = direction_index(d);
            queue.push_back(n);
        }
    }
    return false;
}

// Die on our own body so the dropped pearls land beside a longer ally: the v10.3.3
// king feeder (round >= SACRIFICE_ROUND), plus a short dragon right next to our
// queen from QUEEN_FEED_ROUND. The queen never sacrifices itself.
bool cascade_feed()
{
    if (is_queen())
        return false;
    if (play_feeder_strategy())
        return true;
    int const round = game_.get_round_num();
    int const L = controller_.get_length();
    if (round < Config::QUEEN_FEED_ROUND || L > Config::QUEEN_FEED_MAX_LENGTH)
        return false;
    Position const here = controller_.get_position();
    bool queen_near = false;
    for (SeenHead const &h : heads_)
    {
        if (h.team != controller_.get_team())
        {
            if (distance(here, h.position) <= Config::QUEEN_FEED_ENEMY_RADIUS)
                return false; // they would eat it
            continue;
        }
        if (h.id <= 1 && distance(here, h.position) <= Config::QUEEN_FEED_RADIUS)
            queen_near = true;
    }
    if (!queen_near)
        return false;
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(here, d);
        if (!s.passable || !s.destination_known)
            continue;
        unswbc::DragonPart const *part = visible_part(s.destination);
        if (part && !part->is_head() && part->get_id() == controller_.get_id())
        {
            controller_.output_log("FEED_QUEEN", L);
            controller_.make_move(d);
            return true;
        }
    }
    return false;
}

// Split when the swarm has room (should_split checks capacity and roles). The queen
// keeps its body late in the game.
bool cascade_mitosis()
{
    if (is_queen() && game_.get_round_num() >= Config::QUEEN_NO_SPLIT_ROUND)
        return false;
    if (!should_split())
        return false;
    int const child_size = worker_child_size();
    controller_.do_split(child_size);
    send_status(controller_.get_position(), controller_.get_length() - child_size);
    return true;
}

bool run_cascade()
{
    if (cascade_assassinate())
        return true;
    if (cascade_feed())
        return true;
    ++cell(controller_.get_position()).visits;
    return cascade_mitosis();
}

// ---------------------------------------------------------------------------
// v15 layer 5: post-evaluation panic escapes
// ---------------------------------------------------------------------------

// Fatal: no exit, a proven dead end, inside a known enemy sprint reach, or (for a
// dragon without the reach model) next to an enemy head. Trades are deliberate.
bool is_fatal(Candidate const &c) const
{
    if (c.head_trade || !c.destination_known)
        return false;
    return c.exits == 0 || c.dead_end || c.hard_threats > 0 ||
           (!valuable_now_ && c.enemy_threats > 0);
}

Candidate const *best_non_fatal(std::vector<Candidate> const &list) const
{
    Candidate const *best = nullptr;
    for (Candidate const &c : list)
        if (!c.head_trade && !is_fatal(c) && (!best || c.score > best->score))
            best = &c;
    return best;
}

// The queen only splits away its body when its head is certainly dead.
bool panic_split_escape(std::vector<Candidate> const &candidates)
{
    if (is_queen())
    {
        bool const certain_death = candidates.empty() ||
                                   std::all_of(candidates.begin(), candidates.end(), [](Candidate const &c)
                                               { return c.head_trade || c.hard_threats > 0 || c.exits == 0; });
        if (!certain_death)
            return false;
    }
    return play_split_escape(candidates);
}

// The queen's 2-long remnant never kills itself: it still counts for the tiebreak.
bool clear_remnant_v15()
{
    if (is_queen())
    {
        remnant_after_escape_ = false;
        return false;
    }
    return play_clear_remnant();
}

// Returns true when it already acted (split). Otherwise may replace `chosen`.
bool panic_escape(Candidate &chosen, std::vector<Candidate> const &candidates)
{
    if (!Config::PANIC_ENABLED || !is_fatal(chosen))
        return false;
    if (Candidate const *alt = best_non_fatal(candidates))
    {
        controller_.output_log("PANIC_ALT");
        chosen = *alt;
        return false;
    }
    std::vector<Candidate> extra;
    add_escape_sprints(extra, true);
    if (Candidate const *alt = best_non_fatal(extra))
    {
        controller_.output_log("PANIC_SPRINT", static_cast<int>(alt->steps.size()));
        chosen = *alt;
        return false;
    }
    if (controller_.get_length() >= Config::PANIC_SPLIT_MIN_LENGTH && panic_split_escape(candidates))
        return true;
    Candidate const reverse = evaluate_single(controller_.get_dir().get_opposite());
    if (reverse.score > -INF && !is_fatal(reverse))
    {
        controller_.output_log("PANIC_REVERSE");
        chosen = reverse;
    }
    return false;
}
