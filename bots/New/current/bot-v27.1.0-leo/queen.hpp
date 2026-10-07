// Included inside ReplayDerivedBot (after survival.hpp).
// v22: the queen. The engine ranks teams at the end by their queen's length (dragon 0
// or 1: the first dragon each team starts with; dead = 0), then the longest dragon,
// then total length. So the queen:
//  - until QUEEN_SEED_END_ROUND (75) plays as a founder and splits to grow the swarm
//    (the usual small worker splits; she keeps her head, so she stays the queen). After
//    that she never splits, with one exception: when she is certainly dead (every move
//    hopeless, see play_split_escape) she detaches a two-segment child and waits in
//    place, preserving the rest of her length. She repeats while blocked if the split
//    limits allow it, and resumes movement when a way opens. Pocket raids are never hers;
//  - until QUEEN_CYCLE_END_ROUND walks a Hamiltonian cycle on a block of tiles around
//    her, as many of her free steps a turn as are queen-safe (no threat of any kind, our
//    whole length proven to have a way on). Her body trails her along the cycle, so the
//    tiles ahead stay free while the cycle is longer than she is; she walks it only while
//    every tile ahead is free of her body by the time she gets there (queen_body_fits),
//    and only moves to a new block (kelp found, or grown too long) that passes the same
//    test. When the cycle's next tiles are not safe she takes the safest move nearest the
//    cycle; when no cycle fits her she plays as a normal long dragon (never splitting)
//    and looks again every QUEEN_RETRY_ROUNDS;
//  - from QUEEN_CROWN_ROUND wins every king election (a queen flag in the beacons and on
//    the heads we see), so the swarm gathers on her; from SACRIFICE_ROUND she is the
//    king and feeders die in front of her.

// Raw id: the 12-bit ids in our packets wrap past 4096.
bool is_queen() const
{
#ifdef NO_QUEEN
    return false; // (tests: the queen plays like any founder)
#else
    return controller_.get_id() < 2;
#endif
}

// Early game: she grows the swarm like any founder (worker splits; no escape splits).
bool queen_seeding() const
{
    return is_queen() && game_.get_round_num() < Config::QUEEN_SEED_END_ROUND;
}

bool queen_cycling() const
{
    return is_queen() && !queen_seeding() && game_.get_round_num() < Config::QUEEN_CYCLE_END_ROUND;
}

bool queen_crowned() const
{
    return game_.get_round_num() >= Config::QUEEN_CROWN_ROUND;
}

// ---------------------------------------------------------------------------
// The cycle
// ---------------------------------------------------------------------------
std::vector<int> qcycle_;  // the cycle's tiles, in walking order
std::vector<int> qnext_;   // per tile: the next tile on the cycle (-1: not on it)
std::vector<int> qpos_;    // per tile: its place in qcycle_ (-1: not on it)
std::vector<int> qdist_;   // per tile: steps to the nearest cycle tile (kelp aside, bodies ignored)
int qcycle_tried_ = -1000; // last round a cycle was looked for

// The direction stepping from a to b (an ordinary step: no kelp, no portal), or -1.
int plain_dir(int a, int b) const
{
    for (int d = 0; d < 4; ++d)
        if (nb(a, d) == b && !nbr_portal_[static_cast<std::size_t>(4 * a + d)])
            return d;
    return -1;
}

// A Hamiltonian cycle of a w x h grid (h even): along row 0, then a serpentine over
// columns 1..w-1 of rows 1..h-1 (row 1 leftwards), and back up column 0.
static std::vector<std::pair<int, int>> grid_cycle(int w, int h)
{
    std::vector<std::pair<int, int>> seq;
    for (int i = 0; i < w; ++i)
        seq.push_back({i, 0});
    for (int j = 1; j < h; ++j)
    {
        if (j % 2 == 1)
            for (int i = w - 1; i >= 1; --i)
                seq.push_back({i, j});
        else
            for (int i = 1; i < w; ++i)
                seq.push_back({i, j});
    }
    for (int j = h - 1; j >= 1; --j)
        seq.push_back({0, j});
    return seq;
}

// Every step of the cycle still an ordinary step (kelp or a portal found since breaks it).
bool queen_cycle_intact() const
{
    std::size_t const n = qcycle_.size();
    if (n < 4)
        return false;
    for (std::size_t i = 0; i < n; ++i)
        if (plain_dir(qcycle_[i], qcycle_[(i + 1) % n]) < 0)
            return false;
    return true;
}

