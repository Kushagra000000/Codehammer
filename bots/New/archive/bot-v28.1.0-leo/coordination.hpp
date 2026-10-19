// Included inside ReplayDerivedBot. Coordination is advisory; movement safety wins.
std::deque<Position> own_trail_;
std::optional<TeamSignal> feeder_target_;
std::optional<Position> enemy_target_;
std::optional<TeamSignal> enemy_report_;
bool founder_ = false, initialized_ = false;

bool higher_priority(int length, int id) const
{
    return length > controller_.get_length() ||
           (length == controller_.get_length() && id < my_id());
}

bool worker() const
{
    // From GATHER_ROUND every dragon that does not retain (any length, demoted
    // growers included) works for the king.
    if (retaining_length())
        return false;
    return game_.get_round_num() >= Config::GATHER_ROUND || controller_.get_length() <= Config::FEEDER_MAX_LENGTH;
}

// Pearls right next to a grower are its to take: a worker leaves them to any grower,
// a retaining dragon only to a longer one (v13: was "any grower" for a king that was
// not hash-selected).
bool reserved_food(Position p) const
{
    // Swarm building: every pearl is fair game for whoever can take it first.
    if (game_.get_round_num() < SPECIALISE_ROUND)
        return false;
    for (auto const &s : signals_)
    {
        if (!s.grower || s.id == my_id())
            continue;
        if ((!retaining_length() || higher_priority(s.length, s.id)) && distance(p, s.position) <= 4)
            return true;
    }
    return false;
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
        if (!enemy_target_ && enemy_report_ && enemy_report_->length >= Config::ENDGAME_HUNT_MIN_LENGTH &&
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
}
// (v20: what coordination_score added to a move is now scored by goal: see scoring.hpp.)
