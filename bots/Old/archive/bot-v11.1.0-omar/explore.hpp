// Included inside ReplayDerivedBot (after endgame.hpp).
// v14 exploration: the swarm spreads over the whole map instead of re-exploring
// the spawn area, and contested pearls are grabbed with a sprint.
//
//  - Coverage map: the map is cut into 7x7 blocks; a block is explored once half
//    its tiles have been seen. Dragons share their block masks by sonar (magic 0x9)
//    and hand them to a newborn at every split, so a child starts knowing what the
//    team has already covered instead of knowing nothing.
//  - Destinations: every worker has a destination in a block nobody has explored
//    yet, taken from a low-discrepancy sequence keyed by its id, so dragons born in
//    the same place head for different parts of the map. Reached (the block is
//    explored), unreachable or too slow: the next one in its sequence.
//  - Never-seen tiles in blocks the team already covered are a weaker objective.

// ---------------------------------------------------------------------------
// Coverage map
// ---------------------------------------------------------------------------
static constexpr int XB = 7; // blocks per side
std::array<int, XB * XB> block_cells_{};
std::array<int, XB * XB> block_seen_{};
std::uint64_t own_explored_ = 0;  // blocks we covered ourselves
std::uint64_t team_explored_ = 0; // ours plus what teammates told us
bool blocks_ready_ = false;

static constexpr std::uint64_t ALL_BLOCKS = (1ULL << (XB * XB)) - 1ULL;

int block_of(Position p) const
{
    return (p.y * XB / height_) * XB + (p.x * XB / width_);
}

void ensure_blocks()
{
    if (blocks_ready_)
        return;
    blocks_ready_ = true;
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x)
            ++block_cells_[static_cast<std::size_t>(block_of(Position(x, y)))];
    // Tiny maps: a block may have no tiles at all; it counts as explored.
    for (int b = 0; b < XB * XB; ++b)
        if (block_cells_[static_cast<std::size_t>(b)] == 0)
            own_explored_ |= 1ULL << b;
    team_explored_ |= own_explored_;
}

void note_first_sighting(Position p)
{
    ensure_blocks();
    int const b = block_of(p);
    std::size_t const i = static_cast<std::size_t>(b);
    ++block_seen_[i];
    if (block_seen_[i] * 2 >= block_cells_[i])
    {
        own_explored_ |= 1ULL << b;
        team_explored_ |= 1ULL << b;
    }
}

bool block_explored(int b) const
{
    return ((team_explored_ >> b) & 1ULL) != 0;
}

// 63-60 magic 0x9 | 59 team | 58-10 explored blocks (49 bits) | 9-5 zero | 4-0 checksum
std::uint64_t explore_message() const
{
    std::uint64_t m = 0x9ULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    m |= (team_explored_ & ALL_BLOCKS) << 10U;
    m |= packet_checksum(m, 5);
    return m;
}

void receive_explore(std::uint64_t m)
{
    if ((m & 31ULL) != packet_checksum(m & ~31ULL, 5) || ((m >> 5U) & 31ULL) != 0)
        return;
    ensure_blocks();
    team_explored_ |= (m >> 10U) & ALL_BLOCKS;
}

// At a normal split: the rays that will hit the child carry our coverage map.
void send_explore_handover(int child)
{
    if (!Config::EXPLORE_SHARING || !team_explored_)
        return;
    int const keep = controller_.get_length() - child;
    std::uint64_t const m = explore_message();
    for (Direction d : Direction::get_direction_list())
        if (ray_hits_child(d, keep))
            controller_.send_sonar(d, m);
}

// ---------------------------------------------------------------------------
// Destinations
// ---------------------------------------------------------------------------
std::optional<Position> explore_target_;
int explore_k_ = 0;
int target_since_ = 0;
std::vector<int> target_dist_;

bool dispersing() const
{
    return Config::DISPERSE_ENABLED && width_ * height_ >= Config::DISPERSE_MIN_AREA && !retaining_length() &&
           game_.get_round_num() >= Config::DISPERSE_START_ROUND && game_.get_round_num() < Config::GATHER_ROUND &&
           (team_explored_ & ALL_BLOCKS) != ALL_BLOCKS;
}