// Can we walk `cells` (a cycle) from position k onwards (dir +1 or -1) without meeting
// our own body? Segment i of ours (0 = the head) has gone after L - i of our steps, one
// step later for each pearl we eat before it (the ones we know of on the way, plus
// QUEEN_GROWTH_MARGIN). A segment we cannot place in order counts as there for our whole
// length. Also: at least QUEEN_CYCLE_MIN_FREE tiles of the cycle beyond our length.
bool queen_body_fits(std::vector<int> const &cells, std::size_t k, int dir) const
{
    int const L = controller_.get_length();
    int const n = static_cast<int>(cells.size());
    if (n < L + Config::QUEEN_CYCLE_MIN_FREE)
        return false;
    int eaten = Config::QUEEN_GROWTH_MARGIN;
    for (int t = 1; t < n; ++t)
    {
        int const c = cells[static_cast<std::size_t>(((static_cast<int>(k) + dir * t) % n + n) % n)];
        int const i = own_idx_[static_cast<std::size_t>(c)];
        if (i == -2 ? t < L + eaten : i >= 0 && t < L - i + 1 + eaten)
            return false;
        if (i == -1 && memory_[static_cast<std::size_t>(c)].pearl)
            ++eaten;
    }
    return true;
}

// The best block around our head that we can start walking now (queen_body_fits): the
// largest size (from QUEEN_BLOCK_MAX x QUEEN_BLOCK_MAX down, at most max(144, 2 x (length
// + slack)) tiles, at least `min_area`) with some placement whose cycle is all ordinary
// steps; of those placements, the fewest other bodies on it and the fewest tiles never
// seen. Installs it and returns true.
bool build_queen_cycle(int min_area)
{
    int const L = controller_.get_length();
    Position const head = controller_.get_position();
    int const hc = index(head);
    int const max_area = std::max(Config::QUEEN_BLOCK_AREA, 2 * (L + Config::QUEEN_CYCLE_SLACK));
    std::vector<std::pair<int, int>> sizes;
    for (int a = Config::QUEEN_BLOCK_MAX; a >= 4; a -= 2)
        for (int b = a; b >= 4; b -= 2)
        {
            sizes.push_back({a, b});
            if (a != b)
                sizes.push_back({b, a});
        }
    std::stable_sort(sizes.begin(), sizes.end(), [](auto const &x, auto const &y)
                     { return x.first * x.second > y.first * y.second; });
    for (auto const &[w, h] : sizes)
    {
        int const area = w * h;
        if (area > max_area || w > width_ || h > height_)
            continue;
        if (area < min_area)
            break; // the rest are smaller still
        std::vector<std::pair<int, int>> const seq = grid_cycle(w, h);
        std::vector<int> cells(seq.size());
        std::vector<int> best;
        int best_dir = 1;
        int best_score = std::numeric_limits<int>::min();
        for (int oy = 0; oy < h; ++oy)
            for (int ox = 0; ox < w; ++ox)
            {
                int const x0 = head.x - ox, y0 = head.y - oy;
                int unseen = 0, bodies = 0;
                std::size_t k = 0;
                for (std::size_t j = 0; j < seq.size(); ++j)
                {
                    int const c = index(Position(wrap(x0 + seq[j].first, width_), wrap(y0 + seq[j].second, height_)));
                    cells[j] = c;
                    if (c == hc)
                        k = j;
                    if (!memory_[static_cast<std::size_t>(c)].seen)
                        ++unseen;
                    if (other_occ_[static_cast<std::size_t>(c)])
                        ++bodies;
                }
                int const score = -bodies * 50 - unseen;
                if (score <= best_score)
                    continue;
                bool ok = true;
                for (std::size_t j = 0; j < cells.size() && ok; ++j)
                    ok = plain_dir(cells[j], cells[(j + 1) % cells.size()]) >= 0;
                if (!ok)
                    continue;
                int dir = queen_body_fits(cells, k, 1) ? 1 : queen_body_fits(cells, k, -1) ? -1
                                                                                           : 0;
                if (dir == 0)
                    continue;
                best_score = score;
                best = cells;
                best_dir = dir;
            }
        if (best.empty())
            continue;
        if (best_dir < 0)
            std::reverse(best.begin(), best.end());
        qcycle_ = std::move(best);
        qnext_.assign(memory_.size(), -1);
        qpos_.assign(memory_.size(), -1);
        for (std::size_t i = 0; i < qcycle_.size(); ++i)
        {
            qnext_[static_cast<std::size_t>(qcycle_[i])] = qcycle_[(i + 1) % qcycle_.size()];
            qpos_[static_cast<std::size_t>(qcycle_[i])] = static_cast<int>(i);
        }
        // Distance to the cycle (for coming back to it).
        qdist_.assign(memory_.size(), INF);
        std::vector<int> queue(qcycle_);
        for (int c : qcycle_)
            qdist_[static_cast<std::size_t>(c)] = 0;
        for (std::size_t q = 0; q < queue.size(); ++q)
        {
            int const c = queue[q];
            int const d = qdist_[static_cast<std::size_t>(c)];
            if (d >= Config::QUEEN_RETURN_RADIUS)
                continue;
            for (int e = 0; e < 4; ++e)
            {
                int const n = nb(c, e);
                if (n >= 0 && qdist_[static_cast<std::size_t>(n)] > d + 1)
                {
                    qdist_[static_cast<std::size_t>(n)] = d + 1;
                    queue.push_back(n);
                }
            }
        }
        return true;
    }
    return false;
}

