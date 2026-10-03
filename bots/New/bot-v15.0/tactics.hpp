// bot-v15.0 layers 1 and 5: priority cascade (assassination, feeding, mitosis) and panic escapes.
// Class-body fragment: included inside ReplayDerivedBot (like endgame.hpp).

        // ---------------------------------------------------------------
        // v11: aggression
        // ---------------------------------------------------------------

        bool eligible_target(SeenHead const &h) const
        {
            int const round = game_.get_round_num();
            if (h.team == controller_.get_team() || round < Config::AGGRESSION_START_ROUND ||
                retaining_length())
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
            if (retaining_length() || L < 3 || game_.get_round_num() < Config::AGGRESSION_START_ROUND)
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
        void add_escape_sprints(std::vector<Candidate> &candidates) const
        {
            if (!valuable_now_)
                return;
            int const L = controller_.get_length();
            int const max_steps = std::min(L - 2, Config::MAX_ESCAPE_STEPS);
            if (max_steps < 2)
                return;
            if (std::any_of(candidates.begin(), candidates.end(),
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
            if (!retaining_length() && !hard_everywhere && !trapped)
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
