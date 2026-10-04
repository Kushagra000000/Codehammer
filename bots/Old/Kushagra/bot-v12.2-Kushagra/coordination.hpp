// Included inside ReplayDerivedBot. Coordination is advisory; movement safety wins.
struct Claim
{
    int id = -1, length = 0, round = 0;
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(Position p) const
    {
        return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }
};
Claim territory_;
std::map<int, Claim> claims_;
std::deque<Position> own_trail_;
std::vector<Position> patrol_;
std::optional<TeamSignal> feeder_target_;
std::optional<Position> enemy_target_;
std::optional<TeamSignal> enemy_report_;
bool founder_ = false, initialized_ = false;

bool higher_priority(int length, int id) const
{
    return length > controller_.get_length() ||
           (length == controller_.get_length() && id < controller_.get_id());
}

bool worker() const
{
    // From GATHER_ROUND every dragon that does not retain (any length, demoted
    // growers included) works for the king.
    if (retaining_length())
        return false;
    return game_.get_round_num() >= Config::GATHER_ROUND || controller_.get_length() <= Config::FEEDER_MAX_LENGTH;
}

bool reserved_food(Position p) const
{
    // Swarm building: every pearl is fair game for whoever can take it first.
    if (game_.get_round_num() < SPECIALISE_ROUND)
        return false;
    for (auto const &s : signals_)
    {
        if (!s.grower || s.id == controller_.get_id())
            continue;
        if ((!selected_grower() || higher_priority(s.length, s.id)) && distance(p, s.position) <= 4 && (game_.get_round_num() >= 250 || distance(p, s.position) <= distance(p, controller_.get_position())))
            return true;
    }
    for (auto const &[id, c] : claims_)
    {
        if (id != controller_.get_id() && c.contains(p) &&
            (!selected_grower() || higher_priority(c.length, id)))
            return true;
    }
    // v12 (experimental, KING_FOOD_RADIUS > 0): from SACRIFICE_ROUND non-kings leave
    // pearls next to the elected king's estimated head to the king, even when its
    // own status sonar has not reached us in the last two rounds.
    if (Config::KING_FOOD_RADIUS > 0 && game_.get_round_num() >= Config::SACRIFICE_ROUND && !retaining_length() &&
        king_head_estimate_ && king_fresh() && king_.id != controller_.get_id() &&
        distance(p, *king_head_estimate_) <= Config::KING_FOOD_RADIUS)
        return true;
    return false;
}

bool known_clear_edge(Position p, Direction d) const
{
    auto const &e = cell(p).edges[direction_index(d)];
    return e.known && e.type == EdgeType::EMPTY;
}

void update_coordination()
{
    // founder_/heir_ are set on the first turn and own_trail_ is maintained by
    // update_body_model (main.cpp).
    Position here = controller_.get_position();
    // Visible heads override stale positions. v12: the exact length has its own
    // timestamp. A complete sighting is exact (even if shorter: sprints and splits
    // shrink dragons); a partial one is a lower bound and never renews an old
    // sonar length beyond STATUS_LENGTH_TTL rounds.
    int const now_round = game_.get_round_num();
    for (auto &s : signals_)
        for (auto const &h : heads_)
            if (s.id == h.id)
            {
                s.position = h.position;
                s.round = now_round;
                if (!Config::V12_FRESHNESS)
                    s.length = std::max(s.length, h.visible_length);
                else if (!h.truncated)
                {
                    s.length = h.lower_length;
                    s.length_round = now_round;
                }
                else if (now_round - s.length_round > Config::STATUS_LENGTH_TTL)
                    s.length = h.lower_length;
                else
                    s.length = std::max(s.length, h.lower_length);
            }
    feeder_target_.reset();
    enemy_target_.reset();
    if (enemy_report_ && game_.get_round_num() - enemy_report_->round > 2)
        enemy_report_.reset();
    for (auto const &h : heads_)
        if (h.team != controller_.get_team() && h.estimated_length() >= Config::ENEMY_TARGET_MIN_LENGTH)
            if (!enemy_report_ || enemy_report_->round < game_.get_round_num() || h.estimated_length() > enemy_report_->length)
                enemy_report_ = TeamSignal{h.id, h.position, h.estimated_length(), game_.get_round_num(), false};
    // v11: from AGGRESSION_START_ROUND, hunt visible enemies worth more than we are,
    // and long reported ones nearby. Nothing before that round.
    if (!valuable_now_ && game_.get_round_num() >= Config::AGGRESSION_START_ROUND)
    {
        for (auto const &h : heads_)
            if (eligible_target(h) && profitable_target(h))
                if (!enemy_target_ || distance(here, h.position) < distance(here, *enemy_target_))
                    enemy_target_ = h.position;
        if (AGGR_MODE != 1 && !enemy_target_ && enemy_report_ && enemy_report_->length >= Config::ENDGAME_HUNT_MIN_LENGTH &&
            (AGGR_MODE == 2 || controller_.get_unit_count() >= Config::HUNT_MIN_UNITS) &&
            distance(here, enemy_report_->position) <= Config::REPORT_HUNT_RADIUS &&
            !retaining_length() && controller_.get_unit_count() >= Config::MIN_UNITS_FOR_TRADE &&
            enemy_report_->length >= controller_.get_length())
            enemy_target_ = enemy_report_->position;
    }
    if (worker() && game_.get_round_num() >= Config::GATHER_ROUND)
    {
        update_feeding();
        if (!enemy_target_)
            for (auto const &h : heads_)
                if (h.team != controller_.get_team() && h.estimated_length() >= Config::ENEMY_TARGET_MIN_LENGTH)
                    if (!enemy_target_ || distance(here, h.position) < distance(here, *enemy_target_))
                        enemy_target_ = h.position;
        if (!enemy_target_ && enemy_report_)
            enemy_target_ = enemy_report_->position;
        // A confirmed attack in reach takes priority over donating resources.
        if (enemy_target_ && distance(here, *enemy_target_) <= 3)
            feeder_target_.reset();
    }
    map_territory();
    orient_patrol();
}