// Is the cycle ours to walk from where our head is now? (On it, intact, and our body
// out of the way in time.) If not, or once we are within slack of its length, look for
// one that is (every QUEEN_RETRY_ROUNDS; a bigger one when it still fits but is tight,
// every QUEEN_REBUILD_ROUNDS). True when we can walk a cycle from here this turn.
bool maintain_queen_cycle()
{
    int const L = controller_.get_length();
    int const now = game_.get_round_num();
    std::size_t const hc = static_cast<std::size_t>(index(controller_.get_position()));
    int const n = static_cast<int>(qcycle_.size());
    bool const fits = n > 0 && qpos_[hc] >= 0 && queen_cycle_intact() &&
                      queen_body_fits(qcycle_, static_cast<std::size_t>(qpos_[hc]), 1);
    bool const tight = fits && n < L + Config::QUEEN_CYCLE_SLACK;
    if ((!fits && now - qcycle_tried_ >= Config::QUEEN_RETRY_ROUNDS) ||
        (tight && now - qcycle_tried_ >= Config::QUEEN_REBUILD_ROUNDS))
    {
        qcycle_tried_ = now;
        // Never a smaller cycle than one that still fits.
        if (build_queen_cycle(fits ? n + 1 : L + Config::QUEEN_CYCLE_MIN_FREE))
            return true;
    }
    return fits;
}

// Queen-safe: the usual safety, plus no threat of any kind (enemy, ally, reach) and our
// whole length proven to have a way on (or, if the search was cut short, two exits).
bool queen_safe(Candidate const &c) const
{
    return c.safe && c.critical_ok && !c.head_trade && c.destination_known && !c.unknown_portal &&
           c.hard_threats == 0 && c.soft_threats == 0 && c.enemy_threats == 0 && c.ally_threats == 0 &&
           (c.dfs_ok() || (!c.dfs_complete && c.exits >= 2));
}

