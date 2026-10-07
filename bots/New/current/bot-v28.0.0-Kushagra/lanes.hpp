// Included inside ReplayDerivedBot. Known dead-end branches have a direction:
// keep harvesting while walking inward, then preserve the body by leaving from
// the tail. Unknown edges and portals are boundaries, never invented walls.
#ifdef V28_NO_LANES
static constexpr bool LANES_ENABLED = false;
#else
static constexpr bool LANES_ENABLED = true;
#endif
static constexpr int LANE_MAX_AREA = 1099;
static constexpr int LANE_SPAWN_WAIT = 4;

std::vector<int> lane_parent_, lane_depth_;
bool lane_inward_ = false;
int lane_food_ahead_ = -1; // -1: not fully observed now, or another dragon intervenes

bool lane_worker() const
{
    int const now = game_.get_round_num();
    return LANES_ENABLED && memory_.size() <= LANE_MAX_AREA && !is_queen() &&
           !retaining_length() && now < Config::GATHER_ROUND && body_complete() &&
           !escape_child_holding() && !(now > 0 && now == first_round_);
}

void prepare_lanes()
{
    lane_inward_ = false;
    lane_food_ahead_ = -1;
    lane_parent_.clear();
    lane_depth_.clear();
    if (!lane_worker())
        return;
    int const size = static_cast<int>(memory_.size());
    lane_parent_.assign(static_cast<std::size_t>(size), -1);
    lane_depth_.assign(static_cast<std::size_t>(size), 0);
    std::vector<int> degree(static_cast<std::size_t>(size), 0), queue;
    std::vector<unsigned char> pinned(static_cast<std::size_t>(size), 0), removed(static_cast<std::size_t>(size), 0);
    queue.reserve(static_cast<std::size_t>(size));
    for (int c = 0; c < size; ++c)
    {
        Position const p = position_of(c);
        if (!memory_[static_cast<std::size_t>(c)].seen)
            pinned[static_cast<std::size_t>(c)] = 1;
        for (Direction d : Direction::get_direction_list())
        {
            Step const s = step_from(p, d);
            if (!edge_known(p, d) || s.portal || (s.passable && !s.destination_known))
                pinned[static_cast<std::size_t>(c)] = 1;
            if (s.passable && s.destination_known && !s.portal)
                ++degree[static_cast<std::size_t>(c)];
        }
        if (!pinned[static_cast<std::size_t>(c)] && degree[static_cast<std::size_t>(c)] <= 1)
            queue.push_back(c);
    }
    // Peel complete, known leaves. The remaining graph (including unknown
    // boundaries) is the exit side. Every cell is removed at most once.
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(size));
    for (std::size_t q = 0; q < queue.size(); ++q)
    {
        int const c = queue[q];
        if (removed[static_cast<std::size_t>(c)])
            continue;
        removed[static_cast<std::size_t>(c)] = 1;
        order.push_back(c);
        for (Direction d : Direction::get_direction_list())
        {
            Step const s = step_from(position_of(c), d);
            if (!s.passable || !s.destination_known || s.portal)
                continue;
            int const n = index(s.destination);
            if (removed[static_cast<std::size_t>(n)])
                continue;
            lane_parent_[static_cast<std::size_t>(c)] = n;
            if (--degree[static_cast<std::size_t>(n)] <= 1 && !pinned[static_cast<std::size_t>(n)])
                queue.push_back(n);
        }
    }
    // A completely sealed tree has no exit. Do not label it a harvest lane.
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        int const c = *it, p = lane_parent_[static_cast<std::size_t>(c)];
        if (p >= 0 && (!removed[static_cast<std::size_t>(p)] || lane_depth_[static_cast<std::size_t>(p)] > 0))
            lane_depth_[static_cast<std::size_t>(c)] = lane_depth_[static_cast<std::size_t>(p)] + 1;
    }
    if (own_trail_.size() < 2)
        return;
    int const head = index(controller_.get_position());
    lane_inward_ = lane_depth_[static_cast<std::size_t>(head)] > 0 &&
                   lane_parent_[static_cast<std::size_t>(head)] == index(own_trail_[1]);
    if (!lane_inward_)
        return;
    lane_food_ahead_ = 0;
    std::vector<std::pair<int, int>> forward{{head, 0}};
    int const now = game_.get_round_num();
    for (std::size_t q = 0; q < forward.size(); ++q)
    {
        auto const [c, dist] = forward[q];
        CellMemory const &m = memory_[static_cast<std::size_t>(c)];
        if (m.last_seen != now || m.other_seen)
        {
            lane_food_ahead_ = -1;
            return; // stale memory cannot prove either food or depletion
        }
        if (c != head && (m.pearl || (m.pearl_time >= 0 && m.pearl_time <= dist + LANE_SPAWN_WAIT)))
            ++lane_food_ahead_;
        for (Direction d : Direction::get_direction_list())
        {
            Step const s = step_from(position_of(c), d);
            if (!s.passable || !s.destination_known || s.portal)
                continue;
            int const n = index(s.destination);
            if (lane_depth_[static_cast<std::size_t>(n)] > 0 && lane_parent_[static_cast<std::size_t>(n)] == c)
                forward.emplace_back(n, dist + 1);
        }
    }
}

bool lane_blocks_routine_split() const
{
    return lane_worker() && lane_inward_ && lane_food_ahead_ > 0;
}

bool play_lane_harvest_escape(std::vector<Candidate> const &candidates)
{
    int const length = controller_.get_length();
    if (!lane_worker() || !lane_inward_ || lane_food_ahead_ != 0 || length < 4 ||
        !escape_slot_available() || !controller_.can_split(length - 2))
        return false;
    int const head_depth = lane_depth_[static_cast<std::size_t>(index(controller_.get_position()))];
    for (Candidate const &c : candidates)
        if (c.head_trade || (c.safe && c.destination_known &&
                            lane_depth_[static_cast<std::size_t>(index(c.destination))] < head_depth))
            return false; // a useful attack or a safe way back needs no sacrifice
    Position const tail = own_trail_.back();
    int const tc = index(tail);
    // Ordinary workers do not populate hard/soft reach maps. The always-built
    // free-step map also protects the newborn before its first turn this round.
    if (enemy_threats_[static_cast<std::size_t>(tc)] || hard_threat_[static_cast<std::size_t>(tc)] ||
        soft_threat_[static_cast<std::size_t>(tc)])
        return false;
    bool outward_exit = false;
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(tail, d);
        if (!s.passable || !s.destination_known || s.portal || !edge_known(tail, d))
            continue;
        int const n = index(s.destination);
        if (other_occ_[static_cast<std::size_t>(n)] || own_idx_[static_cast<std::size_t>(n)] != -1 ||
            !cell(s.destination).seen || enemy_threats_[static_cast<std::size_t>(n)] ||
            hard_threat_[static_cast<std::size_t>(n)] || soft_threat_[static_cast<std::size_t>(n)])
            continue;
        if (lane_depth_[static_cast<std::size_t>(tc)] == 0 ||
            lane_depth_[static_cast<std::size_t>(n)] < lane_depth_[static_cast<std::size_t>(tc)])
            outward_exit = true;
    }
    if (!outward_exit)
        return false;
    int exits = 0;
    SpaceStats stats;
    int const child = length - 2;
    int const room = child_space(child, child + 20, exits, &stats);
    if (exits == 0 || room < child + 2 || stats.trap(child))
        return false;
    do_escape_split(child, "lane_depleted", room, 0);
    return true;
}
