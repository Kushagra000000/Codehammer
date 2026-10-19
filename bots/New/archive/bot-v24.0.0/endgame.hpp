// Included inside ReplayDerivedBot (after coordination.hpp).
// Late game: who the king is (relayed by sonar beacons), feeders that die next to
// the king, and split escapes (reverse out of a dead end, leaving 2 segments behind).
// v13: no Hamiltonian patrols. The king and the growers roam for pearls (see
// build_objective_map); feeders chase the king's last known position.

// ---------------------------------------------------------------------------
// King knowledge, relayed through sonar
// ---------------------------------------------------------------------------
struct KingInfo
{
    int id = -1;
    int length = 0;
    Position pos{};
    int obs = -100000;  // round the position/length were observed
    bool queen = false; // v22: the queen (wins every election from QUEEN_CROWN_ROUND)
};
KingInfo king_;
std::optional<Position> king_head_estimate_;

// A small team hears from its king rarely; forgetting it would make a feeder
// crown itself, so reports last longer then.
int king_ttl() const
{
    return small_team_mode() ? Config::SMALL_TEAM_KING_TTL : Config::KING_INFO_TTL;
}

bool king_fresh() const
{
    return king_.id >= 0 && game_.get_round_num() - king_.obs <= king_ttl();
}

// Before GATHER_ROUND only real growers (12+) count as king. From then on the
// team must always have one, however short everyone is.
int king_min_length() const
{
    return game_.get_round_num() >= Config::GATHER_ROUND ? Config::LATE_KING_MIN_LENGTH : Config::KING_MIN_LENGTH;
}

// Total order used by everyone: longer wins, ties go to the lower id.
static bool king_beats(int length_a, int id_a, int length_b, int id_b)
{
    return length_a > length_b || (length_a == length_b && id_a < id_b);
}

void offer_king(int id, int length, std::optional<Position> pos, int obs, bool queen = false)
{
    int const now = game_.get_round_num();
    if (now - obs > king_ttl())
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
        king_.queen = king_.queen || queen;
        return;
    }
    int const min_length = king_min_length();
    if (!pos || length < min_length)
        return;
    bool const self = id == my_id();
    // A split-escape heir hears its parent's last reports (length = its own birth
    // length + 2, observed before it was born). That parent is now a remnant.
    if (!self && heir_ && obs < first_round_ && length <= birth_length_ + 2)
        return;
    // A clearly newer report of a dragon nearly as long also wins for relays (a king
    // that split-escaped carries on under a new id while its old id's reports
    // circulate). We never promote ourselves that way: self-offers must win outright.
    bool better = !king_fresh() || king_.length < min_length ||
                  king_beats(length, id, king_.length, king_.id) ||
                  (!self && obs >= king_.obs + 3 && length * 10 >= king_.length * 9);
    // v22: from her crown the queen beats everyone, and nobody beats her.
    if (queen_crowned() && king_fresh() && queen != king_.queen)
        better = queen;
    if (better)
        king_ = KingInfo{id, length, *pos, obs, queen};
}

// After roles are known: expire stale knowledge, refresh from what we see, and
// nominate ourselves if we are retaining and nobody longer is known.
void update_king_info()
{
    int const now = game_.get_round_num();
    if (king_.id >= 0 && now - king_.obs > king_ttl())
        king_ = KingInfo{};
    // If we were the king but no longer qualify (e.g. we are the 2-long remnant of a
    // split escape), forget it, so we relay nothing stale and can feed the real king.
    if (king_.id == my_id() &&
        (!retaining_now_ || controller_.get_length() < king_min_length()))
        king_ = KingInfo{};
    for (SeenHead const &h : heads_)
        if (h.team == controller_.get_team() && h.id == king_.id)
        {
            king_.pos = h.position;
            king_.obs = now;
            // A body we see whole gives the exact length (e.g. the 2-long remnant
            // of a split escape); otherwise it is only a lower bound.
            king_.length = h.truncated ? std::max(king_.length, h.lower_length) : h.lower_length;
        }
    // Late game: an ally we can see is a candidate too (its visible length is a lower
    // bound), so dragons that meet elect a king without any sonar getting through.
    if (now >= Config::GATHER_ROUND)
        for (SeenHead const &h : heads_)
            if (h.team == controller_.get_team() && h.id != king_.id)
                offer_king(h.id, h.lower_length, h.position, now, h.queen && queen_crowned());
    // v22: the queen stands only from her crown (before it she walks her cycle and the
    // team's king is someone else).
    if (retaining_now_ && controller_.get_length() >= king_min_length() && (!is_queen() || queen_crowned()))
        offer_king(my_id(), controller_.get_length(), controller_.get_position(), now, is_queen());

    king_head_estimate_.reset();
    if (king_fresh() && king_.id != my_id())
        king_head_estimate_ = king_.pos;
}

