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
    return Config::DISPERSE_ENABLED && !retaining_length() && game_.get_round_num() < Config::GATHER_ROUND &&
           (team_explored_ & ALL_BLOCKS) != ALL_BLOCKS &&
           ((width_ * height_ >= Config::DISPERSE_MIN_AREA && game_.get_round_num() >= Config::DISPERSE_START_ROUND) ||
            scout()); // v27 (test): scouts on every map from round 0
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

// Value of heading to the destination (one step closer: +STEP_DISPERSE).
int disperse_score(Position destination, int k = 1) const // k: steps the move takes (v21)
{
    if (!explore_target_ || target_dist_.empty())
        return 0;
    int const before = target_dist_[static_cast<std::size_t>(index(controller_.get_position()))];
    int const after = target_dist_[static_cast<std::size_t>(index(destination))];
    if (before >= INF || after >= INF)
        return 0;
    return std::clamp(before - after, -k, k) * Config::STEP_DISPERSE;
}

// ---------------------------------------------------------------------------
// Contested pearls: an enemy is in sight and could reach a pearl no later than we
// would by walking. Sprint there by the shortest path, if the pearls eaten pay for
// the segments spent (a sprint must at least break even).
// v21: a pearl within our free steps is a free move (route.hpp), so a sprint is only
// for one further away; "no later" compares the turns each of us needs (prepare_etas).
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
    int const max_steps = std::min(max_turn_steps(L), std::max(Config::PEARL_SPRINT_MAX_STEPS, free_moves(L) + 1));
    int const free = free_moves(L);
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
            if (steps_needed <= free || !visible_pearl(s.destination))
                continue; // within our free steps: a step or a free move
            // Contested: an enemy gets there no later than we would at our free speed.
            int const enemy_turn = enemy_turns_to(n);
            bool const contested = enemy_turn < INF ? enemy_turn <= turns_for(steps_needed, L)
                                                    : std::any_of(enemies.begin(), enemies.end(), [&](Position const &e)
                                                                  { return distance(e, s.destination) <= steps_needed; });
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
        int const net = pm.eaten - paid_steps(L, pm.steps);
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
        SurvivalResult const sr = survival(pm, steps.back()); // v21
        cand.dfs_depth = sr.depth;
        cand.dfs_target = sr.target;
        cand.dfs_complete = sr.complete;
        cand.walled = sr.walled;         // v23
        cand.via_portal = sr.via_portal; // v26
        cand.critical_ok = (cand.dfs_ok() || (!cand.dfs_dead() && cand.exits > 0 && cand.space >= need + 2 && !trap)) &&
                           (!valuable_now_ || cand.hard_threats == 0);
        // Ending next to the enemy head risks a head-on: a worker accepts it.
        cand.safe = cand.critical_ok && (cand.enemy_threats == 0 || !valuable_now_);
        // v20: pearls eaten minus segments paid (a sprint that only breaks even loses
        // to walking there), a little for taking them from under the enemy's nose,
        // then room and risks where it ends, and the fight from there.
        cand.score = (pm.eaten - paid_steps(L, pm.steps)) * SEG + Config::CONTEST_VALUE * pm.eaten +
                     position_value(cand, pm, goal_ == Goal::FIGHT) + deterministic_jitter(steps.front()) / 5;
        if (goal_ == Goal::FIGHT)
            cand.score += fight_value(pm, steps.back());
        best_eaten = pm.eaten;
        best_sprint = std::move(cand);
        found = true;
    }
    if (found)
        candidates.push_back(std::move(best_sprint));
}
// ---------------------------------------------------------------------------
// v27 (test): rich spots (Config::RICH_SHARE). Where the food is: a dragon whose view (7x7 round its
// head) holds beds that refill often reports the spot, everyone relays the best few, and a worker
// with much less food round it heads for the best spot that still has room.
// 63-60 magic 0x8 | 59 team | 58-53 x | 52-47 y | 46-37 score (pearls per 100 rounds, max 1023)
// | 36-31 round seen & 63 | 30-5 zero | 4-0 checksum
// ---------------------------------------------------------------------------
struct RichSpot
{
    Position pos{};
    int score = 0;
    int round = -1000;
};
std::vector<RichSpot> rich_spots_;
int local_rich_ = 0;
bool rich_follow_ = false;
Position rich_target_pos_{};
std::vector<int> rich_dist_;
int rich_send_k_ = 0;

