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
    // NEW: If a grower is demoted in the late game, force it to act as a feeder
    if (game_.get_round_num() >= 450 && !retaining_length())
        return true;

    return !retaining_length() && controller_.get_length() <= Config::FEEDER_MAX_LENGTH;
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

std::vector<Position> cycle_for_claim(Claim const &c) const
{
    std::vector<Position> cycle;
    if (c.w < 3 || c.h < 4)
        return cycle;
    // Even-height Hamiltonian cycle, reserving its west column for the return.
    for (int y = 0; y < c.h; ++y)
    {
        if (y % 2 == 0)
        {
            for (int x = (y == 0 ? 0 : 1); x < c.w; ++x)
                cycle.push_back(Position(c.x + x, c.y + y));
        }
        else
        {
            for (int x = c.w - 1; x >= 1; --x)
                cycle.push_back(Position(c.x + x, c.y + y));
        }
    }
    for (int y = c.h - 1; y >= 1; --y)
        cycle.push_back(Position(c.x, c.y + y));
    return cycle;
}

void build_patrol()
{
    patrol_ = cycle_for_claim(territory_);
    pearl_detour_.clear();
    patrol_rejoin_pending_ = false;
}

#include "patrol_growth.hpp"

void map_territory()
{
    if (!retaining_length() || controller_.get_length() < Config::CLAIM_MIN_LENGTH)
        return;
    bool displaced = false;
    for (auto const &[id, c] : claims_)
    {
        if (higher_priority(c.length, id) && territory_.w &&
            c.x < territory_.x + territory_.w && c.x + c.w > territory_.x &&
            c.y < territory_.y + territory_.h && c.y + c.h > territory_.y)
            displaced = true;
    }
    if (displaced)
    {
        territory_ = Claim{};
        patrol_.clear();
    }
    if (game_.get_round_num() % 20 != controller_.get_id() % 20 && territory_.w)
        return;
    Position here = controller_.get_position();
    Claim best = territory_;
    int best_area = best.w * best.h;
    int best_productivity = patrol_productivity(best);
    // Maximal known empty-edge rectangles containing our head. At most 64^3,
    // once per 20 rounds, and no guessed terrain or portal shortcuts.
    for (int top = std::max(0, here.y - 23); top <= here.y; ++top)
    {
        int left = std::max(0, here.x - 15), right = std::min(width_ - 1, here.x + 15);
        for (int bottom = top; bottom < std::min(height_, top + 24); ++bottom)
        {
            if (!cell(Position(here.x, bottom)).seen)
                break;
            int l = here.x, r = here.x;
            while (l > left && cell(Position(l - 1, bottom)).seen && known_clear_edge(Position(l, bottom), Direction::WEST))
                --l;
            while (r < right && cell(Position(r + 1, bottom)).seen && known_clear_edge(Position(r, bottom), Direction::EAST))
                ++r;
            left = std::max(left, l);
            right = std::min(right, r);
            if (bottom > top)
            {
                if (!known_clear_edge(Position(here.x, bottom), Direction::NORTH))
                    break;
                for (int x = here.x - 1; x >= left; --x)
                    if (!known_clear_edge(Position(x, bottom), Direction::NORTH))
                    {
                        left = x + 1;
                        break;
                    }
                for (int x = here.x + 1; x <= right; ++x)
                    if (!known_clear_edge(Position(x, bottom), Direction::NORTH))
                    {
                        right = x - 1;
                        break;
                    }
            }
            int h = bottom - top + 1, w = right - left + 1;
            int max_area = std::max(80, controller_.get_length() * 4);
            w = std::min(w, max_area / h);
            int claim_left = std::max(left, std::min(here.x - w / 2, right - w + 1));
            if (bottom < here.y || h < 4 || h % 2 || w < 3)
                continue;
            Claim c{controller_.get_id(), controller_.get_length(), game_.get_round_num(), claim_left, top, w, h};
            int const productivity = patrol_productivity(c);
            if (productivity < best_productivity ||
                (productivity == best_productivity && w * h <= best_area))
                continue;
            bool conflict = false;
            for (auto const &[id, o] : claims_)
                if (higher_priority(o.length, id) &&
                    o.x < c.x + c.w && o.x + o.w > c.x && o.y < c.y + c.h && o.y + o.h > c.y)
                    conflict = true;
            if (!conflict)
            {
                // Once a long snake has a working loop, do not remap under its body
                // unless the new cycle preserves that body's complete circular order.
                if (!patrol_.empty() && controller_.get_length() >= 16 &&
                    (c.x != territory_.x || c.y != territory_.y || c.w != territory_.w || c.h != territory_.h))
                {
                    if (!body_complete() || !ordered_on_cycle(own_trail_, cycle_for_claim(c), false) ||
                        (best_productivity > 0 && productivity * 100 <
                         patrol_productivity(territory_) * Config::PATROL_RELOCATION_PERCENT))
                        continue;
                }
                best = c;
                best_area = w * h;
                best_productivity = productivity;
            }
        }
    }
    if (best_area >= 24 && (best.x != territory_.x || best.y != territory_.y || best.w != territory_.w || best.h != territory_.h))
    {
        territory_ = best;
        build_patrol();
        controller_.output_log("CLAIM", best.x, best.y, best.w, best.h);
    }
}