// R2 low-discrepancy sequence: consecutive (and interleaved) ids land far apart.
Position sequence_point(int n) const
{
    constexpr double g = 1.32471795724474602596;
    double const fx = std::fmod(0.5 + static_cast<double>(n) / g, 1.0);
    double const fy = std::fmod(0.5 + static_cast<double>(n) / (g * g), 1.0);
    return Position(std::min(width_ - 1, static_cast<int>(fx * width_)),
                    std::min(height_ - 1, static_cast<int>(fy * height_)));
}

void build_target_field(Position aim)
{
    target_dist_.assign(memory_.size(), INF);
    std::deque<Position> queue{aim};
    target_dist_[static_cast<std::size_t>(index(aim))] = 0;
    while (!queue.empty())
    {
        Position const p = queue.front();
        queue.pop_front();
        int const d = target_dist_[static_cast<std::size_t>(index(p))];
        for (Direction dir : Direction::get_direction_list())
        {
            Step const st = step_from(p, dir);
            if (!st.passable || !st.destination_known)
                continue;
            std::size_t const n = static_cast<std::size_t>(index(st.destination));
            if (target_dist_[n] <= d + 1)
                continue;
            target_dist_[n] = d + 1;
            queue.push_back(st.destination);
        }
    }
}

void update_explore_target()
{
    ensure_blocks();
    if (!dispersing())
    {
        explore_target_.reset();
        return;
    }
    int const now = game_.get_round_num();
    int const timeout = std::max(Config::TARGET_TIMEOUT_MIN, width_ + height_);
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (explore_target_ && (block_explored(block_of(*explore_target_)) || now - target_since_ > timeout))
        {
            explore_target_.reset();
            ++explore_k_;
        }
        if (!explore_target_)
        {
            for (int tries = 0; tries < 256 && !explore_target_; ++tries, ++explore_k_)
            {
                Position const p = sequence_point(controller_.get_id() + 64 * explore_k_);
                if (!block_explored(block_of(p)))
                {
                    explore_target_ = p;
                    target_since_ = now;
                }
            }
            if (!explore_target_)
                return;
        }
        build_target_field(*explore_target_);
        // Walled off, as far as we know: take the next destination.
        if (target_dist_[static_cast<std::size_t>(index(controller_.get_position()))] < INF)
            return;
        explore_target_.reset();
        ++explore_k_;
    }
}

// Score for heading to the destination (one step closer: +DISPERSE_PULL).
int disperse_score(Position destination) const
{
    if (!explore_target_ || target_dist_.empty())
        return 0;
    int const before = target_dist_[static_cast<std::size_t>(index(controller_.get_position()))];
    int const after = target_dist_[static_cast<std::size_t>(index(destination))];
    if (before >= INF || after >= INF)
        return 0;
    return std::clamp(before - after, -1, 1) * Config::DISPERSE_PULL;
}

