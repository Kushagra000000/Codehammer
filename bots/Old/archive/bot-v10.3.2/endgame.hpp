// Included inside ReplayDerivedBot (after coordination.hpp).
// Late game: who the king is (relayed by sonar beacons), Hamiltonian patrols,
// feeders that die where the king will pass, and split escapes for long dragons.

// ---------------------------------------------------------------------------
// Hamiltonian cycles over a rectangle
// ---------------------------------------------------------------------------
struct CycleMap
{
    Claim rect{};
    int variant = 0;
    std::vector<Position> cells;
    std::vector<int> idx; // cell index -> position in `cells`, or -1
};

// Boustrophedon cycle over a rectangle. variant bit 0: sweep along columns instead
// of rows; bit 1: mirror, so the return lane is on the other side. The number of
// sweeps must be even.
void make_cycle(Claim const &r, int variant, CycleMap &out) const
{
    out.rect = r;
    out.variant = variant;
    out.cells.clear();
    out.idx.assign(memory_.size(), -1);
    bool const transpose = (variant & 1) != 0, flip = (variant & 2) != 0;
    int const a = transpose ? r.h : r.w; // length of a sweep
    int const b = transpose ? r.w : r.h; // number of sweeps
    if (a < 2 || b < 2 || b % 2 != 0)
        return;
    auto at = [&](int u, int v)
    {
        int const uu = flip ? a - 1 - u : u;
        return transpose ? Position(r.x + v, r.y + uu) : Position(r.x + uu, r.y + v);
    };
    for (int v = 0; v < b; ++v)
    {
        if (v % 2 == 0)
            for (int u = (v == 0 ? 0 : 1); u < a; ++u)
                out.cells.push_back(at(u, v));
        else
            for (int u = a - 1; u >= 1; --u)
                out.cells.push_back(at(u, v));
    }
    for (int v = b - 1; v >= 1; --v)
        out.cells.push_back(at(0, v));
    for (std::size_t i = 0; i < out.cells.size(); ++i)
        out.idx[static_cast<std::size_t>(index(out.cells[i]))] = static_cast<int>(i);
}

