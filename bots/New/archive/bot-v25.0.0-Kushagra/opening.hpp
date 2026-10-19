// v25 opening policy. Included inside ReplayDerivedBot.
// Status, beacons, portal sharing and escape-body handovers remain unchanged.
// A normal split can replace its coverage handover with magic 0x8, carrying the
// identical 49-bit coverage plus a two-bit centre seat and three-bit parent sector.

int centre_grants_ = 0, centre_seat_ = 0;
int centre_entered_ = -1, centre_crowded_ = 0, centre_dry_ = 0;
bool centre_released_ = false, opening_handover_seen_ = false;
bool opening_initialized_ = false;
int parent_sector_ = -1;
bool region_active_ = false;
int region_progress_round_ = 0, region_best_dist_ = INF, region_last_length_ = 0;
std::array<int, XB * XB> region_retry_after_{};

int opening_sector(Position p) const
{
    return std::min(1, p.y * 2 / height_) * 4 + std::min(3, p.x * 4 / width_);
}

void initialize_opening()
{
    if (opening_initialized_)
        return;
    opening_initialized_ = true;
    if (!centre_map() || first_round_ > 1)
        return;
    // The map format mandates alternating teams and file-order initial IDs.
    // This rule is ONLY for starting workers, never for general child IDs.
    if (is_queen())
        centre_grants_ = std::clamp(controller_.get_unit_count() - 1, 0, Config::CENTRE_SCOUTS);
    else if (!opening_handover_seen_ && controller_.get_id() >= 2 && controller_.get_id() < 6)
        centre_seat_ = controller_.get_id() / 2;
}

bool opening_regions() const
{
    return Config::DISPERSE_ENABLED && !is_queen() && !retaining_length() &&
           game_.get_round_num() < Config::OPENING_END_ROUND;
}

bool centre_scout_active() const
{
    return centre_seat_ > 0 && !centre_released_ && !is_queen() &&
           !retaining_length() && centre_rush();
}

int breeder_threshold() const
{
    return std::min(std::max(0, unit_cap() - Config::ESCAPE_UNIT_RESERVE),
                    std::clamp((width_ * height_ + Config::BREEDER_TILES_PER_UNIT - 1) /
                                   Config::BREEDER_TILES_PER_UNIT,
                               Config::BREEDER_MIN_UNITS, Config::BREEDER_MAX_UNITS));
}

bool understrength_swarm() const
{
    return game_.get_round_num() < Config::SPLIT_STOP_ROUND &&
           controller_.get_unit_count() < breeder_threshold();
}

bool candidate_allowed(Candidate const &c, bool has_safe) const
{
    bool const protect = understrength_swarm();
    bool const conservative = protect || retaining_length() || controller_.get_length() >= 4;
    return !conservative || !has_safe || c.safe || (c.head_trade && !protect);
}

std::uint64_t opening_handover_message(int seat) const
{
    std::uint64_t m = 0x8ULL << 60U;
    m |= std::uint64_t(controller_.get_team().value == unswbc::Team::B) << 59U;
    m |= (team_explored_ & ALL_BLOCKS) << 10U;
    m |= std::uint64_t(seat & 3) << 8U;
    Position const area = explore_target_.value_or(controller_.get_position());
    m |= std::uint64_t(opening_sector(area)) << 5U;
    return m | packet_checksum(m, 5);
}

void receive_opening_handover(std::uint64_t m)
{
    if ((m >> 60U) != 0x8ULL || (m & 31ULL) != packet_checksum(m & ~31ULL, 5) ||
        ((m >> 59U) & 1ULL) != std::uint64_t(controller_.get_team().value == unswbc::Team::B))
        return;
    int const seat = static_cast<int>((m >> 8U) & 3ULL);
    if (seat > Config::CENTRE_SCOUTS)
        return;
    ensure_blocks();
    team_explored_ |= (m >> 10U) & ALL_BLOCKS;
    // Never relay roles. Duplicate rays do not issue multiple assignments.
    if (!is_queen() && !opening_handover_seen_ && game_.get_round_num() == first_round_)
    {
        opening_handover_seen_ = true;
        centre_seat_ = seat;
        parent_sector_ = static_cast<int>((m >> 5U) & 7ULL);
    }
}

