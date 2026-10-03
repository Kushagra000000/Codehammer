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
    // Visible heads override stale positions; only sonar supplies exact length.
    for (auto &s : signals_)
        for (auto const &h : heads_)
            if (s.id == h.id)
            {
                s.position = h.position;
                s.round = game_.get_round_num();
                s.length = std::max(s.length, h.visible_length);
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

// Grower claims describe verified static clear terrain. Sharing these internal
// edges lets fresh split children inherit useful geometry through sonar.
void share_terrain(Claim const &c)
{
    for (int y = c.y; y < c.y + c.h; ++y)
        for (int x = c.x; x < c.x + c.w; ++x)
        {
            auto &m = cell(Position(x, y));
            for (Direction d : Direction::get_direction_list())
            {
                Position q = Position(x, y).add_dir(d);
                auto &e = m.edges[direction_index(d)];
                if (c.contains(q) && !e.known)
                    e = {true, EdgeType::EMPTY, -1};
            }
        }
}