static bool same_rect(Claim const &a, Claim const &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// Steps from index i to index j travelling in direction dir (+1 / -1).
static int cycle_ahead(int i, int j, int dir, int size)
{
    return (((j - i) * dir) % size + size) % size;
}

// ---------------------------------------------------------------------------
// King knowledge, relayed through sonar
// ---------------------------------------------------------------------------
struct KingInfo
{
    int id = -1;
    int length = 0;
    Position pos{};
    int obs = -100000; // round the position/length were observed
};
struct KingTerritory
{
    int id = -1;
    Claim rect{};
    int variant = 0;
    bool reverse = false;
    int obs = -100000;
};
KingInfo king_;
KingTerritory king_rect_;
std::map<int, KingTerritory> known_territories_; // every grower territory heard, by owner
CycleMap patrol_cycle_;                          // our own patrol
int patrol_dir_ = 1;
mutable CycleMap king_cycle_; // the king's patrol, as feeders reconstruct it
std::optional<Position> king_head_estimate_;
int king_cycle_index_ = -1; // predicted index of the king's head on its cycle

bool king_fresh() const
{
    return king_.id >= 0 && game_.get_round_num() - king_.obs <= Config::KING_INFO_TTL;
}

void offer_king(int id, int length, std::optional<Position> pos, int obs)
{
    int const now = game_.get_round_num();
    if (now - obs > Config::KING_INFO_TTL)
        return;
    if (id == king_.id)
    {
        // Same dragon: take any newer report, even a shrunken one (after a split
        // escape the old id is a length-2 remnant).
        if (obs >= king_.obs)
        {
            king_.length = length;
            if (pos)
                king_.pos = *pos;
            king_.obs = obs;
        }
        return;
    }
    if (!pos || length < Config::KING_MIN_LENGTH)
        return;
    // A clearly newer report of a dragon nearly as long wins too: a king that
    // split-escaped keeps ~all its length under a new id, while reports of its old
    // id keep circulating until they expire.
    bool const better = !king_fresh() || king_.length < Config::KING_MIN_LENGTH || length > king_.length ||
                        (length == king_.length && id < king_.id) ||
                        (obs >= king_.obs + 3 && length * 10 >= king_.length * 9);
    if (better)
        king_ = KingInfo{id, length, *pos, obs};
}

// Kept for any owner: whoever turns out to be king, we may already know its cycle.
void offer_territory(int id, Claim const &rect, int variant, bool reverse, int obs)
{
    if (game_.get_round_num() - obs > Config::KING_INFO_TTL)
        return;
    auto found = known_territories_.find(id);
    if (found != known_territories_.end() && obs < found->second.obs)
        return;
    known_territories_[id] = KingTerritory{id, rect, variant, reverse, obs};
}

// After roles are known: expire stale knowledge, refresh from what we see, and
// nominate ourselves if we are retaining and nobody longer is known.
void update_king_info()
{
    int const now = game_.get_round_num();
    if (king_.id >= 0 && now - king_.obs > Config::KING_INFO_TTL)
        king_ = KingInfo{};
    std::erase_if(known_territories_, [&](auto const &item)
                  { return now - item.second.obs > Config::KING_INFO_TTL; });
    king_rect_ = KingTerritory{};
    if (auto found = known_territories_.find(king_.id); king_.id >= 0 && found != known_territories_.end())
        king_rect_ = found->second;
    for (SeenHead const &h : heads_)
        if (h.team == controller_.get_team() && h.id == king_.id)
        {
            king_.pos = h.position;
            king_.obs = now;
            king_.length = std::max(king_.length, h.lower_length);
        }
    if (retaining_now_ && controller_.get_length() >= Config::KING_MIN_LENGTH)
        offer_king(controller_.get_id(), controller_.get_length(), controller_.get_position(), now);
    if (retaining_now_ && patrol_ready())
        offer_territory(controller_.get_id(), territory_, territory_variant_, patrol_dir_ < 0, now);
    if (king_.id == controller_.get_id() && patrol_ready())
        king_rect_ = KingTerritory{controller_.get_id(), territory_, territory_variant_, patrol_dir_ < 0, now};

    // Where the king's head is now, and where it sits on its cycle.
    king_head_estimate_.reset();
    king_cycle_index_ = -1;
    if (!king_fresh() || king_.id == controller_.get_id())
        return;
    king_head_estimate_ = king_.pos;
    bool const visible = seen_head(king_.id) != nullptr;
    if (king_rect_.id == king_.id)
    {
        if (!same_rect(king_cycle_.rect, king_rect_.rect) || king_cycle_.variant != king_rect_.variant ||
            king_cycle_.cells.empty())
            make_cycle(king_rect_.rect, king_rect_.variant, king_cycle_);
        if (!king_cycle_.cells.empty())
        {
            int const i = king_cycle_.idx[static_cast<std::size_t>(index(king_.pos))];
            if (i >= 0)
            {
                int const size = static_cast<int>(king_cycle_.cells.size());
                int const dir = king_rect_.reverse ? -1 : 1;
                int const age = visible ? 0 : std::min(now - king_.obs, 6);
                king_cycle_index_ = ((i + dir * age) % size + size) % size;
                king_head_estimate_ = king_cycle_.cells[static_cast<std::size_t>(king_cycle_index_)];
            }
        }
    }
}

// Beacon: magic 0xE, relayed by everyone so the king's identity, length, position
// and territory reach dragons that are never in line with it.
//  subtype 0: 57-46 id | 45-37 length | 36-31 x | 30-25 y | 24-19 obs&63 | 4-0 checksum
//  subtype 1: 57-46 id | 45-40 x | 39-34 y | 33-28 w | 27-22 h | 21-16 obs&63 | 15 reverse | 4-0 checksum
std::uint64_t beacon_message(bool territory) const
{
    std::uint64_t m = 0xEULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    if (!territory)
    {
        m |= std::uint64_t(king_.id & 4095) << 46U;
        m |= std::uint64_t(std::min(511, king_.length)) << 37U;
        m |= std::uint64_t(king_.pos.x & 63) << 31U;
        m |= std::uint64_t(king_.pos.y & 63) << 25U;
        m |= std::uint64_t(king_.obs & 63) << 19U;
    }
    else
    {
        m |= 1ULL << 58U;
        m |= std::uint64_t(king_rect_.id & 4095) << 46U;
        m |= std::uint64_t(king_rect_.rect.x & 63) << 40U;
        m |= std::uint64_t(king_rect_.rect.y & 63) << 34U;
        m |= std::uint64_t(king_rect_.rect.w & 63) << 28U;
        m |= std::uint64_t(king_rect_.rect.h & 63) << 22U;
        m |= std::uint64_t(king_rect_.obs & 63) << 16U;
        m |= std::uint64_t(king_rect_.reverse ? 1 : 0) << 15U;
        m |= std::uint64_t(king_rect_.variant & 3) << 13U;
    }
    m |= packet_checksum(m, 5);
    return m;
}

void receive_beacon(std::uint64_t message)
{
    if ((message & 31ULL) != packet_checksum(message & ~31ULL, 5))
        return;
    int const now = game_.get_round_num();
    int const id = static_cast<int>((message >> 46U) & 4095ULL);
    if (((message >> 58U) & 1ULL) == 0)
    {
        int const length = static_cast<int>((message >> 37U) & 511ULL);
        Position const p(static_cast<int>((message >> 31U) & 63ULL), static_cast<int>((message >> 25U) & 63ULL));
        int const age = (now - static_cast<int>((message >> 19U) & 63ULL)) & 63;
        if (age > Config::KING_INFO_TTL || p.x >= width_ || p.y >= height_ || length < 2 ||
            id == controller_.get_id())
            return;
        offer_king(id, length, p, now - age);
        remember_ally_length(id, length, now - age);
    }
    else
    {
        Claim c;
        c.id = id;
        c.x = static_cast<int>((message >> 40U) & 63ULL);
        c.y = static_cast<int>((message >> 34U) & 63ULL);
        c.w = static_cast<int>((message >> 28U) & 63ULL);
        c.h = static_cast<int>((message >> 22U) & 63ULL);
        int const age = (now - static_cast<int>((message >> 16U) & 63ULL)) & 63;
        bool const reverse = ((message >> 15U) & 1ULL) != 0;
        int const variant = static_cast<int>((message >> 13U) & 3ULL);
        if (age > Config::KING_INFO_TTL || c.w < 2 || c.h < 2 || c.x + c.w > width_ || c.y + c.h > height_)
            return;
        c.round = now - age;
#ifdef BOT_DEBUG
        controller_.output_log("BCN1", id == king_.id ? "match" : "nomatch", age);
#endif
        offer_territory(id, c, variant, reverse, now - age);
        maybe_adopt_territory(c, variant, reverse);
    }
}

// ---------------------------------------------------------------------------
// Patrol (our own Hamiltonian cycle)
// ---------------------------------------------------------------------------
bool patrol_ready() const
{
    return Config::PATROL_ENABLED && territory_.w && !patrol_cycle_.cells.empty() &&
           controller_.get_length() >= Config::PATROL_MIN_LENGTH &&
           static_cast<int>(patrol_cycle_.cells.size()) >= controller_.get_length() + Config::PATROL_SLACK;
}

int territory_variant_ = 0;

void rebuild_patrol()
{
    make_cycle(territory_, territory_variant_, patrol_cycle_);
    patrol_.assign(patrol_cycle_.cells.begin(), patrol_cycle_.cells.end());
}

// Travel the cycle the way our body already lies (a grower escape heir lies along it reversed).
void orient_patrol()
{
    if (patrol_cycle_.cells.empty() || own_trail_.size() < 2)
        return;
    int const size = static_cast<int>(patrol_cycle_.cells.size());
    int const i = patrol_cycle_.idx[static_cast<std::size_t>(index(controller_.get_position()))];
    int const n = patrol_cycle_.idx[static_cast<std::size_t>(index(own_trail_[1]))];
    if (i < 0 || n < 0)
        return;
    if (cycle_ahead(i, n, 1, size) == size - 1)
        patrol_dir_ = 1;
    else if (cycle_ahead(i, n, 1, size) == 1)
        patrol_dir_ = -1;
}

// A grower-escape heir wakes up inside its parent's territory with no map; adopt it.
void maybe_adopt_territory(Claim const &c, int variant, bool reverse)
{
    if (!heir_ || territory_.w || !c.contains(controller_.get_position()))
        return;
    territory_ = c;
    territory_variant_ = variant;
    territory_.id = controller_.get_id();
    territory_.length = controller_.get_length();
    territory_.round = game_.get_round_num();
    share_terrain(c);
    rebuild_patrol();
    patrol_dir_ = reverse ? 1 : -1; // we lie along the parent's cycle backwards
    orient_patrol();
    controller_.output_log("ADOPT", c.x, c.y, c.w, c.h);
}

// Score for moving to `destination` while patrolling. Moving to the next cycle cell
// is rewarded. Any other tile inside the territory is a jump forward along the
// cycle: fine while it stays clear of the tail, fatal-in-waiting otherwise.
int patrol_score(Position destination) const
{
    if (!retaining_length())
        return 0;
    Position const here = controller_.get_position();
    if (!patrol_ready())
    {
        if (territory_.w && territory_.contains(here) && !territory_.contains(destination))
            return -900;
        return 0;
    }
    int const size = static_cast<int>(patrol_cycle_.cells.size());
    int const i = patrol_cycle_.idx[static_cast<std::size_t>(index(here))];
    int const j = patrol_cycle_.idx[static_cast<std::size_t>(index(destination))];
    if (i < 0)
    {
        // Outside: head back in.
        auto gap = [&](Position p)
        {
            int dx = std::max({0, territory_.x - p.x, p.x - (territory_.x + territory_.w - 1)});
            int dy = std::max({0, territory_.y - p.y, p.y - (territory_.y + territory_.h - 1)});
            return dx + dy;
        };
        return (gap(here) - gap(destination)) * Config::PATROL_RETURN_PULL;
    }
    if (j < 0)
        return -Config::PATROL_LEAVE_PENALTY;
    int const length_after = controller_.get_length() + (visible_pearl(destination) ? 1 : 0);
    int const ahead = cycle_ahead(i, j, patrol_dir_, size);
    if (ahead == 1)
        return Config::PATROL_BONUS;
    if (ahead >= 2 && ahead <= size - length_after - 2)
        return visible_pearl(destination) ? 0 : -Config::PATROL_JUMP_PENALTY;
    return -Config::PATROL_BACKJUMP_PENALTY;
}

// ---------------------------------------------------------------------------
// Feeding
// ---------------------------------------------------------------------------
bool feeding_role() const
{
    return !retaining_length() && game_.get_round_num() >= Config::GATHER_ROUND;
}

// Where a feeder should head: a point ahead of the king on its cycle, or the king.
void update_feeding()
{
    if (!feeding_role() || !king_head_estimate_ || king_.id == controller_.get_id())
        return;
    Position aim = *king_head_estimate_;
    if (king_cycle_index_ >= 0)
    {
        int const size = static_cast<int>(king_cycle_.cells.size());
        int const dir = king_rect_.reverse ? -1 : 1;
        int const k = ((king_cycle_index_ + dir * Config::FEED_AIM_AHEAD) % size + size) % size;
        aim = king_cycle_.cells[static_cast<std::size_t>(k)];
    }
    feeder_target_ = TeamSignal{king_.id, aim, king_.length, game_.get_round_num(), true};
}

// Movement score for feeders (replaces the plain pull toward the grower).
int feeding_score(Position destination) const
{
    if (!feeder_target_ || !king_head_estimate_)
        return 0;
    int score = 0;
    Position const here = controller_.get_position();
    Position const king = *king_head_estimate_;
    int const to_king = distance(destination, king);
    if (game_.get_round_num() < Config::SACRIFICE_ROUND && to_king < Config::GATHER_DISTANCE)
    {
        // Gather nearby, but do not crowd the king before sacrifices start.
        score -= (Config::GATHER_DISTANCE - to_king) * 400;
    }
    else
    {
        int const before = distance(here, feeder_target_->position);
        int const after = distance(destination, feeder_target_->position);
        score += (before - after) * 650;
    }
    if (to_king <= 1)
        score -= 4000; // never take the king's next step
    if (king_cycle_index_ >= 0)
    {
        // Keep off the next few cells of its cycle while alive.
        int const size = static_cast<int>(king_cycle_.cells.size());
        int const j = king_cycle_.idx[static_cast<std::size_t>(index(destination))];
        if (j >= 0)
        {
            int const ahead = cycle_ahead(king_cycle_index_, j, king_rect_.reverse ? -1 : 1, size);
            if (ahead >= 1 && ahead <= 2)
                score -= 3000;
        }
    }
    return score;
}

// Is this a good place to turn into pearls for the king?
bool good_sacrifice_spot() const
{
    if (!feeder_target_ || !king_head_estimate_)
        return false;
    int const now = game_.get_round_num();
    int const rounds_left = unswbc::Constants::MAX_ROUNDS - now;
    Position const here = controller_.get_position();
    if (king_cycle_index_ >= 0)
    {
        // The king patrols: die inside its territory, a little ahead of it, so it
        // eats us without leaving its cycle.
        int const j = king_cycle_.idx[static_cast<std::size_t>(index(here))];
        if (j < 0)
            return false;
        int const size = static_cast<int>(king_cycle_.cells.size());
        int const ahead = cycle_ahead(king_cycle_index_, j, king_rect_.reverse ? -1 : 1, size);
        int const limit = std::min({size - king_.length - 3, Config::FEED_AHEAD_MAX, rounds_left - 1});
        return ahead >= Config::FEED_AHEAD_MIN && ahead <= limit;
    }
    // No known patrol: die close enough that the king's pearl pull brings it here,
    // and only where it can actually reach us.
    SeenHead const *king = seen_head(king_.id);
    if (!king)
        return false;
    int const d = distance(here, king->position);
    if (d < 2 || d > Config::FEED_NEAR_RADIUS || d > rounds_left - 1)
        return false;
    std::deque<std::pair<Position, int>> q{{king->position, 0}};
    std::unordered_set<int> visited{index(king->position)};
    while (!q.empty())
    {
        auto [p, n] = q.front();
        q.pop_front();
        if (p == here)
            return true;
        if (n >= Config::FEED_NEAR_RADIUS + 2)
            continue;
        for (Direction dir : Direction::get_direction_list())
        {
            Step s = step_from(p, dir);
            if (!s.passable || !s.destination_known || s.portal || !controller_.get_tile(s.destination))
                continue;
            int const c = index(s.destination);
            // Our own body turns into pearls and gaps, so it does not block.
            if (other_occ_[static_cast<std::size_t>(c)] && !(s.destination == here))
                continue;
            if (visited.insert(c).second)
                q.push_back({s.destination, n + 1});
        }
    }
    return false;
}

bool play_feeder_strategy()
{
    if (!Config::SACRIFICE_METHOD || !feeder_target_ || !feeding_role() ||
        game_.get_round_num() < Config::SACRIFICE_ROUND)
        return false;
    Position const here = controller_.get_position();
    for (auto const &h : heads_)
        if (h.team != controller_.get_team() && distance(here, h.position) <= 3)
            return false; // they would eat it, and we may have a trade instead
    if (!good_sacrifice_spot())
        return false;
    for (Direction d : Direction::get_direction_list())
    {
        Step s = step_from(here, d);
        if (!s.passable || !s.destination_known)
            continue;
        auto part = visible_part(s.destination);
        // Own-body death keeps the full ceil(L/2) yield.
        if (part && !part->is_head() && part->get_id() == controller_.get_id())
        {
            controller_.output_log("FEED", king_.id, "len", controller_.get_length());
            controller_.make_move(d);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Split escape: hand the body to a new dragon that starts at our tail
// ---------------------------------------------------------------------------

// Time-aware room for the would-be child: head at our tail, body = our segments
// 2..L-1 reversed. Our first two segments stay behind as an obstacle.
int child_space(int cap, int &exits) const
{
    int const L = controller_.get_length();
    auto idx = [&](int c)
    {
        int const oi = own_idx_[static_cast<std::size_t>(c)];
        if (oi == -1)
            return -1;
        if (oi < 2) // our head and neck (-2: unknown own segment) stay put
            return -3;
        return L - 1 - oi;
    };
    Position const tail = own_trail_.back();
    exits = 0;
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(tail, d);
        if (!s.passable)
            continue;
        if (!s.destination_known)
        {
            ++exits;
            continue;
        }
        int const n = index(s.destination);
        if (other_occ_[static_cast<std::size_t>(n)] || idx(n) != -1)
            continue;
        ++exits;
    }
    return timed_space_generic(tail, L - 2, idx, cap, false);
}

bool play_split_escape(std::vector<Candidate> const &candidates)
{
    int const L = controller_.get_length();
    bool const certain_death = candidates.empty() ||
                               std::all_of(candidates.begin(), candidates.end(), [](Candidate const &c)
                                           { return c.head_trade || c.hard_threats > 0 || c.exits == 0; });
    if (!body_complete() || L < 4 || (!certain_death && L < Config::SPLIT_ESCAPE_MIN_LENGTH))
        return false;
    // Only growers split to get out of tight space; anyone splits to avoid certain death.
    if (!certain_death && !retaining_length())
        return false;
    int const child = L - 2;
    if (!controller_.can_split(child))
        return false;
    // A cheap way out (a step, or a sprint of at most 3 steps) wins.
    bool fine = false;
    int best_space = 0;
    for (Candidate const &c : candidates)
    {
        if (c.head_trade)
            continue;
        if (c.critical_ok && c.steps.size() <= 3)
            fine = true;
        if (c.hard_threats == 0)
            best_space = std::max(best_space, c.space);
    }
    if (fine)
        return false;
    // Tight is not doomed: the space estimate treats other dragons as walls.
    if (!certain_death && best_space >= std::max(8, L / 2))
        return false;
    Position const tail = own_trail_.back();
    if (hard_threat_[static_cast<std::size_t>(index(tail))] > 0)
        return false;
    int exits = 0;
    int const room = child_space(child + 20, exits);
    if (exits == 0)
        return false;
    bool const child_safe = room >= child + 2;
    if (!child_safe && room <= best_space + 1)
        return false;
    controller_.output_log("GROWER_REVERSE", child, "room", room, "head_room", best_space);
    controller_.do_split(child);
    send_status(controller_.get_position(), L - child);
    return true;
}

// ---------------------------------------------------------------------------
// Territory search: the largest rectangle around our head (all tiles seen, no
// higher-priority claim) that has a Hamiltonian cycle whose own edges are all
// known clear. Kelp on edges the cycle does not use is fine.
// ---------------------------------------------------------------------------
bool edge_clear(Position p, Direction d) const
{
    auto const &e = cell(p).edges[static_cast<std::size_t>(direction_index(d))];
    return e.known && e.type == EdgeType::EMPTY;
}

void map_territory()
{
    int const L = controller_.get_length();
    if (!retaining_length() || L < Config::CLAIM_MIN_LENGTH)
        return;
    bool displaced = false;
    for (auto const &[id, c] : claims_)
        if (higher_priority(c.length, id) && territory_.w &&
            c.x < territory_.x + territory_.w && c.x + c.w > territory_.x &&
            c.y < territory_.y + territory_.h && c.y + c.h > territory_.y)
            displaced = true;
    if (displaced)
    {
        territory_ = Claim{};
        patrol_.clear();
        patrol_cycle_.cells.clear();
    }
    int const now = game_.get_round_num();
    bool const outgrown = territory_.w &&
                          static_cast<int>(patrol_cycle_.cells.size()) < L + Config::PATROL_REGROW_SLACK;
    // Keep a working cycle: switching cycles mid-flight is the risky part.
    if (territory_.w && !outgrown)
        return;
    if (territory_.w && now % 3 != controller_.get_id() % 3)
        return;
    Position const here = controller_.get_position();
    if (territory_.w && !territory_.contains(here))
        return; // regrow only from inside, so the new cycle overlaps where we lie

    // Prefix sums: blocked east edges per row, blocked south edges per column, unseen tiles.
    int const W = width_, H = height_;
    std::vector<int> pe(static_cast<std::size_t>((W + 1) * H), 0), ps(static_cast<std::size_t>(W * (H + 1)), 0),
        unseen(static_cast<std::size_t>((W + 1) * (H + 1)), 0);
    auto PE = [&](int x, int y) -> int &
    { return pe[static_cast<std::size_t>(y * (W + 1) + x)]; };
    auto PS = [&](int x, int y) -> int &
    { return ps[static_cast<std::size_t>(x * (H + 1) + y)]; };
    auto U = [&](int x, int y) -> int &
    { return unseen[static_cast<std::size_t>(y * (W + 1) + x)]; };
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            PE(x + 1, y) = PE(x, y) + (edge_clear(Position(x, y), Direction::EAST) ? 0 : 1);
            PS(x, y + 1) = PS(x, y) + (edge_clear(Position(x, y), Direction::SOUTH) ? 0 : 1);
            U(x + 1, y + 1) = U(x + 1, y) + U(x, y + 1) - U(x, y) + (cell(Position(x, y)).seen ? 0 : 1);
        }
    auto run_h = [&](int y, int xa, int xb)
    { return xb <= xa || PE(xb, y) - PE(xa, y) == 0; }; // edges xa..xb-1
    auto run_v = [&](int x, int ya, int yb)
    { return yb <= ya || PS(x, yb) - PS(x, ya) == 0; };
    auto link_h = [&](int x, int y)
    { return PE(x + 1, y) - PE(x, y) == 0; };
    auto link_v = [&](int x, int y)
    { return PS(x, y + 1) - PS(x, y) == 0; };

    // Does layout `variant` fit rectangle (x0, y0, w, h)?
    auto valid = [&](int x0, int y0, int w, int h, int variant)
    {
        bool const transpose = (variant & 1) != 0, flip = (variant & 2) != 0;
        int const a = transpose ? h : w, b = transpose ? w : h;
        if (a < 2 || b < 2 || b % 2 != 0)
            return false;
        // Position of sweep coordinate u along the sweep axis.
        int const origin = transpose ? y0 : x0;
        auto pos = [&](int u)
        { return origin + (flip ? a - 1 - u : u); };
        auto sweep_run = [&](int v, int ua, int ub) // links between u in [ua, ub]
        {
            int const p1 = std::min(pos(ua), pos(ub)), p2 = std::max(pos(ua), pos(ub));
            return transpose ? run_v(x0 + v, p1, p2) : run_h(y0 + v, p1, p2);
        };
        auto turn = [&](int v, int u) // link between sweep v and v+1 at u
        {
            return transpose ? link_h(x0 + v, pos(u)) : link_v(pos(u), y0 + v);
        };
        if (!sweep_run(0, 0, a - 1))
            return false;
        for (int v = 1; v < b; ++v)
            if (!sweep_run(v, 1, a - 1))
                return false;
        for (int v = 0; v + 1 < b; ++v)
            if (!turn(v, v % 2 == 0 ? a - 1 : 1))
                return false;
        // Return lane at u = 0 across all sweeps.
        return transpose ? run_h(pos(0), x0, x0 + b - 1) : run_v(pos(0), y0, y0 + b - 1);
    };

    int const max_area = std::max(80, L * 4);
    int const min_area = std::max(24, L + Config::PATROL_REGROW_SLACK);
    Claim best = territory_;
    int best_variant = territory_variant_;
    int best_area = outgrown ? static_cast<int>(patrol_cycle_.cells.size()) : 0;
    for (int h = std::min(H, 24); h >= 4; --h)
        for (int w = std::min(W, 24); w >= 3; --w)
        {
            int const area = w * h;
            if (area > max_area || area < min_area || area <= best_area)
                continue;
            for (int y0 = std::max(0, here.y - h + 1); y0 <= here.y && y0 + h <= H; ++y0)
                for (int x0 = std::max(0, here.x - w + 1); x0 <= here.x && x0 + w <= W; ++x0)
                {
                    if (area <= best_area)
                        break;
                    if (U(x0 + w, y0 + h) - U(x0, y0 + h) - U(x0 + w, y0) + U(x0, y0) != 0)
                        continue;
                    bool conflict = false;
                    for (auto const &[id, o] : claims_)
                        if (higher_priority(o.length, id) &&
                            o.x < x0 + w && o.x + o.w > x0 && o.y < y0 + h && o.y + o.h > y0)
                            conflict = true;
                    if (conflict)
                        continue;
                    for (int variant = 0; variant < 4; ++variant)
                        if (valid(x0, y0, w, h, variant))
                        {
                            best = Claim{controller_.get_id(), L, now, x0, y0, w, h};
                            best_variant = variant;
                            best_area = area;
                            break;
                        }
                }
        }
    if (best.w && (best.x != territory_.x || best.y != territory_.y || best.w != territory_.w ||
                   best.h != territory_.h || best_variant != territory_variant_))
    {
        territory_ = best;
        territory_variant_ = best_variant;
        rebuild_patrol();
        orient_patrol();
        controller_.output_log("CLAIM", best.x, best.y, best.w, best.h, "v", best_variant);
    }
}