// Beacon: magic 0xE, relayed by everyone so the king's identity, length and
// position reach dragons that are never in line with it.
//  57-46 id | 45-37 length | 36-31 x | 30-25 y | 24-19 obs&63 | 4-0 checksum
// (Bit 58 used to mark territory beacons; those are gone and ignored if heard.)
std::uint64_t beacon_message() const
{
    std::uint64_t m = 0xEULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    m |= std::uint64_t(king_.id & 4095) << 46U;
    m |= std::uint64_t(std::min(511, king_.length)) << 37U;
    m |= std::uint64_t(king_.pos.x & 63) << 31U;
    m |= std::uint64_t(king_.pos.y & 63) << 25U;
    m |= std::uint64_t(king_.obs & 63) << 19U;
    m |= std::uint64_t(king_.queen ? 1 : 0) << 18U; // v22
    m |= packet_checksum(m, 5);
    return m;
}

void receive_beacon(std::uint64_t message)
{
    if ((message & 31ULL) != packet_checksum(message & ~31ULL, 5))
        return;
    if (((message >> 58U) & 1ULL) != 0)
        return;
    int const now = game_.get_round_num();
    int const id = static_cast<int>((message >> 46U) & 4095ULL);
    int const length = static_cast<int>((message >> 37U) & 511ULL);
    Position const p(static_cast<int>((message >> 31U) & 63ULL), static_cast<int>((message >> 25U) & 63ULL));
    int const age = (now - static_cast<int>((message >> 19U) & 63ULL)) & 63;
    if (age > king_ttl() || p.x >= width_ || p.y >= height_ || length < 2 ||
        id == my_id())
        return;
    offer_king(id, length, p, now - age, ((message >> 18U) & 1ULL) != 0);
    remember_ally_length(id, length, now - age);
}

// ---------------------------------------------------------------------------
// Feeding
// ---------------------------------------------------------------------------
bool feeding_role() const
{
    return !retaining_length() && game_.get_round_num() >= Config::GATHER_ROUND;
}

// Where a feeder should head: the king's last known position. The king roams for
// pearls now, so the aim moves with every beacon we hear.
void update_feeding()
{
    if (!feeding_role() || !king_head_estimate_ || king_.id == my_id())
        return;
    Position const aim = *king_head_estimate_;
    feeder_target_ = TeamSignal{king_.id, aim, king_.length, game_.get_round_num(), true};
    build_feed_field(aim);
}

// Path distance from every tile to the feeder's aim (kelp and other dragons in the
// way), so feeders walk around walls instead of pressing against them.
std::vector<int> feed_dist_;
void build_feed_field(Position aim)
{
    feed_dist_.assign(memory_.size(), INF);
    std::deque<Position> queue{aim};
    feed_dist_[static_cast<std::size_t>(index(aim))] = 0;
    while (!queue.empty())
    {
        Position const p = queue.front();
        queue.pop_front();
        int const d = feed_dist_[static_cast<std::size_t>(index(p))];
        for (Direction dir : Direction::get_direction_list())
        {
            Step const st = step_from(p, dir);
            if (!st.passable || !st.destination_known)
                continue;
            std::size_t const n = static_cast<std::size_t>(index(st.destination));
            if (feed_dist_[n] <= d + 1 || other_occ_[n])
                continue;
            feed_dist_[n] = d + 1;
            queue.push_back(st.destination);
        }
    }
}

int feed_distance(Position p) const
{
    if (!feeder_target_)
        return 0;
    std::size_t const c = static_cast<std::size_t>(index(p));
    if (c < feed_dist_.size() && feed_dist_[c] < INF)
        return feed_dist_[c];
    return distance(p, feeder_target_->position) + 20; // unreachable: fall back, but prefer reachable tiles
}

// Late game with no king in sight: every such dragon walks to a point all of us
// know (the map centre), so isolated survivors meet, elect a king and feed it.
int rendezvous_score(Position destination) const
{
    int const now = game_.get_round_num();
    if (now < Config::SACRIFICE_ROUND || feeder_target_)
        return 0;
    bool const lonely = !king_fresh() || king_.id == my_id();
    if (!lonely)
        return 0;
    for (SeenHead const &h : heads_)
        if (h.team == controller_.get_team())
            return 0; // company already; let the election settle it
    Position const centre(width_ / 2, height_ / 2);
    int const weight = !retaining_length() ? Config::RENDEZVOUS_PULL
                       : small_team_mode() ? Config::SMALL_TEAM_KING_PULL
                                           : Config::RENDEZVOUS_KING_PULL;
    return (distance(controller_.get_position(), centre) - distance(destination, centre)) * weight;
}