// A bed's food, in pearls per 100 rounds: 100 / its refill gap (from the countdowns seen on it;
// one countdown c seen: about 2c; none: RICH_DEFAULT_GAP).
int bed_rate_x100(CellMemory const &m) const
{
    if (!m.seen || (m.pearl_time < 0 && !m.pearl))
        return 0;
    int gap = Config::RICH_DEFAULT_GAP;
    if (m.cd_n > 0)
        gap = (2 * m.cd_sum - m.cd_n) / m.cd_n;
    else if (m.pearl_time > 0)
        gap = 2 * m.pearl_time;
    return 100 / std::max(2, gap);
}

// The beds we can walk to within RICH_RADIUS steps of `c` (kelp and portals as we know them):
// beds seen across a wall do not count.
int rich_score_at(Position c) const
{
    int s = 0;
    std::vector<std::pair<int, int>> queue{{index(c), 0}};
    for (std::size_t k = 0; k < queue.size(); ++k)
    {
        auto const [at, d] = queue[k];
        s += bed_rate_x100(memory_[static_cast<std::size_t>(at)]);
        if (d >= Config::RICH_RADIUS)
            continue;
        for (Direction dir : Direction::get_direction_list())
        {
            Step const st = step_from(position_of(at), dir);
            if (!st.passable || !st.destination_known)
                continue;
            int const n = index(st.destination);
            bool seen = false;
            for (auto const &q : queue)
                if (q.first == n)
                    seen = true;
            if (!seen)
                queue.push_back({n, d + 1});
        }
    }
    return std::min(s, 1023);
}

void offer_rich(RichSpot const &s)
{
    for (RichSpot &k : rich_spots_)
        if (chebyshev_distance(k.pos, s.pos) <= Config::RICH_MERGE)
        {
            if (s.score > k.score)
            {
                k.pos = s.pos;
                k.score = s.score;
            }
            k.round = std::max(k.round, s.round);
            return;
        }
    rich_spots_.push_back(s);
    if (static_cast<int>(rich_spots_.size()) > Config::RICH_MAX_SPOTS)
        rich_spots_.erase(std::min_element(rich_spots_.begin(), rich_spots_.end(), [](RichSpot const &a, RichSpot const &b)
                                           { return a.score < b.score; }));
}

std::uint64_t rich_message(RichSpot const &s) const
{
    std::uint64_t m = 0x8ULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    m |= std::uint64_t(s.pos.x & 63) << 53U;
    m |= std::uint64_t(s.pos.y & 63) << 47U;
    m |= std::uint64_t(std::clamp(s.score, 0, 1023)) << 37U;
    m |= std::uint64_t(s.round & 63) << 31U;
    m |= packet_checksum(m, 5);
    return m;
}

// The spots in turn (0: none).
std::uint64_t next_rich_message()
{
    if (rich_spots_.empty())
        return 0;
    RichSpot const &s = rich_spots_[static_cast<std::size_t>(rich_send_k_++ % static_cast<int>(rich_spots_.size()))];
    return rich_message(s);
}

void receive_rich(std::uint64_t m)
{
    if (!Config::RICH_SHARE || (m & 31ULL) != packet_checksum(m & ~31ULL, 5) || ((m >> 5U) & 0x3FFFFFFULL) != 0)
        return;
    int const now = game_.get_round_num();
    Position const p(static_cast<int>((m >> 53U) & 63ULL), static_cast<int>((m >> 47U) & 63ULL));
    int const age = (now - static_cast<int>((m >> 31U) & 63ULL)) & 63;
    if (p.x >= width_ || p.y >= height_ || age > Config::RICH_TTL)
        return;
    offer_rich(RichSpot{p, static_cast<int>((m >> 37U) & 1023ULL), now - age});
}

