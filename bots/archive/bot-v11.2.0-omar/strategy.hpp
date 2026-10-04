// Included inside ReplayDerivedBot (after route.hpp).
// v18: what only AGGR mode 2 (elimination first) does. Every entry point returns
// early in mode 3, so mode 3 plays exactly as before.
//
//  - Trades (trade_advantage): a head-on trade kills both dragons, so it is taken
//    only with a clear advantage: the target is clearly longer than us, or it is an
//    even trade where we outnumber them around it, or it is their endgame dragon.
//  - Guard (guard_threat): a long worker keeps its head out of reach (a step, or a
//    sprint of up to 3) of shorter enemy heads, which would win the trade.
//  - Blocking (block_score): a move that shrinks a visible enemy head's room scores
//    per tile taken; one that leaves it too little room to live scores like a kill,
//    without our dragon dying for it.
//  - Hunting (update_hunting, hunt_score): long enemies are reported by sonar (magic
//    0xC, relayed); short dragons converge on them, each to its own side of the head.
//    An enemy body in sight with its head out of sight is followed towards its head
//    (every segment faces the next one): that is how the long ones are found late.

static constexpr bool MODE2 = AGGR_MODE == 2;

// ---------------------------------------------------------------------------
// Trades
// ---------------------------------------------------------------------------
// How long an enemy is, for deciding a trade: what we can see of it, plus one if its
// body runs out of sight (it is at least that long; the usual estimate adds two).
static int likely_length(SeenHead const &h)
{
    return h.lower_length + (h.truncated ? 1 : 0);
}

bool trade_advantage(SeenHead const &h) const
{
    int const L = controller_.get_length();
    int const est = likely_length(h);
    if (est >= L + Config::TRADE_MASS_MARGIN)
        return true; // they lose more segments than we do
    if (est < L)
        return false;
    // Even trade: only where we outnumber them. We then win the rest of the fight
    // and eat the pearls both bodies leave.
    int allies = 1, enemies = 0;
    std::unordered_set<int> counted;
    for (SeenHead const &o : heads_)
    {
        if (chebyshev_distance(o.position, h.position) > Config::LOCAL_EDGE_RADIUS)
            continue;
        if (o.team == controller_.get_team())
        {
            ++allies;
            counted.insert(o.id);
        }
        else
            ++enemies;
    }
    // Allies out of our sight that told us where they are (status sonar, this round
    // or the last).
    int const now = game_.get_round_num();
    for (TeamSignal const &a : signals_)
        if (now - a.round <= 1 && a.id != controller_.get_id() && !counted.count(a.id) &&
            chebyshev_distance(a.position, h.position) <= Config::LOCAL_EDGE_RADIUS)
            ++allies;
    return allies >= enemies + Config::LOCAL_EDGE;
}

// ---------------------------------------------------------------------------
// Guard
// ---------------------------------------------------------------------------
// How exposed our head would be at `destination` to enemy heads shorter than us (a
// trade that pays for them): 2 for each that can step onto it, 1 for each that can
// sprint there (up to 3 steps through free tiles; a sprint costs it a segment per
// extra step). 0 for dragons the guard does not cover.
int guard_threat(Position destination) const
{
    if constexpr (!MODE2)
        return 0;
    int const L = controller_.get_length();
    if (L < Config::GUARD_MIN_LENGTH || valuable_now_) // valuable dragons have the full threat model
        return 0;
    int const target = index(destination);
    int threat = 0;
    for (SeenHead const &h : heads_)
    {
        if (h.team == controller_.get_team() || h.lower_length >= L ||
            chebyshev_distance(h.position, destination) > 3)
            continue;
        // Its body may run out of sight: then assume it can afford a 2-step sprint.
        int const len = h.truncated ? std::max(h.lower_length + 1, 3) : h.lower_length;
        int const reach = std::clamp(len - 1, 1, 3);
        std::vector<int> frontier{index(h.position)}, seen{index(h.position)};
        int found = 0;
        for (int depth = 1; depth <= reach && !found && !frontier.empty(); ++depth)
        {
            std::vector<int> next;
            for (int c : frontier)
                for (Direction d : Direction::get_direction_list())
                {
                    if (depth == 1 && d == h.facing.get_opposite())
                        continue;
                    Step const st = step_from(position_of(c), d);
                    if (!st.passable || !st.destination_known)
                        continue;
                    int const n = index(st.destination);
                    if (n == target)
                    {
                        found = depth;
                        break;
                    }
                    if (std::find(seen.begin(), seen.end(), n) != seen.end() || visible_occupied(st.destination))
                        continue;
                    seen.push_back(n);
                    next.push_back(n);
                }
            frontier = std::move(next);
        }
        if (found)
            threat += found == 1 ? 2 : 1;
    }
    return threat;
}