bool opening_ray_hits_child(Direction emitted, int keep) const
{
    if (!body_complete() || keep < 2)
        return false;
    Position p = controller_.get_position();
    Direction ray = emitted;
    if (emitted == controller_.get_dir().get_opposite())
    {
        p = own_trail_[static_cast<std::size_t>(keep - 1)];
        // A bent parent's shortened tail can point differently from its head.
        bool found = false;
        for (Direction d : Direction::get_direction_list())
        {
            Step const s = step_from(p, d);
            if (s.passable && s.destination_known && s.destination == own_trail_[static_cast<std::size_t>(keep - 2)])
            {
                ray = d.get_opposite();
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    for (int n = 0; n < width_ + height_; ++n)
    {
        Step const s = step_from(p, ray);
        if (!s.passable || !s.destination_known)
            return false;
        std::size_t const c = static_cast<std::size_t>(index(s.destination));
        int const body_index = own_idx_[c];
        if (body_index >= keep)
            return true; // Our complete body is known even beyond the vision window.
        if (body_index != -1 || other_occ_[c] || !controller_.get_tile(s.destination))
            return false;
        p = s.destination;
    }
    return false;
}

bool send_opening_handover(int child)
{
    if (!Config::EXPLORE_SHARING || game_.get_round_num() >= Config::OPENING_END_ROUND)
        return false;
    // The queen fills seats not already held by the starting workers.
    // Workers (including centre scouts) always send seat zero to their children.
    int const seat = is_queen() && centre_rush() && centre_grants_ < Config::CENTRE_SCOUTS
                         ? centre_grants_ + 1 : 0;
    std::uint64_t const m = opening_handover_message(seat);
    bool sent = false;
    for (Direction d : Direction::get_direction_list())
        if (opening_ray_hits_child(d, controller_.get_length() - child))
        {
            controller_.send_sonar(d, m);
            sent = true;
        }
    if (sent && seat)
        ++centre_grants_;
    return sent;
}

struct OpeningRegion
{
    int food = 0, unknown = 0, allies = 0, enemies = 0;
    int anchor = -1, anchor_score = -INF, distance = INF;
};

void release_region(bool cooldown)
{
    if (cooldown && explore_target_)
        region_retry_after_[static_cast<std::size_t>(block_of(*explore_target_))] =
            game_.get_round_num() + Config::REGION_RETRY_ROUNDS;
    explore_target_.reset();
    target_dist_.clear();
    region_best_dist_ = INF;
}

void update_explore_target()
{
    if (!opening_regions())
    {
        if (region_active_)
            release_region(false);
        region_active_ = false;
        update_legacy_explore_target();
        return;
    }
    region_active_ = true;
    ensure_blocks();
    int const now = game_.get_round_num();
    Position const here = controller_.get_position();
    int const L = controller_.get_length();
    // One bounded whole-map traversal, then one for the selected destination.
    // Same known-edge/portal model as legacy dispersal; tactical safety is separate.
    build_target_field(here);
    std::vector<int> const from_head = target_dist_;
    std::array<OpeningRegion, XB * XB> regions{};
    for (int c = 0; c < static_cast<int>(memory_.size()); ++c)
    {
        int const steps = from_head[static_cast<std::size_t>(c)];
        if (steps >= INF)
            continue;
        Position const p = position_of(c);
        int const b = block_of(p);
        OpeningRegion &r = regions[static_cast<std::size_t>(b)];
        CellMemory const &m = memory_[static_cast<std::size_t>(c)];
        int value = 0;
        if (!m.seen)
        {
            ++r.unknown;
            value = block_explored(b) ? 40 : 100;
        }
        else if (!other_occ_[static_cast<std::size_t>(c)] && own_idx_[static_cast<std::size_t>(c)] == -1)
        {
            int const age = now - m.last_seen;
            int food = 0;
            if (m.pearl && now - m.pearl_seen <= 12)
                food = 1000 - std::min(600, (now - m.pearl_seen) * 50);
            else if (m.pearl_time >= 0)
            {
                int const wait = m.pearl_time - age;
                int const arrival = turns_for(steps, L);
                if (wait >= 0 && wait <= arrival + Config::SPAWN_SOON_ROUNDS)
                    food = 600 / (1 + std::max(0, wait - arrival));
                else if (wait < 0 && -wait <= 6 && m.last_seen < now)
                    food = 180 / (1 - wait); // uncertain, fades rapidly; never eternal camping
            }
            r.food += food;
            value = food;
        }
        // Anchor a region on useful terrain, not an arbitrary empty block centre.
        if (value > 0 && value - steps * 20 > r.anchor_score)
        {
            r.anchor = c;
            r.anchor_score = value - steps * 20;
            r.distance = steps;
        }
    }
    for (auto const &h : heads_)
        if (h.id != my_id())
        {
            auto &r = regions[static_cast<std::size_t>(block_of(h.position))];
            ++(h.team == controller_.get_team() ? r.allies : r.enemies);
        }
    for (auto const &s : signals_)
        if (s.id != my_id() && now - s.round <= 1 && !seen_head(s.id))
            ++regions[static_cast<std::size_t>(block_of(s.position))].allies;

    // Centre access is a short opening job, not a permanent occupation order.
    if (centre_scout_active() && !centre_dist_.empty() &&
        centre_dist_[static_cast<std::size_t>(index(here))] <= centre_radius())
    {
        if (centre_entered_ < 0)
            centre_entered_ = now;
        int nearby = 0, food = 0, unknown = 0;
        for (auto const &h : heads_)
            nearby += h.id != my_id() && h.team == controller_.get_team() && distance(here, h.position) <= 4;
        for (auto const &r : regions)
            if (r.anchor >= 0 && centre_dist_[static_cast<std::size_t>(r.anchor)] <= centre_radius() + 2)
            {
                food += r.food;
                unknown += r.unknown;
            }
        centre_crowded_ = nearby >= 3 ? centre_crowded_ + 1 : 0;
        centre_dry_ = food == 0 && unknown == 0 ? centre_dry_ + 1 : 0;
        if (now - centre_entered_ >= Config::CENTRE_DWELL_ROUNDS ||
            centre_crowded_ >= Config::CENTRE_CROWD_ROUNDS || centre_dry_ >= 3)
        {
            centre_released_ = true;
            release_region(true);
        }
    }

    int current = explore_target_ ? block_of(*explore_target_) : -1;
    if (current >= 0)
    {
        int const d = from_head[static_cast<std::size_t>(index(*explore_target_))];
        if (d < region_best_dist_ || L > region_last_length_)
        {
            region_best_dist_ = d;
            region_progress_round_ = now;
        }
        OpeningRegion const &r = regions[static_cast<std::size_t>(current)];
        if (r.anchor < 0 || d >= INF || now - region_progress_round_ >= Config::REGION_STALL_ROUNDS)
        {
            release_region(true);
            current = -1;
        }
    }
    region_last_length_ = L;
    int chosen = -1, best_score = -INF;
    for (int b = 0; b < XB * XB; ++b)
    {
        OpeningRegion const &r = regions[static_cast<std::size_t>(b)];
        if (r.anchor < 0 || region_retry_after_[static_cast<std::size_t>(b)] > now)
            continue;
        Position const p = position_of(r.anchor);
        int const capacity = std::clamp(r.food / 2000, 1, 3);
        int score = std::min(r.food, 6000) / 4 + std::min(r.unknown, 16) * (block_explored(b) ? 12 : 35) -
                    r.distance * 45 - std::max(0, r.allies - capacity + 1) * 400 - r.enemies * 500;
        // Stable per-dragon preference breaks sibling ties, without replacing food.
        score += static_cast<int>(mix(std::uint64_t(controller_.get_id()) * 101 + b * 1009) % 301);
        if (now - first_round_ < Config::REGION_RETRY_ROUNDS)
        {
            if (parent_sector_ >= 0 && opening_sector(p) == parent_sector_)
                score -= 280;
            // Missing handover: the reversed child's heading points away from its parent.
            Step const ahead = step_from(here, controller_.get_dir());
            if (ahead.passable && ahead.destination_known)
                score += (distance(here, p) - distance(ahead.destination, p)) * 90;
        }
        if (centre_scout_active() && !centre_dist_.empty())
        {
            int const cd = centre_dist_[static_cast<std::size_t>(r.anchor)];
            if (cd >= INF)
                continue;
            score -= std::max(0, cd - centre_radius()) * 240;
            // The two scouts approach different portions of the central resource area.
            int const side = p.x < width_ / 2 ? 1 : 2;
            if (side == centre_seat_)
                score += 160;
        }
        if (b == current)
            score += now - target_since_ < Config::REGION_COMMIT_ROUNDS ? 500 : 250;
        if (score > best_score)
        {
            best_score = score;
            chosen = b;
        }
    }
    if (chosen < 0)
    {
        release_region(false);
        return; // Fully observed empty areas exert no attraction; ordinary routing remains.
    }
    Position const aim = position_of(regions[static_cast<std::size_t>(chosen)].anchor);
    if (chosen != current)
    {
        target_since_ = region_progress_round_ = now;
        region_best_dist_ = from_head[static_cast<std::size_t>(index(aim))];
    }
    explore_target_ = aim;
    build_target_field(aim);
}

double opening_route_weight(Position pearl, int steps) const
{
    if (!opening_regions() || !explore_target_ || steps <= 2)
        return 1.0;
    return block_of(pearl) == block_of(*explore_target_) ? 1.25 : 0.85;
}
