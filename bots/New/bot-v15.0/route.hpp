// bot-v15.0 layer 4: global route planner, portal memory, vision memory.
// Class-body fragment: included inside ReplayDerivedBot (like endgame.hpp).

        EdgeKey canonical_edge(Position position, Direction direction) const
        {
            switch (direction.value)
            {
            case Direction::NORTH:
                return {true, position.x, position.y};
            case Direction::SOUTH:
                return {true, position.x, wrap(position.y + 1, height_)};
            case Direction::WEST:
                return {false, position.x, position.y};
            default:
                return {false, wrap(position.x + 1, width_), position.y};
            }
        }

        void remember_portal(int portal_id, EdgeKey endpoint)
        {
            std::vector<EdgeKey> &endpoints = portal_edges_[portal_id];
            if (std::find(endpoints.begin(), endpoints.end(), endpoint) == endpoints.end())
                endpoints.push_back(endpoint);
        }

        std::optional<Position> portal_exit(Position position, Direction direction,
                                            int portal_id) const
        {
            auto const found = portal_edges_.find(portal_id);
            if (found == portal_edges_.end() || found->second.size() != 2)
                return std::nullopt;

            EdgeKey const entry = canonical_edge(position, direction);
            EdgeKey const *other = nullptr;
            for (EdgeKey const &endpoint : found->second)
            {
                if (!(endpoint == entry))
                {
                    other = &endpoint;
                    break;
                }
            }
            if (!other)
                return std::nullopt;

            if (other->horizontal)
            {
                if (direction.value == Direction::NORTH)
                    return Position(other->x, wrap(other->y - 1, height_));
                if (direction.value == Direction::SOUTH)
                    return Position(other->x, other->y);
                return std::nullopt;
            }
            if (direction.value == Direction::WEST)
                return Position(wrap(other->x - 1, width_), other->y);
            if (direction.value == Direction::EAST)
                return Position(other->x, other->y);
            return std::nullopt;
        }

        // Memory holds every visible edge (remember_vision runs first), so no
        // per-step vision lookup is needed.
        Step step_from(Position position, Direction direction) const
        {
            RememberedEdge const &edge = cell(position).edges[static_cast<std::size_t>(direction_index(direction))];
            if (edge.known && edge.type == EdgeType::KELP)
                return {};
            if (edge.known && edge.type == EdgeType::PORTAL)
            {
                std::optional<Position> const exit = portal_exit(position, direction, edge.portal_id);
                if (!exit)
                    return {true, false, true, position};
                return {true, true, true, *exit};
            }
            return {true, true, false, position.add_dir(direction)};
        }

        // NEW: remember_vision incorporates symmetry deduction
        void remember_vision()
        {
            int const round = game_.get_round_num();
            for (unswbc::Tile const &tile : controller_.get_tiles())
            {
                Position pos = tile.get_position();
                CellMemory &remembered = cell(pos);
                remembered.seen = true;
                remembered.last_seen = round;
                remembered.pearl = tile.has_pearl();
                remembered.pearl_seen = tile.has_pearl() ? round : -1;
                remembered.pearl_time = tile.get_pearl_time();
                remembered.pearl_sample_round = round;
                remembered.other_seen = tile.get_dragon() && tile.get_dragon()->get_id() != controller_.get_id();

                for (Direction direction : Direction::get_direction_list())
                {
                    unswbc::Edge const &edge = tile.get_edge(direction);
                    RememberedEdge &known = remembered.edges[static_cast<std::size_t>(direction_index(direction))];
                    known = {true, edge.get_edge_type(), edge.get_portal_id()};
                    if (edge.is_portal())
                        remember_portal(edge.get_portal_id(),
                                        canonical_edge(pos, direction));
                }
                // Symmetry deduction remains disabled (see v10).
            }
        }

// ---------------------------------------------------------------------------
// v15 layer 4: global BFS route planner
// ---------------------------------------------------------------------------
// When no pearl pulls hard locally, BFS from the head through known portals and
// unseen cells to the nearest remembered pearl, or failing that the nearest
// exploration frontier (an unseen or long-unseen cell), and boost the first step.
mutable std::vector<int> route_stamp_, route_first_;
mutable int route_gen_ = 0;
int route_dir_ = -1;   // direction index chosen this turn, -1 if none
int route_kind_ = 0;   // 1 pearl, 2 frontier

int plan_route()
{
    std::size_t const cells = memory_.size();
    if (route_stamp_.size() != cells)
    {
        route_stamp_.assign(cells, 0);
        route_first_.assign(cells, -1);
    }
    ++route_gen_;
    int const gen = route_gen_;
    int const now = game_.get_round_num();
    bool const late = now >= SPECIALISE_ROUND;
    Position const here = controller_.get_position();
    Direction const facing = controller_.get_dir();
    int const start = index(here);
    route_stamp_[static_cast<std::size_t>(start)] = gen;
    std::deque<std::pair<int, int>> queue{{start, 0}};
    int frontier_dir = -1;
    route_kind_ = 0;
    while (!queue.empty())
    {
        auto const [c, d] = queue.front();
        queue.pop_front();
        if (c != start)
        {
            CellMemory const &m = memory_[static_cast<std::size_t>(c)];
            int const first = route_first_[static_cast<std::size_t>(c)];
            if (m.seen && m.pearl && now - m.pearl_seen <= Config::ROUTE_PEARL_MAX_AGE)
            {
                Position const p = position_of(c);
                if (!(late && reserved_food(p)) && !pearl_taken_by_ally(p))
                {
                    route_kind_ = 1;
                    return first;
                }
            }
            if (frontier_dir < 0 && (!m.seen || now - m.last_seen >= Config::FRONTIER_STALE_ROUNDS))
                frontier_dir = first;
            if (!m.seen)
                continue; // walk into the unknown, but do not plan through it
        }
        if (d >= Config::ROUTE_MAX_DIST)
            continue;
        Position const p = position_of(c);
        for (Direction dir : Direction::get_direction_list())
        {
            if (c == start && dir == facing.get_opposite())
                continue;
            Step const s = step_from(p, dir);
            if (!s.passable || !s.destination_known)
                continue;
            int const n = index(s.destination);
            if (route_stamp_[static_cast<std::size_t>(n)] == gen || blocks_pearl_path(n))
                continue;
            route_stamp_[static_cast<std::size_t>(n)] = gen;
            route_first_[static_cast<std::size_t>(n)] =
                c == start ? direction_index(dir) : route_first_[static_cast<std::size_t>(c)];
            queue.push_back({n, d + 1});
        }
    }
    if (frontier_dir >= 0)
        route_kind_ = 2;
    return frontier_dir;
}

void apply_route_boost(std::vector<Candidate> &candidates)
{
    route_dir_ = -1;
    if (best_gravity_ >= Config::ROUTE_MIN_GRAVITY || feeding_role() ||
        (territory_.w && retaining_length()))
        return;
    route_dir_ = plan_route();
    if (route_dir_ < 0)
        return;
    int const boost = route_kind_ == 1 ? Config::ROUTE_PEARL_BOOST : Config::ROUTE_FRONTIER_BOOST;
    for (Candidate &c : candidates)
        if (!c.head_trade && c.steps.size() == 1 && direction_index(c.steps.front()) == route_dir_)
            c.score += boost;
}