std::optional<Direction> patrol_direction() const
{
    if (!Config::PATROL_ENABLED || controller_.get_length() < 16 || patrol_.size() < static_cast<std::size_t>(std::max(80, controller_.get_length() + 20)))
        return std::nullopt;
    auto it = std::find(patrol_.begin(), patrol_.end(), controller_.get_position());
    if (it == patrol_.end())
        return std::nullopt;
    Position next = patrol_[(it - patrol_.begin() + 1) % patrol_.size()];
    for (Direction d : Direction::get_direction_list())
    {
        Step s = step_from(controller_.get_position(), d);
        if (s.passable && s.destination_known && !s.portal && s.destination == next)
            return d;
    }
    return std::nullopt;
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
                // Seeing a head cannot refresh an old request or its exact length.
                if (!s.feed_request)
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
    bool const alpha_phase = game_.get_round_num() >= Config::ALPHA_FEED_START_ROUND;
    if ((worker() || controller_.get_length() <= Config::FEEDER_MAX_LENGTH || alpha_phase) &&
        game_.get_round_num() >= Config::FEED_START_ROUND)
    {
        for (auto const &s : signals_)
            if (s.id != controller_.get_id() && s.grower && s.feed_request &&
                s.length >= Config::FEED_MIN_GROWER_LENGTH && s.length > controller_.get_length() &&
                game_.get_round_num() - s.round <= Config::FEED_REQUEST_MAX_AGE &&
                (!feeder_target_ || s.length > feeder_target_->length ||
                 (s.length == feeder_target_->length && s.id < feeder_target_->id)))
                feeder_target_ = s;
        if (!enemy_target_)
            for (auto const &h : heads_)
                if (h.team != controller_.get_team() && h.estimated_length() >= Config::ENEMY_TARGET_MIN_LENGTH)
                    if (!enemy_target_ || distance(here, h.position) < distance(here, *enemy_target_))
                        enemy_target_ = h.position;
        if (!enemy_target_ && enemy_report_)
            enemy_target_ = enemy_report_->position;
        // A confirmed attack in reach takes priority over donating resources.
        if (!alpha_phase && enemy_target_ && distance(here, *enemy_target_) <= 3)
            feeder_target_.reset();
    }
    map_territory();
    update_pearl_detour();
}

bool play_feeder_strategy()
{
    if (!Config::SACRIFICE_METHOD || !feeder_target_)
        return false;
    auto const &g = *feeder_target_;
    if (game_.get_round_num() - g.round > Config::FEED_REQUEST_MAX_AGE ||
        !g.feed_request || g.length <= controller_.get_length())
        return false;
    auto head = visible_part(g.position);
    // No remote or stale-head suicide: ensure the recipient is alive and safe.
    if (!head || !head->is_head() || head->get_id() != g.id || head->get_team() != controller_.get_team())
        return false;
    Position here = controller_.get_position();
    if (distance(here, g.position) > Config::FEEDING_RADIUS)
        return false;
    for (auto const &h : heads_)
        if (h.team != controller_.get_team() &&
            (distance(here, h.position) <= 4 || distance(g.position, h.position) <= 4))
            return false;
    for (auto const &h : heads_)
        if (h.id == g.id && !h.truncated && h.lower_length <= controller_.get_length())
            return false; // recipient visibly shrank after announcing its length
    // The pearl stays on our present head tile. It must be locally reachable
    // by the recipient, not separated from its mouth by kelp or a body.
    std::deque<std::pair<Position, int>> q{{g.position, 0}};
    std::unordered_set<int> visited{index(g.position)};
    bool reachable = false;
    while (!q.empty())
    {
        auto [p, n] = q.front();
        q.pop_front();
        if (p == here)
        {
            reachable = true;
            break;
        }
        if (n >= Config::FEEDING_RADIUS)
            continue;
        for (Direction d : Direction::get_direction_list())
        {
            Step s = step_from(p, d);
            if (!s.passable || !s.destination_known || s.portal || !controller_.get_tile(s.destination))
                continue;
            auto part = visible_part(s.destination);
            if (part && !(s.destination == here))
                continue;
            if (visited.insert(index(s.destination)).second)
                q.push_back({s.destination, n + 1});
        }
    }
    if (!reachable)
        return false;
    for (Direction d : Direction::get_direction_list())
    {
        Step s = step_from(here, d);
        if (!s.passable || !s.destination_known)
            continue;
        auto part = visible_part(s.destination);
        // Never collide with any friendly head. Own-body death keeps full yield.
        if (part && !part->is_head() && part->get_id() == controller_.get_id())
        {
            controller_.output_log("FEED", g.id, "self", controller_.get_length());
            controller_.make_move(d);
            return true;
        }
    }
    return false;
}