// The queen's move while cycling: along the cycle, as many free steps as are queen-safe;
// otherwise the queen-safe move that ends nearest the cycle. None of those: nullopt (the
// usual choice).
std::optional<Candidate> queen_cycle_choice(std::vector<Candidate> const &candidates)
{
    bool const walk = maintain_queen_cycle();
    if (qcycle_.empty())
        return std::nullopt;
    int const L = controller_.get_length();
    int const hc = index(controller_.get_position());
    if (walk)
    {
        std::vector<Direction> path;
        int at = hc;
        for (int k = 0; k < free_moves(L); ++k)
        {
            int const nx = qnext_[static_cast<std::size_t>(at)];
            int const d = plain_dir(at, nx);
            if (d < 0)
                break;
            path.push_back(Direction::get_direction_list()[static_cast<std::size_t>(d)]);
            at = nx;
        }
        for (std::size_t k = path.size(); k >= 1; --k)
        {
            std::vector<Direction> const steps(path.begin(), path.begin() + static_cast<std::ptrdiff_t>(k));
            PostMove pm;
            if (!simulate_path(steps, pm, false))
                continue;
            Candidate c = evaluate_path(steps, pm);
            c.queen_cycle = true;
            if (queen_safe(c))
                return c;
        }
    }
    Candidate const *best = nullptr;
    for (Candidate const &c : candidates)
    {
        if (!queen_safe(c))
            continue;
        int const d = qdist_[static_cast<std::size_t>(index(c.destination))];
        int const bd = best ? qdist_[static_cast<std::size_t>(index(best->destination))] : INF;
        if (!best || d < bd || (d == bd && c.score > best->score))
            best = &c;
    }
    if (best)
        return *best;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// v23: the enemy queen. Their queen's length is the first tiebreak, so any of us who sees
// her (all but our own queen, who would die with her) kills her: our head onto hers kills
// her (and us). Dragons move one at a time, so where we see her head is where it is when
// we move. Checked first in play_turn, before anything else we might do; out of reach
// this turn, QUEEN_HUNT_PULL per step draws us towards her (goal_progress).
// ---------------------------------------------------------------------------
SeenHead const *enemy_queen_in_sight() const
{
    if (!Config::KILL_ENEMY_QUEEN || is_queen())
        return nullptr;
    for (SeenHead const &h : heads_)
        if (h.queen && h.team != controller_.get_team())
            return &h;
    return nullptr;
}

// The shortest way onto her head this turn: through tiles we can see that hold no dragon,
// at most max_turn_steps (free steps, then paid ones; we keep at least 2 segments), and
// replayed exactly (simulate_path) so it is a legal move.
std::optional<std::vector<Direction>> queen_kill_path(SeenHead const &q) const
{
    int const reach = max_turn_steps(controller_.get_length());
    int const start = index(controller_.get_position());
    int const target = index(q.position);
    int const back = direction_index(controller_.get_dir().get_opposite());
    std::size_t const cells = memory_.size();
    std::vector<int> depth(cells, -1), parent(cells, -1), via(cells, 0);
    std::vector<int> queue{start};
    depth[static_cast<std::size_t>(start)] = 0;
    for (std::size_t k = 0; k < queue.size(); ++k)
    {
        int const c = queue[k];
        int const dc = depth[static_cast<std::size_t>(c)];
        if (dc >= reach)
            continue;
        for (int d = 0; d < 4; ++d)
        {
            if (dc == 0 && d == back)
                continue;
            int const n = nb(c, d);
            if (n < 0 || depth[static_cast<std::size_t>(n)] >= 0)
                continue;
            Position const np = position_of(n);
            if (!controller_.get_tile(np))
                continue; // only through tiles we can see
            depth[static_cast<std::size_t>(n)] = dc + 1;
            parent[static_cast<std::size_t>(n)] = c;
            via[static_cast<std::size_t>(n)] = d;
            if (n == target)
            {
                std::vector<Direction> steps;
                for (int at = n; at != start; at = parent[static_cast<std::size_t>(at)])
                    steps.push_back(Direction::get_direction_list()[static_cast<std::size_t>(via[static_cast<std::size_t>(at)])]);
                std::reverse(steps.begin(), steps.end());
                PostMove pm;
                if (simulate_path(steps, pm, true))
                    return steps;
                return std::nullopt;
            }
            if (visible_part(np) || own_idx_[static_cast<std::size_t>(n)] != -1)
                continue; // a body (any dragon's, ours too) is in the way
            queue.push_back(n);
        }
    }
    return std::nullopt;
}

bool play_queen_kill()
{
    SeenHead const *q = enemy_queen_in_sight();
    if (!q)
        return false;
    std::optional<std::vector<Direction>> const steps = queen_kill_path(*q);
    if (!steps)
        return false;
    if (steps->size() == 1)
        controller_.make_move(steps->front());
    else
        controller_.make_moves(*steps);
    controller_.output_log("QUEEN_KILL", static_cast<int>(steps->size()));
    note_portals_taken(*steps);
    return true;
}

// v23: our queen's ways out are hers. She moves first each round (dragon 0 or 1), so a
// worker standing beside her head when she has at most two free sides may leave her none
// next round (Small's start: the swarm boxed her into its corner). A move whose path takes
// one of those sides costs QUEEN_EXIT_BLOCK_COST; any free side of her head, QUEEN_SIDE_COST.
int queen_exit_block(PostMove const &pm) const
{
    if (is_queen() || pm.path.empty())
        return 0;
    SeenHead const *q = nullptr;
    for (SeenHead const &h : heads_)
        if (h.queen && h.team == controller_.get_team())
            q = &h;
    if (!q)
        return 0;
    std::array<int, 4> sides{};
    int free = 0;
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(q->position, d);
        if (s.passable && s.destination_known && !visible_part(s.destination))
            sides[static_cast<std::size_t>(free++)] = index(s.destination);
    }
    for (Position const &p : pm.path)
        for (int i = 0; i < free; ++i)
            if (index(p) == sides[static_cast<std::size_t>(i)])
                return free <= 2 ? Config::QUEEN_EXIT_BLOCK_COST : Config::QUEEN_SIDE_COST;
    return 0;
}