// ---------------------------------------------------------------------------
// Blocking
// ---------------------------------------------------------------------------
struct BlockTarget
{
    int id = -1;
    Position head{};
    Direction facing = Direction::NORTH;
    int length = 2; // lower bound: its segments free up no later than this says
    int est = 2;
    int need = 2; // room below this: it cannot live
    int cap = 5;
    int base = 0;                     // its room if our move took nothing from it
    std::unordered_map<int, int> own; // its visible segments: cell -> index
};
std::vector<BlockTarget> block_targets_;
mutable std::vector<int> blk_stamp_;
mutable int blk_gen_ = 0;

bool blocking_active() const
{
    return MODE2 && !retaining_length() && game_.get_round_num() >= Config::AGGRESSION_START_ROUND;
}

// Tiles the enemy head can still reach, move by move, before it runs out of room.
// It moves once after each of our moves. Its own segment i frees for its move k when
// k >= length - i + 1; ours (after `pm`, or where we are now) likewise; any other
// body is a wall; unseen tiles are free.
int enemy_room(BlockTarget const &t, PostMove const *pm) const
{
    if (blk_stamp_.size() != memory_.size())
        blk_stamp_.assign(memory_.size(), 0);
    ++blk_gen_;
    int const gen = blk_gen_;
    int const now = game_.get_round_num();
    int const L = controller_.get_length();
    std::deque<std::pair<int, int>> queue; // (cell, move number of arrival)
    int const start = index(t.head);
    blk_stamp_[static_cast<std::size_t>(start)] = gen;
    queue.push_back({start, 0});
    int room = 0;
    while (!queue.empty() && room < t.cap)
    {
        auto const [c, k0] = queue.front();
        queue.pop_front();
        int const k = k0 + 1;
        Position const p = position_of(c);
        for (Direction d : Direction::get_direction_list())
        {
            if (k0 == 0 && d == t.facing.get_opposite())
                continue;
            Step const s = step_from(p, d);
            if (!s.passable || !s.destination_known)
                continue;
            int const n = index(s.destination);
            std::size_t const ni = static_cast<std::size_t>(n);
            if (blk_stamp_[ni] == gen)
                continue;
            // Our body.
            // (With no move given: as if we moved without adding a head tile, so the
            // baseline ages the same way as every real move does.)
            int j = -1;
            if (pm)
                j = post_index(*pm, n);
            else if (own_idx_[ni] != -1)
                j = (own_idx_[ni] == -2 ? known_body_ : own_idx_[ni]) + 1;
            int const our_len = pm ? pm->length : L;
            if (j >= 0 && k < our_len - j + 1)
                continue; // not free yet: try again from a later path
            // Its own body, and everyone else's.
            if (auto const it = t.own.find(n); it != t.own.end())
            {
                if (k < t.length - it->second + 1)
                    continue;
            }
            else if (other_occ_[ni])
                continue;
            else
            {
                CellMemory const &m = memory_[ni];
                if (m.last_seen != now && m.seen && m.other_seen && now - m.last_seen <= Config::OCCUPIED_MEMORY)
                    continue;
            }
            blk_stamp_[ni] = gen;
            ++room;
            queue.push_back({n, k});
        }
    }
    return room;
}

