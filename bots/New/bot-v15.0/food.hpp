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
                    add(c, age / Config::REMEMBERED_PEARL_AGE_DIV);
                    continue;
                }
                if (m.pearl_time < 0 || m.pearl_sample_round < 0)
                    continue; // never spawns
                int const remaining = m.pearl_sample_round + m.pearl_time - now;
                if (remaining > 0)
                {
                    // Arrive the move after it spawns, not before (we would block it).
                    if (remaining <= Config::SPAWN_SOON_ROUNDS && !(late && reserved_food(p)) && !pearl_taken_by_ally(p))
                        add(c, remaining + 1);
                }
                else if (!visible && -remaining <= Config::EXPIRED_TIMER_MAX_AGE && !(late && reserved_food(p)))
                {
                    add(c, Config::EXPIRED_TIMER_BASE_COST + (-remaining) / Config::EXPIRED_TIMER_AGE_DIV);
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