// Teammates (seen, or told us where they are lately) within the spot's 7x7.
int rich_crowd(Position spot) const
{
    int n = 0;
    for (SeenHead const &h : heads_)
        if (h.team == controller_.get_team() && h.id != my_id() && chebyshev_distance(h.position, spot) <= 3)
            ++n;
    int const now = game_.get_round_num();
    for (TeamSignal const &s : signals_)
        if (s.id != my_id() && !seen_head(s.id) && now - s.round <= Config::CLAIM_SIGNAL_AGE &&
            chebyshev_distance(s.position, spot) <= 3)
            ++n;
    return n;
}

// Once a turn: our own view's score (reported when rich), stale spots dropped, and for a worker
// the spot to head for, with its distance field.
void update_rich()
{
    rich_follow_ = false;
    if (!Config::RICH_SHARE)
        return;
    int const now = game_.get_round_num();
    std::erase_if(rich_spots_, [&](RichSpot const &s)
                  { return now - s.round > Config::RICH_TTL; });
    Position const here = controller_.get_position();
    local_rich_ = rich_score_at(here);
    if (local_rich_ >= Config::RICH_MIN_SCORE)
        offer_rich(RichSpot{here, local_rich_, now});
#ifdef BOT_DEBUG
    controller_.output_log("RICH", local_rich_, static_cast<int>(rich_spots_.size()),
                           rich_spots_.empty() ? -1 : rich_spots_[0].score);
#endif
    if (is_queen() || retaining_length() || valuable_now_ || now >= Config::RICH_END_ROUND || feeder_target_ ||
        Config::RICH_MODE != 0)
        return;
    // What each of us would get: a spot's food shared with the teammates already there, over
    // RICH_HORIZON rounds less the trip; here, ours shared with those round us.
    double const T = Config::RICH_HORIZON;
    double const local_v = local_rich_ / (1.0 + rich_crowd(here));
    auto value = [&](RichSpot const &s, int d)
    { return s.score / (1.0 + rich_crowd(s.pos)) * std::max(0.0, T - d) / T; };
    RichSpot const *best = nullptr;
    double best_v = 0.0;
    for (RichSpot const &s : rich_spots_)
    {
        if (chebyshev_distance(s.pos, here) <= 3)
            continue; // there already
        double const v = value(s, distance(here, s.pos));
        if (v > best_v)
        {
            best_v = v;
            best = &s;
        }
    }
    if (!best || best_v <= Config::RICH_GAIN * local_v + Config::RICH_MIN_GAIN)
        return;
    if (!(best->pos == rich_target_pos_) || rich_dist_.size() != memory_.size() || now % 4 == 0)
    {
        rich_target_pos_ = best->pos;
        rich_dist_.assign(memory_.size(), INF);
        std::deque<Position> queue{best->pos};
        rich_dist_[static_cast<std::size_t>(index(best->pos))] = 0;
        while (!queue.empty())
        {
            Position const p = queue.front();
            queue.pop_front();
            int const d = rich_dist_[static_cast<std::size_t>(index(p))];
            for (Direction dir : Direction::get_direction_list())
            {
                Step const st = step_from(p, dir);
                if (!st.passable || !st.destination_known)
                    continue;
                std::size_t const n = static_cast<std::size_t>(index(st.destination));
                if (rich_dist_[n] <= d + 1)
                    continue;
                rich_dist_[n] = d + 1;
                queue.push_back(st.destination);
            }
        }
    }
    int const d = rich_dist_[static_cast<std::size_t>(index(here))];
    rich_follow_ = d < INF && value(*best, d) > Config::RICH_GAIN * local_v + Config::RICH_MIN_GAIN;
#ifdef BOT_DEBUG
    controller_.output_log("RICHF", local_rich_, static_cast<int>(rich_spots_.size()), rich_target_pos_.x, rich_target_pos_.y,
                           best->score, rich_dist_[static_cast<std::size_t>(index(here))], rich_follow_ ? 1 : 0);
#endif
}

// STEP_RICH per step closer to the spot (k: steps the move takes).
int rich_pull(Position destination, int k = 1) const
{
    if (!rich_follow_)
        return 0;
    int const before = rich_dist_[static_cast<std::size_t>(index(controller_.get_position()))];
    int const after = rich_dist_[static_cast<std::size_t>(index(destination))];
    if (before >= INF || after >= INF)
        return 0;
    return std::clamp(before - after, -k, k) * Config::STEP_RICH;
}