int coordination_score(Position destination, Direction d) const
{
    int score = 0;
    if (visible_pearl(destination) && reserved_food(destination))
        score -= game_.get_round_num() < 250 ? 250 : Config::FOOD_YIELD_PENALTY;
    for (auto const &[id, c] : claims_)
        if (c.contains(destination) &&
            (!selected_grower() || higher_priority(c.length, id)))
        {
            if (!feeder_target_ || feeder_target_->id != id)
                score -= Config::TERRITORY_PENALTY;
        }
    if (feeder_target_)
    {
        int before = distance(controller_.get_position(), feeder_target_->position);
        int after = distance(destination, feeder_target_->position);
        score += (before - after) * Config::FEED_APPROACH_WEIGHT;
        if (after <= 1)
            score -= 4000; // stage beside the mouth, never occupy its next step
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
    if (retaining_length() && !feeder_target_)
    {
        if (!pearl_detour_.empty() && destination == pearl_detour_.front())
            score += Config::PATROL_DETOUR_BONUS;
        auto preferred = patrol_direction();
        if (preferred && d == *preferred)
            score += Config::PATROL_BONUS;
        if (territory_.w && territory_.contains(controller_.get_position()) && !territory_.contains(destination))
            score -= 900;
    }
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
    m |= std::uint64_t(std::min(1023, controller_.get_length())) << 19U;
    m |= std::uint64_t(controller_.get_id() & 4095) << 7U;
    // Symmetry in bits 0-1, keyed checksum in bits 2-6.
    m |= symmetry_bits();
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

bool play_cornered_escape(std::vector<Candidate> const &candidates)
{
    // Any legal-size cornered dragon can reverse by sending its rear length-2
    // segments out as a child. Long children retain the existing heir mechanism;
    // small children remain ordinary workers. No pearl or sprint cost is paid.
    if (controller_.get_length() < 2 * unswbc::Constants::MIN_SIZE || !body_complete())
        return false;
    // Stay if any move (single or escape sprint) keeps room and is not in a certain kill zone.
    int const room = std::min(6, controller_.get_length() + 1);
    if (std::any_of(candidates.begin(), candidates.end(), [room](auto const &c)
                    { return !c.head_trade && c.destination_known && c.exits > 0 && c.space >= room &&
                             c.hard_threats == 0; }))
        return false;
    int child = controller_.get_length() - 2;
    if (!controller_.can_split(child))
        return false;
    Position tail = own_trail_.back();
    if (hard_threat_[index(tail)] > 0)
        return false;
    bool escape = false;
    for (Direction d : Direction::get_direction_list())
    {
        Step s = step_from(tail, d);
        // Require locally observed destination occupancy, not a blind portal.
        if (s.passable && s.destination_known && !s.portal && controller_.get_tile(s.destination) &&
            !visible_occupied(s.destination) && enemy_threats_[index(s.destination)] == 0 &&
            ally_threats_[index(s.destination)] == 0 && hard_threat_[index(s.destination)] == 0)
            escape = true;
    }
    if (!escape)
        return false;
    controller_.output_log(retaining_length() ? "GROWER_REVERSE" : "WORKER_REVERSE", child);
    controller_.do_split(child);
    send_status(controller_.get_position(), 2);
    return true;
}