// Once per turn: the enemy heads in sight and how much room each has now.
void prepare_blocking()
{
    block_targets_.clear();
    if (!blocking_active())
        return;
    for (SeenHead const &h : heads_)
    {
        if (h.team == controller_.get_team())
            continue;
        BlockTarget t;
        t.id = h.id;
        t.head = h.position;
        t.facing = h.facing;
        t.length = std::max(2, h.lower_length);
        t.est = h.estimated_length();
        t.need = t.length;
        t.cap = t.length + 3;
        BodyTrace const trace = trace_body(h.position, h.id);
        for (std::size_t i = 0; i < trace.segments.size(); ++i)
            t.own[index(trace.segments[i])] = static_cast<int>(i);
        t.base = enemy_room(t, nullptr);
        block_targets_.push_back(std::move(t));
    }
}

// Score for a move that leaves us as `pm`: tiles taken from enemy heads, and a kill
// for leaving one without room to live. `kill` reports whether it does that.
int block_score(PostMove const &pm, bool exposed, bool &kill) const
{
    kill = false;
    if (block_targets_.empty())
        return 0;
    int score = 0;
    for (BlockTarget const &t : block_targets_)
    {
        int const after = enemy_room(t, &pm);
        int const base = std::min(t.base, t.cap);
        if (after < base)
            score += (base - after) * Config::BLOCK_PRESSURE_PER_CELL;
        if (after < t.need && base >= t.need)
        {
            // With our head in its reach, a doomed enemy can take us with it instead:
            // then it only counts if that trade would pay for us too.
            if (exposed)
            {
                SeenHead const *h = seen_head(t.id);
                if (!h || !trade_advantage(*h))
                    continue;
            }
            score += Config::BLOCK_KILL_BASE + Config::BLOCK_KILL_PER_LENGTH * std::min(t.est, 20);
            kill = true;
        }
    }
    return score;
}

// ---------------------------------------------------------------------------
// Hunting
// ---------------------------------------------------------------------------
// A report is either a head we (or a teammate) saw, or a trail: an enemy body we saw
// whose head was out of sight. Every body segment faces the next one towards the head,
// so we follow the chain to where it leaves our window; the head lies further along.
// Late in the game the enemy's long dragons (the ones building length) have bodies
// far longer than our window, so a trail is how we usually find them.
struct HuntReport
{
    Position pos{};                      // the head, or for a trail the first segment out of sight
    Direction facing = Direction::NORTH; // the head's heading, or the way the trail leads
    bool facing_known = true;            // false: from a status packet, which has no facing
    int length = 0;
    int round = 0;
    int hops = 0;
    int id = -1; // the enemy dragon, when known
    bool trail = false;
};
std::vector<HuntReport> hunt_reports_;
std::optional<HuntReport> hunt_target_;
std::optional<Position> hunt_aim_;
std::vector<int> hunt_dist_;
std::map<int, std::pair<int, int>> enemy_length_; // id -> (length, round seen)

void note_enemy_length(int id, int length, int round)
{
    if (id < 0)
        return;
    auto it = enemy_length_.find(id);
    if (it == enemy_length_.end() || it->second.second < round || it->second.first < length)
        enemy_length_[id] = {length, round};
}

int known_enemy_length(int id) const
{
    auto const it = enemy_length_.find(id);
    if (it == enemy_length_.end() || game_.get_round_num() - it->second.second > Config::ENEMY_LENGTH_MEMORY)
        return 0;
    return it->second.first;
}