// Out of reach this turn: QUEEN_HUNT_PULL per step closer to her head.
// v27 idea 3: out of sight, a fresh team report pulls ordinary workers QUEEN_REPORT_PULL per step.
int queen_hunt_pull(Position from, Position to) const
{
    SeenHead const *q = enemy_queen_in_sight();
    if (q)
        return (distance(from, q->position) - distance(to, q->position)) * Config::QUEEN_HUNT_PULL;
    if (!queen_share_on() || !Config::KILL_ENEMY_QUEEN || !enemy_queen_report_ || is_queen() ||
        retaining_length() || valuable_now_ ||
        game_.get_round_num() - enemy_queen_report_->round > Config::QUEEN_REPORT_TTL)
        return 0;
    if (Config::QUEEN_SHARE_ARMY && !army_worker())
        return 0;
    Position const at = enemy_queen_report_->position;
    if (distance(from, at) > (Config::QUEEN_SHARE_ARMY ? Config::QUEEN_REPORT_RADIUS_LATE : Config::QUEEN_REPORT_RADIUS))
        return 0;
    return (distance(from, at) - distance(to, at)) * Config::QUEEN_REPORT_PULL;
}

// ---------------------------------------------------------------------------
// v27 idea 3: enemy queen reports (magic 0xC), relayed while fresh
//  59 team | 58-53 x | 52-47 y | 46-38 length | 37-32 obs&63 | 4-0 checksum
// ---------------------------------------------------------------------------
std::optional<TeamSignal> enemy_queen_report_;

// v27 idea 3: sharing is on all game (V27_QUEEN_SHARE) or only from GATHER_ROUND (V27_QUEEN_SHARE_ARMY).
bool queen_share_on() const
{
    return Config::QUEEN_SHARE || (Config::QUEEN_SHARE_ARMY && game_.get_round_num() >= Config::GATHER_ROUND);
}

void note_enemy_queen_seen()
{
    if (!queen_share_on())
        return;
    for (SeenHead const &h : heads_)
        if (h.queen && h.team != controller_.get_team())
            enemy_queen_report_ = TeamSignal{h.id, h.position, h.estimated_length(), game_.get_round_num(), false};
}

bool enemy_queen_share() const
{
    return queen_share_on() && enemy_queen_report_ &&
           game_.get_round_num() - enemy_queen_report_->round <= Config::QUEEN_REPORT_TTL;
}

std::uint64_t enemy_queen_message() const
{
    std::uint64_t m = 0xCULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    m |= std::uint64_t(enemy_queen_report_->position.x & 63) << 53U;
    m |= std::uint64_t(enemy_queen_report_->position.y & 63) << 47U;
    m |= std::uint64_t(std::min(511, enemy_queen_report_->length)) << 38U;
    m |= std::uint64_t(enemy_queen_report_->round & 63) << 32U;
    m |= packet_checksum(m, 5);
    return m;
}

void receive_enemy_queen(std::uint64_t message)
{
    if (!queen_share_on() || (message & 31ULL) != packet_checksum(message & ~31ULL, 5))
        return;
    int const now = game_.get_round_num();
    int const age = (now - static_cast<int>((message >> 32U) & 63ULL)) & 63;
    Position const p(static_cast<int>((message >> 53U) & 63ULL), static_cast<int>((message >> 47U) & 63ULL));
    if (age > Config::QUEEN_REPORT_TTL || p.x >= width_ || p.y >= height_)
        return;
    if (!enemy_queen_report_ || now - age > enemy_queen_report_->round)
        enemy_queen_report_ = TeamSignal{-1, p, static_cast<int>((message >> 38U) & 511ULL), now - age, false};
}