// Movement value for feeders (v20: in thousandths of a segment).
int feeding_score(Position destination, int k = 1) const // k: steps the move takes (v21)
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
        score -= (Config::GATHER_DISTANCE - to_king) * 160;
    }
    else
    {
        int const before = feed_distance(here);
        int const after = feed_distance(destination);
        score += std::clamp(before - after, -(k + 1), k + 1) * (small_team_mode() ? 400 : 260);
    }
    if (to_king <= 1)
        score -= 1600; // never take the king's next step
    return score;
}

bool small_team_mode() const
{
    return game_.get_round_num() >= Config::SACRIFICE_ROUND &&
           controller_.get_unit_count() <= Config::SMALL_TEAM_UNITS;
}

// Is this a good place to turn into pearls for the king? Close enough that its
// pearl pull brings it here (it sees the pearls: we can see it, so it can see
// us), and only where it can actually reach us.
bool good_sacrifice_spot() const
{
    if (!feeder_target_ || !king_head_estimate_)
        return false;
    int const now = game_.get_round_num();
    int const rounds_left = unswbc::Constants::MAX_ROUNDS - now;
    Position const here = controller_.get_position();
    SeenHead const *king = seen_head(king_.id);
    if (!king)
        return false;
    int const radius = small_team_mode() ? Config::SMALL_TEAM_FEED_RADIUS : Config::FEED_NEAR_RADIUS;
    int const d = distance(here, king->position);
    if (d < 2 || d > radius || d > rounds_left - 1)
        return false;
    // Not in a corridor: the king does not walk into dead ends, pearls and all.
    if (kelp_sides(here) >= 2)
        return false;
    std::deque<std::pair<Position, int>> q{{king->position, 0}};
    std::unordered_set<int> visited{index(king->position)};
    while (!q.empty())
    {
        auto [p, n] = q.front();
        q.pop_front();
        if (p == here)
            return true;
        if (n >= radius + 2)
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

// A feeder boxed in by the crowd around the king just dies: its pearls land where
// the king is. Walled in by kelp (a dead end), or far from the king, it splits its
// way out and keeps walking: the king does not go into dead ends for pearls.
bool feeder_near_king() const
{
    return king_head_estimate_ &&
           distance(controller_.get_position(), *king_head_estimate_) <= Config::FEEDER_ESCAPE_KING_RADIUS;
}

int kelp_sides(Position p) const
{
    int n = 0;
    for (Direction d : Direction::get_direction_list())
        if (!step_from(p, d).passable)
            ++n;
    return n;
}

bool head_crowded() const
{
    Position const here = controller_.get_position();
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(here, d);
        if (s.passable && s.destination_known && other_occ_[static_cast<std::size_t>(index(s.destination))])
            return true;
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
        // Every death leaves ceil(L/2) pearls on the body's tiles; dying on our own
        // neck here (rather than in a trade) puts them where the king will eat them.
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

// Time-aware room for a would-be child of `child` segments: its head is our tail and
// its body is our rear segments reversed. The segments we keep stay put (an obstacle).
int child_space(int child, int cap, int &exits, SpaceStats *stats = nullptr) const
{
    int const L = controller_.get_length();
    int const keep = L - child;
    auto idx = [&](int c)
    {
        int const oi = own_idx_[static_cast<std::size_t>(c)];
        if (oi == -1)
            return -1;
        if (oi < keep) // the part we keep (-2: unknown own segment) stays put
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
    return timed_space_generic(tail, child, idx, cap, false, stats);
}

// After a split escape we are the remnant, usually sitting in the pocket the new
// dragon has to escape through (we move before it every round). Boxed in, we die on
// our own neck at once, leaving our pearl beside it. v23: with a free side (the
// child's first step freed the tile beside our head: see choose_escape) we live on,
// and the queen never takes this way out.
bool remnant_after_escape_ = false;
bool play_clear_remnant()
{
    if (!remnant_after_escape_)
        return false;
    remnant_after_escape_ = false;
    if (controller_.get_length() > 3 || is_queen())
        return false;
    Position const here = controller_.get_position();
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(here, d);
        if (s.passable && s.destination_known && !visible_part(s.destination))
            return false; // v23: a way on (the child's tail has left it)
    }
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(here, d);
        if (!s.passable || !s.destination_known)
            continue;
        unswbc::DragonPart const *part = visible_part(s.destination);
        if (part && !part->is_head() && part->get_id() == controller_.get_id())
        {
            controller_.output_log("REMNANT_CLEAR");
            controller_.make_move(d);
            return true;
        }
    }
    return false;
}

// v23: how much of the body to keep in a split escape. The child (our segments keep..L-1,
// reversed) moves later this round: its first step frees its tail, our segment `keep`,
// and a child that cannot move at all (boxed, and too short to split) dies on the spot
// and frees all of its tiles. When such a tile is beside our head we live on too, so
// the escape loses no unit: keep the most length that does this. Without one we keep
// 2, which die (the v13 escape).
struct EscapeChoice
{
    int keep = 2;
    bool parent_lives = false; // our head gets a free side next turn
    bool child_lives = false;  // the child has a way out
    int room = 0, exits = 0;
    SpaceStats stats;
};
EscapeChoice choose_escape() const
{
    int const L = controller_.get_length();
    Position const here = controller_.get_position();
    std::array<int, 4> beside{};
    int nb = 0;
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(here, d);
        if (!s.passable || !s.destination_known)
            continue;
        int const oi = own_idx_[static_cast<std::size_t>(index(s.destination))];
        if (oi >= 2 && oi <= L - 1)
            beside[static_cast<std::size_t>(nb++)] = oi;
    }
    // Only these keeps can let us live: a segment beside the head that the child's
    // tail frees, or a child of 2-3 that dies at once (keep L-2, L-3); and keep 2.
    std::array<int, 7> keeps{};
    int nk = 0;
    auto consider = [&](int k)
    {
        if (k < 2 || k > L - 2)
            return;
        for (int i = 0; i < nk; ++i)
            if (keeps[static_cast<std::size_t>(i)] == k)
                return;
        keeps[static_cast<std::size_t>(nk++)] = k;
    };
    for (int i = 0; i < nb; ++i)
        consider(beside[static_cast<std::size_t>(i)]);
    consider(L - 2);
    consider(L - 3);
    consider(2);
    EscapeChoice best;
    int best_saved = -1;
    for (int i = 0; i < nk; ++i)
    {
        int const keep = keeps[static_cast<std::size_t>(i)];
        int const child = L - keep;
        EscapeChoice e;
        e.keep = keep;
        e.room = child_space(child, child + 20, e.exits, &e.stats);
        e.child_lives = e.exits > 0;
        bool const child_dies = e.exits == 0 && child < 4;
        for (int j = 0; j < nb; ++j)
        {
            int const b = beside[static_cast<std::size_t>(j)];
            if (b == keep || (child_dies && b >= keep))
                e.parent_lives = true;
        }
        int const saved = (e.parent_lives ? keep : 0) + (e.child_lives ? child : 0);
        // The queen is the part we keep: she must live, then keep the most.
        bool better = best_saved < 0;
        if (!better && is_queen() && e.parent_lives != best.parent_lives)
            better = e.parent_lives;
        else if (!better)
            better = saved > best_saved || (saved == best_saved && keep > best.keep);
        if (better)
        {
            best = e;
            best_saved = saved;
        }
    }
    return best;
}

int escape_keep_ = 2; // v23: what the last split escape kept (the body handover uses it)

void do_escape_split(int child, char const *how, int room, int head_room, bool parent_lives = false)
{
    char const *tag = retaining_length()                                 ? "GROWER_REVERSE"
                      : game_.get_round_num() >= Config::SACRIFICE_ROUND ? "FEEDER_REVERSE"
                                                                         : "WORKER_REVERSE";
    controller_.output_log(tag, child, how, room, "head_room", head_room, parent_lives ? "keep_lives" : "keep_dies");
    remnant_after_escape_ = true;
    escape_keep_ = controller_.get_length() - child;
    controller_.do_split(child);
    send_status(controller_.get_position(), controller_.get_length() - child);
    send_body_handover(child); // the rays that will hit the child carry its body instead
}

// Reverse out: the rear L-2 segments become a new dragon whose head is our tail,
// and the 2 we keep (head and neck) are the sacrifice. Anyone does it rather than
// die; growers and the king also do it to get out of tight space.
bool play_split_escape(std::vector<Candidate> const &candidates)
{
    int const L = controller_.get_length();
    int const now = game_.get_round_num();
    // A zero-exit move onto a pearl is not death yet: eat it, then split next turn
    // (the tail has not moved, so the child is one segment longer).
    auto pearl_then_split = [&](Candidate const &c)
    {
        return c.steps.size() == 1 && c.exits == 0 && c.direct_pearl && !c.head_trade &&
               c.hard_threats == 0 && c.enemy_threats == 0 && c.new_length >= 4 && escape_slot_available();
    };
    // v22: the queen splits only when nothing she can do saves her: every move is a head
    // trade, or leaves her head no free side (short of a pearl to eat, then split), or was
    // searched every way on and allows at most one more move. A reach threat is not
    // certain death for her (it may not come). Then the rear L-2 are saved as a new
    // dragon, and she (the 2 left behind) dies as she would have anyway.
    auto queen_hopeless = [&](Candidate const &c)
    {
        return c.head_trade || (c.exits == 0 && !pearl_then_split(c)) ||
               (c.dfs_complete && c.dfs_depth >= 0 && c.dfs_depth <= 1);
    };
    bool const certain_death =
        candidates.empty() ||
        (is_queen() ? std::all_of(candidates.begin(), candidates.end(), queen_hopeless)
                    : std::all_of(candidates.begin(), candidates.end(), [&](Candidate const &c)
                                  { return c.head_trade || c.hard_threats > 0 || (c.exits == 0 && !pearl_then_split(c)); }));
    if (is_queen() && !certain_death)
        return false; // v22: otherwise the queen never splits
#ifdef BOT_DEBUG
    auto why = [&](char const *reason)
    {
        if (certain_death && L >= 4)
            controller_.output_log("NO_SPLIT", reason, L, controller_.get_unit_count());
        return false;
    };
#else
    auto why = [](char const *)
    { return false; };
#endif
    if (L < 4)
        return false;
    bool const retaining = retaining_length();
    bool const feeder = !retaining && now >= Config::SACRIFICE_ROUND;
    // Workers and feeders split only when the head is certainly dead.
    if (!retaining && !certain_death)
        return false;
    // A feeder dying in the crowd next to the king is food; one stuck in a dead end
    // or far away escapes.
    if (feeder && (good_sacrifice_spot() || (feeder_near_king() && head_crowded())))
        return why("feeder_near_king");
    if (!certain_death && L < Config::SPLIT_ESCAPE_MIN_LENGTH)
        return false;
    int const child = L - 2;
    if (!escape_slot_available())
        return why("unit_reserve");
    if (!controller_.can_split(child))
        return why("unit_cap");
    if (!body_complete())
    {
        // We cannot judge where the tail is, but when the head is certainly dead a
        // blind split still saves the body more often than not.
        if (!certain_death || feeder)
            return why("body_unknown");
        do_escape_split(child, "blind", -1, -1);
        return true;
    }
    // A cheap way out (a step, a free move, or a sprint paying at most 2 segments) wins.
    bool fine = false;
    int best_space = 0;
    for (Candidate const &c : candidates)
    {
        if (c.head_trade)
            continue;
        if (c.critical_ok && paid_steps(L, static_cast<int>(c.steps.size())) <= 2)
            fine = true;
        if (c.hard_threats == 0)
            best_space = std::max(best_space, c.space);
    }
    if (fine && !is_queen())
        return false; // (the queen's test already searched every way on: it overrules room estimates)
    // Tight is not doomed: the space estimate treats other dragons as walls.
    if (!certain_death && best_space >= std::max(8, L / 2))
        return false;
    // With the head certainly dead, a child in danger still beats no child.
    Position const tail = own_trail_.back();
    if (!certain_death && hard_threat_[static_cast<std::size_t>(index(tail))] > 0)
        return why("tail_threatened");
    // v23: keep the most that lives (choose_escape). A child that dies at once is a
    // price paid only when our head is otherwise dead (it is how the queen survives
    // a box, e.g. Schooltime's start).
    EscapeChoice const plan = choose_escape();
    if (plan.parent_lives && (plan.child_lives || certain_death))
    {
        do_escape_split(L - plan.keep, plan.child_lives ? "keep" : "keep_child_dies", plan.room, best_space, true);
        return true;
    }
    int exits = 0;
    SpaceStats stats;
    int const room = child_space(child, child + 20, exits, &stats);
    if (exits == 0)
        return why("tail_boxed");
    bool const child_safe = room >= child + 2 && !stats.trap(child);
    // A crowd of feeders would otherwise split again and again: feeders need real room.
    if (feeder && !child_safe)
        return why("feeder_no_room");
    if (!certain_death && !child_safe && room <= best_space + 1)
        return why("tail_no_better");
    do_escape_split(child, "room", room, best_space);
    return true;
}