void add_hunt_report(HuntReport const &r)
{
    note_enemy_length(r.id, r.length, r.round);
    // One report per dragon: the same id, or (with no id) one near an older report.
    // Newer wins; at the same age a head beats a trail, a known facing beats none,
    // and fewer relays beat more.
    auto better = [](HuntReport const &a, HuntReport const &b)
    {
        if (a.round != b.round)
            return a.round > b.round;
        if (a.trail != b.trail)
            return !a.trail;
        if (a.facing_known != b.facing_known)
            return a.facing_known;
        return a.hops < b.hops;
    };
    for (HuntReport &old : hunt_reports_)
    {
        bool const same = (r.id >= 0 && old.id >= 0) ? r.id == old.id : distance(old.pos, r.pos) <= 3;
        if (same)
        {
            if (better(r, old))
            {
                int const len = std::max(r.length, old.length);
                old = r;
                old.length = len;
            }
            return;
        }
    }
    hunt_reports_.push_back(r);
}

// Enemy bodies in sight whose head is not: follow each chain towards its head to where
// it leaves our window.
void observe_trails()
{
    int const now = game_.get_round_num();
    std::unordered_set<int> heads_seen, done;
    for (SeenHead const &h : heads_)
        heads_seen.insert(h.id);
    std::unordered_map<int, int> visible;
    for (unswbc::Tile const &tile : controller_.get_tiles())
        if (unswbc::DragonPart const *part = tile.get_dragon())
            ++visible[part->get_id()];
    for (unswbc::Tile const &tile : controller_.get_tiles())
    {
        unswbc::DragonPart const *part = tile.get_dragon();
        if (!part || part->is_head() || part->get_team() == controller_.get_team() ||
            heads_seen.count(part->get_id()) || done.count(part->get_id()))
            continue;
        int const id = part->get_id();
        done.insert(id);
        Position at = part->get_position();
        Direction dir = part->get_dir();
        bool frontier = false;
        for (int guard = 0; guard < 64; ++guard)
        {
            Step const s = step_from(at, dir);
            if (!s.passable || !s.destination_known)
                break; // through a portal we cannot follow
            if (!controller_.get_tile(s.destination))
            {
                frontier = true; // the chain leaves our window here
                break;
            }
            unswbc::DragonPart const *next = visible_part(s.destination);
            if (!next || next->get_id() != id || next->is_head())
                break;
            at = s.destination;
            dir = next->get_dir();
        }
        if (!frontier)
            continue;
        Step const out = step_from(at, dir);
        // Its head is out of sight, so it is at least one longer than what we see.
        int const length = std::max(known_enemy_length(id), visible[id] + 2);
        bool const late = now >= SPECIALISE_ROUND;
        if (length < Config::HUNT_TARGET_MIN_LENGTH && !(late && visible[id] >= Config::LATE_TRAIL_MIN_SEEN))
            continue;
        HuntReport r;
        r.pos = out.destination;
        r.facing = dir;
        r.length = std::max(length, late ? Config::HUNT_TARGET_MIN_LENGTH : 0);
        r.round = now;
        r.id = id;
        r.trail = true;
        add_hunt_report(r);
    }
}

bool hunter() const
{
    if constexpr (!MODE2)
        return false;
    int const now = game_.get_round_num();
    int const max_length = now >= SPECIALISE_ROUND ? Config::LATE_HUNTER_MAX_LENGTH : Config::HUNTER_MAX_LENGTH;
    if (now < Config::AGGRESSION_START_ROUND || retaining_length() || valuable_now_ ||
        controller_.get_length() > max_length || controller_.get_unit_count() < Config::MIN_UNITS_FOR_TRADE)
        return false;
    if (now >= SPECIALISE_ROUND)
        return true;
    return mix(static_cast<std::uint64_t>(controller_.get_id()) ^ 0x5DEECE66DULL) % Config::HUNT_SHARE_DENOMINATOR == 0;
}