// ---------------------------------------------------------------------------
// Contested pearls: an enemy is in sight and could reach a pearl no later than we
// would by walking. Sprint there by the shortest path, if the pearls eaten pay for
// the segments spent (a sprint must at least break even).
// ---------------------------------------------------------------------------
void add_pearl_sprints(std::vector<Candidate> &candidates) const
{
    int const L = controller_.get_length();
#ifdef NO_PSPRINT
    return;
#endif
    if (L < 3)
        return; // a length-2 dragon cannot pay for a second step
    std::vector<Position> enemies;
    for (SeenHead const &h : heads_)
        if (h.team != controller_.get_team())
            enemies.push_back(h.position);
    if (enemies.empty())
        return;
    int const max_steps = std::min(L - 1, Config::PEARL_SPRINT_MAX_STEPS);
    Position const origin = controller_.get_position();
    Direction const facing = controller_.get_dir();
    ++generation_;
    int const gen = generation_;
    int const start = index(origin);
    stamp_[static_cast<std::size_t>(start)] = gen;
    best_[static_cast<std::size_t>(start)] = 0;
    parent_[static_cast<std::size_t>(start)] = -1;
    std::deque<int> queue{start};
    // Search first, evaluate after: the evaluation (space search) reuses the same
    // scratch arrays, so it must not run while this search is still using them.
    std::vector<std::vector<Direction>> paths;
    int found_depth = INF;
    while (!queue.empty())
    {
        int const c = queue.front();
        queue.pop_front();
        int const depth = best_[static_cast<std::size_t>(c)];
        if (depth >= max_steps || depth >= found_depth)
            continue;
        Position const p = position_of(c);
        for (Direction d : Direction::get_direction_list())
        {
            if (depth == 0 && d == facing.get_opposite())
                continue;
            Step const s = step_from(p, d);
            if (!s.passable || !s.destination_known || !controller_.get_tile(s.destination))
                continue;
            int const n = index(s.destination);
            if (stamp_[static_cast<std::size_t>(n)] == gen || other_occ_[static_cast<std::size_t>(n)] ||
                own_idx_[static_cast<std::size_t>(n)] != -1)
                continue;
            stamp_[static_cast<std::size_t>(n)] = gen;
            best_[static_cast<std::size_t>(n)] = depth + 1;
            parent_[static_cast<std::size_t>(n)] = c;
            parent_dir_[static_cast<std::size_t>(n)] = direction_index(d);
            queue.push_back(n);
            int const steps_needed = depth + 1;
            if (steps_needed < 2 || !visible_pearl(s.destination))
                continue; // an adjacent pearl is a normal step
            bool contested = false;
            for (Position const &e : enemies)
                if (distance(e, s.destination) <= steps_needed)
                    contested = true;
            if (!contested)
                continue;
            std::vector<Direction> steps;
            for (int at = n; parent_[static_cast<std::size_t>(at)] != -1; at = parent_[static_cast<std::size_t>(at)])
                steps.push_back(Direction::get_direction_list()[static_cast<std::size_t>(parent_dir_[static_cast<std::size_t>(at)])]);
            std::reverse(steps.begin(), steps.end());
            paths.push_back(std::move(steps));
            found_depth = steps_needed; // breadth first: only the shortest ones
        }
    }
    Candidate best_sprint;
    bool found = false;
    int best_eaten = -1;
    for (auto const &steps : paths)
    {
        PostMove pm;
        if (!simulate_path(steps, pm, false))
            continue;
        int const net = pm.eaten - (pm.steps - 1);
        if (net < 0 || pm.eaten <= best_eaten) // the pearls must pay for the steps
            continue;
        Candidate cand;
        cand.pearl_sprint = true;
        cand.steps = steps;
        cand.destination = pm.head;
        cand.new_length = pm.length;
        cand.direct_pearl = true;
        std::size_t const hc = static_cast<std::size_t>(index(pm.head));
        cand.hard_threats = hard_threat_[hc];
        cand.soft_threats = soft_threat_[hc];
        cand.enemy_threats = enemy_threats_[hc];
        cand.ally_threats = ally_threats_[hc];
        cand.exits = post_exits(pm);
        cand.space = timed_space(pm, space_cap(pm.length), false, &cand.region);
        int const need = pm.length + unknown_body();
        bool const trap = cand.region.trap(need);
        cand.critical_ok = cand.exits > 0 && cand.space >= need + 2 && !trap &&
                           (!valuable_now_ || cand.hard_threats == 0);
        // Ending next to the enemy head risks a head-on: a worker accepts it.
        cand.safe = cand.critical_ok && (cand.enemy_threats == 0 || !valuable_now_);
        cand.score = Config::PEARL_SPRINT_BONUS + 1000 * pm.eaten - 150 * pm.steps -
                     cand.enemy_threats * Config::ENEMY_THREAT_PENALTY -
                     cand.hard_threats * Config::HARD_THREAT_PENALTY + deterministic_jitter(steps.front());
        best_eaten = pm.eaten;
        best_sprint = std::move(cand);
        found = true;
    }
    if (found)
        candidates.push_back(std::move(best_sprint));
}