int coordination_score(Position destination, Direction d) const
{
    int score = 0;
    if (visible_pearl(destination) && reserved_food(destination))
        score -= feeder_target_ ? Config::FEEDER_FOOD_PENALTY : Config::FOOD_YIELD_PENALTY;
    for (auto const &[id, c] : claims_)
        if (c.contains(destination) &&
            (!selected_grower() || higher_priority(c.length, id)))
        {
            if (!feeder_target_ || feeder_target_->id != id)
                score -= Config::TERRITORY_PENALTY;
        }
    if (feeder_target_)
    {
        score += feeding_score(destination);
    }
    else if (enemy_target_ && !valuable_now_)
    {
        // NEW: prioritize killing enemies only if they're near; otherwise prioritize pearls.
        int dist_to_enemy = distance(controller_.get_position(), *enemy_target_);
        if (dist_to_enemy <= Config::HUNT_RADIUS)
        {
            // Enemy is close! Swarm them.
            int const weight = game_.get_round_num() >= Config::FEED_START_ROUND ? Config::LATE_HUNT_WEIGHT : Config::HUNT_WEIGHT;
            score += (dist_to_enemy - distance(destination, *enemy_target_)) * weight;
        }
        else
        {
            // Enemy is too far. Apply a very weak pull so we don't abandon local farming.
            score += (dist_to_enemy - distance(destination, *enemy_target_)) * 50;
        }
    }
    (void)d;
    score += patrol_score(destination);
    score += rendezvous_score(destination);
    return score;
}

std::uint64_t claim_message() const
{
    std::uint64_t m = 0xCULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    m |= std::uint64_t(game_.get_round_num() & 63) << 53U;
    m |= std::uint64_t(territory_.h & 63) << 47U;
    m |= std::uint64_t(territory_.w & 63) << 41U;
    m |= std::uint64_t(territory_.y & 63) << 35U;
    m |= std::uint64_t(territory_.x & 63) << 29U;
    m |= std::uint64_t(std::min(511, controller_.get_length())) << 19U;
    m |= std::uint64_t((territory_variant_ >> 1) & 1) << 28U;
    m |= std::uint64_t(controller_.get_id() & 4095) << 7U;
    // Bit 0: we patrol our cycle backwards; bit 1 and bit 28: cycle layout.
    // Keyed checksum in bits 2-6.
    m |= std::uint64_t(patrol_dir_ < 0 ? 1 : 0);
    m |= std::uint64_t(territory_variant_ & 1) << 1U;
    m |= packet_checksum(m, 5) << 2U;
    return m;
}

// Grower claims certify only the edges their announced cycle layout uses (the
// territory validator checks exactly those). v12: v10.3.3 marked EVERY internal
// rectangle edge open, inventing interior chords that may be kelp. Only unknown
// edges are filled; a direct observation is never overwritten by inference.
void share_terrain(Claim const &c, int variant)
{
    CycleMap cm;
    make_cycle(c, variant, cm);
    std::size_t const n = cm.cells.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        Position const p = cm.cells[i], q = cm.cells[(i + 1) % n];
        for (Direction d : Direction::get_direction_list())
        {
            if (!(p.add_dir(d) == q))
                continue;
            auto &e = cell(p).edges[static_cast<std::size_t>(direction_index(d))];
            if (!e.known)
                e = {true, EdgeType::EMPTY, -1};
            auto &back = cell(q).edges[static_cast<std::size_t>(direction_index(d.get_opposite()))];
            if (!back.known)
                back = {true, EdgeType::EMPTY, -1};
            break;
        }
    }
}