// Where this hunter goes: ahead of the head, straight or to one side (by id), so
// several hunters close in on it from different sides; the head itself once close.
// For a trail, the head is further along the body: we aim a little past where the
// chain left our window, and see further along it every turn.
Position hunt_aim(HuntReport const &r) const
{
    int const age = game_.get_round_num() - r.round;
    Position head = r.pos;
    if (r.facing_known)
    {
        int const ahead = age + (r.trail ? Config::TRAIL_LEAD : 0);
        for (int i = 0; i < ahead; ++i)
            head = head.add_dir(r.facing); // it has probably kept going
    }
    if (!r.facing_known || r.trail || distance(controller_.get_position(), head) <= 4)
        return head;
    Direction const f = r.facing;
    int const slot = controller_.get_id() % 3;
    Position aim = head;
    int const ahead = slot == 0 ? 3 : 2;
    for (int i = 0; i < ahead; ++i)
        aim = aim.add_dir(f);
    if (slot != 0)
    {
        Direction const side = slot == 1 ? f.get_left() : f.get_right();
        aim = aim.add_dir(side).add_dir(side);
    }
    return aim;
}

void build_hunt_field(Position aim)
{
    hunt_dist_.assign(memory_.size(), INF);
    std::deque<Position> queue{aim};
    hunt_dist_[static_cast<std::size_t>(index(aim))] = 0;
    while (!queue.empty())
    {
        Position const p = queue.front();
        queue.pop_front();
        int const d = hunt_dist_[static_cast<std::size_t>(index(p))];
        for (Direction dir : Direction::get_direction_list())
        {
            Step const st = step_from(p, dir);
            if (!st.passable || !st.destination_known)
                continue;
            std::size_t const n = static_cast<std::size_t>(index(st.destination));
            if (hunt_dist_[n] <= d + 1 || other_occ_[n])
                continue;
            hunt_dist_[n] = d + 1;
            queue.push_back(st.destination);
        }
    }
}

// Once per turn, after vision and sonar: refresh reports, pick a target, aim.
void update_hunting()
{
    hunt_aim_.reset();
    if constexpr (!MODE2)
        return;
    int const now = game_.get_round_num();
    std::erase_if(hunt_reports_, [&](HuntReport const &r)
                  { return now - r.round > Config::HUNT_REPORT_TTL; });
    for (SeenHead const &h : heads_)
        if (h.team != controller_.get_team())
        {
            note_enemy_length(h.id, h.estimated_length(), now);
            if (h.estimated_length() >= Config::HUNT_TARGET_MIN_LENGTH)
                add_hunt_report(HuntReport{h.position, h.facing, true, h.estimated_length(), now, 0, h.id, false});
        }
    observe_trails();
    if (!hunter())
    {
        hunt_target_.reset();
        return;
    }
    Position const here = controller_.get_position();
    int const L = controller_.get_length();
    // A dragon on its way to feed the king only turns to hunt what is close.
    int const radius = feeder_target_ ? Config::FEEDER_HUNT_RADIUS : Config::HUNT_REPORT_RADIUS;
    int const length_cap = now >= SPECIALISE_ROUND ? Config::LATE_HUNT_LENGTH_CAP : 30;
    HuntReport const *best = nullptr;
    int best_value = 0;
    for (HuntReport const &r : hunt_reports_)
    {
        int const d = distance(here, r.pos);
        if (d > radius || r.length < L + Config::TRADE_MASS_MARGIN)
            continue;
        int value = std::min(r.length, length_cap) * 100 / (d + 4);
        if (hunt_target_ && ((r.id >= 0 && r.id == hunt_target_->id) || distance(hunt_target_->pos, r.pos) <= 4))
            value = value * 13 / 10; // stay on the one we were chasing
        if (value > best_value)
        {
            best_value = value;
            best = &r;
        }
    }
    if (!best)
    {
        hunt_target_.reset();
        return;
    }
    hunt_target_ = *best;
    hunt_aim_ = hunt_aim(*best);
    build_hunt_field(*hunt_aim_);
#ifdef BOT_DEBUG
    controller_.output_log("HUNT", best->length, distance(here, best->pos), best->hops, best->trail ? "trail" : "head");
#endif
}

int hunt_score(Position destination) const
{
    if (!hunt_aim_ || hunt_dist_.empty())
        return 0;
    Position const here = controller_.get_position();
    int before = hunt_dist_[static_cast<std::size_t>(index(here))];
    int after = hunt_dist_[static_cast<std::size_t>(index(destination))];
    if (before >= INF || after >= INF)
    {
        before = distance(here, *hunt_aim_);
        after = distance(destination, *hunt_aim_);
    }
    int const pull = game_.get_round_num() >= SPECIALISE_ROUND ? Config::LATE_HUNT_PULL : Config::HUNT_PULL;
    return std::clamp(before - after, -1, 1) * pull;
}

// 63-60 magic 0xC | 59 team | 58-53 x | 52-47 y | 46-45 facing | 44-39 length (max 63)
// 38-36 round&7 | 35-34 hops | 33-22 enemy id | 21 trail | 20-5 zero | 4-0 checksum
std::uint64_t hunt_message(HuntReport const &r) const
{
    std::uint64_t m = 0xCULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    m |= std::uint64_t(r.pos.x & 63) << 53U;
    m |= std::uint64_t(r.pos.y & 63) << 47U;
    m |= std::uint64_t(direction_index(r.facing) & 3) << 45U;
    m |= std::uint64_t(std::min(63, r.length)) << 39U;
    m |= std::uint64_t(r.round & 7) << 36U;
    m |= std::uint64_t(std::min(3, r.hops + 1)) << 34U;
    m |= std::uint64_t(r.id >= 0 ? r.id & 4095 : 4095) << 22U;
    m |= std::uint64_t(r.trail ? 1 : 0) << 21U;
    m |= packet_checksum(m, 5);
    return m;
}

void receive_hunt(std::uint64_t m)
{
    if constexpr (!MODE2)
        return;
    if ((m & 31ULL) != packet_checksum(m & ~31ULL, 5) || ((m >> 5U) & 0xFFFFULL) != 0)
        return;
    int const now = game_.get_round_num();
    HuntReport r;
    r.pos = Position(static_cast<int>((m >> 53U) & 63ULL), static_cast<int>((m >> 47U) & 63ULL));
    r.facing = Direction::get_direction_list()[static_cast<std::size_t>((m >> 45U) & 3ULL)];
    r.length = static_cast<int>((m >> 39U) & 63ULL);
    int const age = (now - static_cast<int>((m >> 36U) & 7ULL)) & 7;
    r.hops = static_cast<int>((m >> 34U) & 3ULL);
    int const id = static_cast<int>((m >> 22U) & 4095ULL);
    r.id = id == 4095 ? -1 : id;
    r.trail = ((m >> 21U) & 1ULL) != 0;
    if (age > Config::HUNT_REPORT_TTL || r.pos.x >= width_ || r.pos.y >= height_ ||
        r.length < Config::HUNT_TARGET_MIN_LENGTH)
        return;
    r.round = now - age;
    add_hunt_report(r);
}

// An enemy sighting from a teammate's status packet: position and a length class.
void hunt_report_from_status(Position pos, int length, int round)
{
    if constexpr (!MODE2)
        return;
    if (length < Config::HUNT_TARGET_MIN_LENGTH)
        return;
    HuntReport r;
    r.pos = pos;
    r.facing_known = false;
    r.length = length;
    r.round = round;
    r.hops = Config::HUNT_RELAY_HOPS; // relayed by the status packets themselves
    add_hunt_report(r);
}

// The report worth passing on this round (fresh, not relayed too often), if any:
// heads before trails, then the longest.
std::optional<std::uint64_t> hunt_broadcast() const
{
    if constexpr (!MODE2)
        return std::nullopt;
    int const now = game_.get_round_num();
    if (now < Config::AGGRESSION_START_ROUND)
        return std::nullopt;
    HuntReport const *best = nullptr;
    for (HuntReport const &r : hunt_reports_)
        if (now - r.round <= 1 && r.facing_known && r.hops < Config::HUNT_RELAY_HOPS &&
            (!best || (best->trail && !r.trail) || (best->trail == r.trail && r.length > best->length)))
            best = &r;
    if (!best)
        return std::nullopt;
    return hunt_message(*best);
}