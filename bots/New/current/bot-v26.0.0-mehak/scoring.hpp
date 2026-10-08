// Included inside ReplayDerivedBot (after route.hpp).
// v20 scoring. How a move is chosen, in three parts:
//
//  1. One currency. Every value is in thousandths of a segment (SEG = 1000): eating
//     a pearl is +1000. Our dragon dying costs its length plus what a unit is worth
//     (death_cost); killing an enemy gains its length plus what one of their units is
//     worth (kill_value); a risk is its probability times what it would cost. Terms
//     with no physical meaning (spacing, visit counts) keep their old weight relative
//     to a pearl.
//  2. One goal per turn (choose_goal), picked by a fixed priority list:
//       FIGHT   an enemy head within FIGHT_RADIUS steps
//       FEED    late game: walk to the king (feeder_target_)
//       HUNT    a profitable enemy target within HUNT_RADIUS (enemy_target_)
//       EAT     a pearl route is planned (route.hpp)
//       EXPLORE anything else: timers, unexplored tiles, the dispersal destination
//     Only the goal's own pull scores progress (goal_progress). In a fight, the goal
//     we would otherwise have still pulls at half strength to break ties.
//  3. Lookahead in fights (fight_value): each move is played out against every reply
//     of each nearby enemy head and our best answer to it. Outcomes: a head-on (both
//     die), the enemy left with no room to live (a kill), us left with no way out.
//     Its replies include a sprint of 2-3 steps into our head when it is long enough
//     (v21: or as far as its free steps go).
//     The enemy is assumed to pick the reply worst for us (mostly; see
//     FIGHT_WORST_WEIGHT).

enum class Goal
{
    FIGHT,
    FEED,
    HUNT,
    EAT,
    EXPLORE
};
Goal goal_ = Goal::EXPLORE;
Goal base_goal_ = Goal::EXPLORE; // what we would do with no enemy close

static constexpr int SEG = 1000;
static int seg(double segments) { return static_cast<int>(std::lround(segments * SEG)); }
static int risk(double probability, int cost) { return static_cast<int>(std::lround(probability * cost)); }

static char const *goal_name(Goal g)
{
    switch (g)
    {
    case Goal::FIGHT:
        return "FIGHT";
    case Goal::FEED:
        return "FEED";
    case Goal::HUNT:
        return "HUNT";
    case Goal::EAT:
        return "EAT";
    default:
        return "EXPLORE";
    }
}

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------
// What losing this dragon costs: its segments (the king's and growers' count more:
// they decide the tiebreak) and the unit.
int death_cost() const
{
    double length = controller_.get_length() * (retaining_length() ? Config::KEEPER_LENGTH_WEIGHT : 1.0);
    if (is_queen()) // v23: losing her loses the first tiebreak, whatever her length
        length = std::max(length, static_cast<double>(Config::QUEEN_DEATH_LENGTH));
    return seg(length + Config::UNIT_VALUE_OURS);
}

// A trapped dragon of 4+ escapes by splitting: it loses the 2-long remnant (a unit).
// v21: only if it may split out then; at the unit cap (most of the midgame on big
// maps) a worker cannot, and a trap costs the whole dragon.
int trap_cost() const
{
    int const L = controller_.get_length();
    // v22: for the queen a trap is her death (an escape split saves only the rest of her).
    bool const can_escape = !is_queen() && L >= 4 && escape_slot_available() && controller_.can_split(L - 2);
    return can_escape ? seg(2.0 + Config::UNIT_VALUE_OURS) : death_cost();
}

// What killing this enemy gains: its segments and the unit; late in the game their
// long dragons are their tiebreak, worth more.
int kill_value(SeenHead const &h) const
{
    int const est = h.estimated_length();
    double v = est + Config::UNIT_VALUE_THEIRS;
    if (game_.get_round_num() >= Config::ENDGAME_HUNT_ROUND && est >= Config::ENDGAME_HUNT_MIN_LENGTH)
        v += Config::ENDGAME_KILL_BONUS;
    return seg(v);
}

// A head-on: both die.
int trade_value(SeenHead const &h) const
{
    return kill_value(h) - death_cost();
}

// Eating the pearl on `p`. A pearl next to a grower is the grower's: a worker
// still gains a little by taking it, a feeder must not.
int food_value(Position p) const
{
    if (!reserved_food(p))
        return Config::PEARL_VALUE;
    if (feeder_target_)
        return Config::RESERVED_PEARL_FEEDER;
    return retaining_length() ? Config::RESERVED_PEARL_KEEPER : Config::RESERVED_PEARL_WORKER;
}

// ---------------------------------------------------------------------------
// Goal
// ---------------------------------------------------------------------------
void choose_goal()
{
    Position const here = controller_.get_position();
    if (feeder_target_)
        base_goal_ = Goal::FEED;
    else if (enemy_target_ && !valuable_now_ && distance(here, *enemy_target_) <= Config::HUNT_RADIUS)
        base_goal_ = Goal::HUNT;
    else if (route_first_ >= 0)
        base_goal_ = Goal::EAT;
    else
        base_goal_ = Goal::EXPLORE;
    prepare_fight();
    goal_ = fight_targets_.empty() ? base_goal_ : Goal::FIGHT;
}

// ---------------------------------------------------------------------------
// v22: the centre rush; v23: only on the maps in Config::CENTRE_MAPS (the first CENTRE_RUSH_END_ROUND rounds)
// ---------------------------------------------------------------------------
std::vector<int> centre_dist_; // path steps to the map centre (bodies ignored), this turn

// v23: the map's size is in Config::CENTRE_MAPS (and the table is switched on).
bool centre_map() const
{
    if (!Config::CENTRE_MAP_TABLE_ENABLED)
        return false;
    for (Config::CentreMap const &m : Config::CENTRE_MAPS)
        if (m.width == width_ && m.height == height_)
            return true;
    return false;
}

bool centre_rush() const
{
    return centre_map() && game_.get_round_num() < Config::CENTRE_RUSH_END_ROUND;
}

int centre_radius() const
{
    return std::max(2, std::min(width_, height_) / Config::CENTRE_RADIUS_DIV);
}

// Once a turn while the rush is on: a search from the centre over the neighbour table.
void prepare_centre()
{
    if (!centre_rush())
    {
        centre_dist_.clear();
        return;
    }
    centre_dist_.assign(memory_.size(), INF);
    int const start = index(Position(width_ / 2, height_ / 2));
    std::vector<int> queue{start};
    centre_dist_[static_cast<std::size_t>(start)] = 0;
    for (std::size_t q = 0; q < queue.size(); ++q)
    {
        int const c = queue[q];
        int const d = centre_dist_[static_cast<std::size_t>(c)];
        for (int e = 0; e < 4; ++e)
        {
            int const n = nb(c, e);
            if (n >= 0 && centre_dist_[static_cast<std::size_t>(n)] > d + 1)
            {
                centre_dist_[static_cast<std::size_t>(n)] = d + 1;
                queue.push_back(n);
            }
        }
    }
}

// CENTRE_PULL per step of path closer to the centre, from outside its radius.
// v25: at most CENTRE_MAX_RUSHERS of us rush: a dragon with that many known teammates
// closer to the centre leaves it to them (once a turn).
mutable int centre_crowd_round_ = -1;
mutable bool centre_crowded_ = false;
bool centre_crowded() const
{
    int const now = game_.get_round_num();
    if (centre_crowd_round_ == now)
        return centre_crowded_;
    centre_crowd_round_ = now;
    int const mine = centre_dist_[static_cast<std::size_t>(index(controller_.get_position()))];
    int closer = 0;
    auto count = [&](Position p, int id, int age)
    {
        int const d = centre_dist_[static_cast<std::size_t>(index(p))];
        if (d < INF && (d + age < mine || (d + age == mine && id < my_id())))
            ++closer;
    };
    for (SeenHead const &h : heads_)
        if (h.team == controller_.get_team() && h.id != my_id())
            count(h.position, h.id, 0);
    for (TeamSignal const &s : signals_)
        if (s.id != my_id() && !seen_head(s.id) && now - s.round <= Config::CLAIM_SIGNAL_AGE)
            count(s.position, s.id, now - s.round);
    centre_crowded_ = closer >= Config::CENTRE_MAX_RUSHERS;
    return centre_crowded_;
}

int centre_pull(Position from, Position to) const
{
    if (centre_dist_.empty() || is_queen()) // v23: the middle is where the fights are
        return 0;
    if (centre_crowded())
        return 0; // v25: enough of us are nearer
    int const df = centre_dist_[static_cast<std::size_t>(index(from))];
    int const dt = centre_dist_[static_cast<std::size_t>(index(to))];
    if (df >= INF || dt >= INF || df <= centre_radius())
        return 0;
    return (df - dt) * Config::CENTRE_PULL;
}

// Progress towards the goal by the move `pm`, plus (v22) the centre rush's pull for
// every goal but fights and feeding.
int goal_progress(Goal g, Candidate const &c, PostMove const &pm) const
{
    int v = goal_progress_base(g, c, pm);
    // NB3: not for a portal crossing: it jumps across the map, and the pull charged it for the
    // whole jump (Stripes: -3300 per crossing, more than 3 pearls), so early on nobody crossed.
    if (!c.head_trade && !pm.path.empty() && g != Goal::FIGHT && g != Goal::FEED && !c.portal)
        v += centre_pull(controller_.get_position(), pm.head);
    if (!c.head_trade && !pm.path.empty())
        v += queen_hunt_pull(controller_.get_position(), pm.head); // v23: whatever the goal
    if (!c.head_trade)
        v -= queen_exit_block(pm); // v23: never take our queen's last ways out
    return v;
}

// Progress towards the goal by the move `pm` (v21: one step or several; pm.path is
// empty when a single step lands where we cannot tell). Per step of progress, so a
// free move of k steps can make up to k steps of it.
int goal_progress_base(Goal g, Candidate const &c, PostMove const &pm) const
{
    if (c.head_trade)
        return 0; // a trade ends us: its value is the trade's
    Position const origin = controller_.get_position();
    bool const keeper = retaining_length();
    bool const known = !pm.path.empty();
    int const k = std::max(1, pm.steps);
    switch (g)
    {
    case Goal::EAT:
    {
        // v21: steps still to go along the planned route (time-aware, see route.hpp);
        // of the equally short ways on, prefer the one the route was checked on.
        int const before = route_remaining_now_;
        int after = before + k; // off the route's reach, or through an unknown portal
        if (known)
        {
            int const r = route_remaining(pm, before + 2 * k + 2);
            if (r < INF)
                after = r;
        }
        int v = std::clamp(before - after, -k, k) * Config::STEP_EAT;
        if (known && follows_route(pm))
            v += Config::ROUTE_TIE;
        return v;
    }
    case Goal::HUNT:
    {
        if (!enemy_target_ || !known)
            return 0;
        int const step_value = game_.get_round_num() >= Config::FEED_START_ROUND ? Config::LATE_STEP_HUNT : Config::STEP_HUNT;
        return (distance(origin, *enemy_target_) - distance(pm.head, *enemy_target_)) * step_value;
    }
    case Goal::FEED:
        return known ? feeding_score(pm.head, k) : 0;
    case Goal::EXPLORE:
    default:
    {
        // The nearest objective (timers, unexplored tiles, pearls beyond the route's
        // reach), the dispersal destination, the late-game meeting point.
        int const radius = objective_radius_;
        int const before = std::min(radius, objective_dist_[static_cast<std::size_t>(index(origin))]);
        int after = before;
        if (known)
            after = std::min(radius, objective_dist_[static_cast<std::size_t>(index(pm.head))]);
        else if (c.unknown_portal && portal_exploring())
            after = Config::UNKNOWN_PORTAL_EXPLORE_COST - 1; // crossing it is reaching that objective
        else if (c.unknown_portal && !keeper && !valuable_now_ && before > Config::WORKER_EXPLORE_COST)
            after = before - 1; // (v13) nothing closer: a portal we never took is worth exploring
        // (A tile the search cannot reach reads as the radius: a strong "stay out".)
        int v = std::min(before - after, k + 1) * Config::STEP_EXPLORE;
        if (known)
        {
            v += disperse_score(pm.head, k);
            v += rendezvous_score(pm.head);
        }
        if (c.unknown_portal && portal_exploring())
            v += Config::PORTAL_EXPLORE_VALUE;
        return v;
    }
    }
}

// ---------------------------------------------------------------------------
// Where a move ends: room and risks
// ---------------------------------------------------------------------------
// Expected loss (negative) and room (positive) at the tile a move ends on.
int position_value(Candidate const &c, PostMove const &pm, bool in_fight) const
{
    int const D = death_cost();
    int const need = pm.length + unknown_body(); // room our whole body needs
    int v = 0;
    if (c.pocket_plan)
        v += (c.region.pearls - 2) * Config::POCKET_NET_PEARL; // eat them, then split out (2 segments)
    else if (c.dfs_ok())
    {
        // v21: the survival search found a way to keep going: no trap risk.
        if (c.exits == 1)
            v -= risk(Config::P_ONE_EXIT, D);
    }
    else if (c.walled)
    {
        // v23: walled in even if no other head moves in: certain death (or, for a dragon
        // that can split out, the remnant), never cheaper than a move that is only
        // contested by another head.
        v -= trap_cost();
    }
    else if (c.dfs_depth >= 0 && c.dfs_complete)
    {
        // v21: it searched every way on and fell short: the share of the target
        // missed is how badly we are trapped.
        double const shortfall = static_cast<double>(c.dfs_target - c.dfs_depth) / c.dfs_target;
        v -= risk(Config::P_DFS_SHORT * shortfall, trap_cost());
    }
    else
    {
        // Not searched, or cut short by the budget: the room estimates.
        if (c.exits == 0)
            v -= risk(Config::P_NO_EXIT, D);
        else if (c.exits == 1)
            v -= risk(Config::P_ONE_EXIT, D);
        // Time-aware: segments that will have moved on count as room.
        if (c.space < need + 3)
            v -= risk(Config::P_SPACE * (need + 3 - c.space) / (need + 3), D);
        if (c.region.dead_end())
            v -= risk(Config::P_DEAD_END, trap_cost());
    }
    v += c.exits * Config::EXIT_VALUE + std::min(need + 5, c.space) * Config::ROOM_CELL_VALUE;
    // Outside a fight (the fight search prices it there) an enemy head next to the tile.
    if (!in_fight)
        v -= c.enemy_threats * risk(Config::P_ADJACENT_ENEMY, D);
    // NB4: a defending worker keeps out of enemy heads' reach (they rammed us 55 times to our 15).
    if (defending())
        v -= c.hard_threats * risk(Config::P_DEFENCE_REACH, D) +
             (in_fight ? 0 : c.enemy_threats * risk(Config::P_DEFENCE_ADJACENT, D));
    v -= c.ally_threats * Config::ALLY_CONGESTION;
    if (valuable_now_)
    {
        v -= c.hard_threats * risk(Config::P_HARD_THREAT, D) + c.soft_threats * risk(Config::P_SOFT_THREAT, D);
        v -= crowd_risk(pm);
    }
    v -= unseen_risk(pm);
    return v;
}

// v21: tiles near where we end (within UNSEEN_RADIUS steps, walls aside) that we
// cannot see now. Every single step ends with the same one such tile; a free move to
// the edge of our window ends with up to a dozen. A head there may
//  - ram a valuable dragon (enemies: P_UNSEEN_ATTACK per enemy head per tile), or
//  - box any dragon of UNSEEN_BOX_MIN_LENGTH+ in before its next move, the survival
//    search notwithstanding (it cannot see them either): P_UNSEEN_BOX per head (ally
//    or enemy) per tile, at what a trap costs.
int unseen_risk(PostMove const &pm) const
{
    bool const ram = valuable_now_;
    bool const box = controller_.get_length() >= Config::UNSEEN_BOX_MIN_LENGTH;
    if (!ram && !box)
        return 0;
    if (pm.path.empty() || !controller_.get_tile(pm.head))
        return 0; // a blind portal exit: priced as a portal
    int const r = Config::UNSEEN_RADIUS;
    int unseen = 0;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (std::abs(dx) + std::abs(dy) <= r &&
                !controller_.get_tile(Position(wrap(pm.head.x + dx, width_), wrap(pm.head.y + dy, height_))))
                ++unseen;
    int v = 0;
    if (ram)
        v += risk(std::min(Config::P_UNSEEN_CAP, unseen * Config::P_UNSEEN_ATTACK * enemy_density_x1e4_ / 10000.0), death_cost());
    if (box)
        v += risk(std::min(Config::P_UNSEEN_CAP, unseen * Config::P_UNSEEN_BOX * (enemy_density_x1e4_ + ally_density_x1e4_) / 10000.0),
                  trap_cost());
    return v;
}

// Valuable dragons: crowds close pockets faster than any static check can see.
int crowd_risk(PostMove const &pm) const
{
    int robust = 0;
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(pm.head, d);
        if (!s.passable || !s.destination_known)
            continue;
        std::size_t const n = static_cast<std::size_t>(index(s.destination));
        if (other_occ_[n] || post_index(pm, static_cast<int>(n)) >= 0)
            continue;
        if (enemy_threats_[n] == 0 && ally_threats_[n] == 0)
            ++robust;
    }
    double p = robust == 0 ? Config::P_NO_ROBUST_EXIT : robust == 1 ? Config::P_ONE_ROBUST_EXIT
                                                                    : 0.0;
    int crowd = 0;
    for (int dy = -Config::CROWD_RADIUS; dy <= Config::CROWD_RADIUS; ++dy)
        for (int dx = -Config::CROWD_RADIUS; dx <= Config::CROWD_RADIUS; ++dx)
        {
            Position const q(wrap(pm.head.x + dx, width_), wrap(pm.head.y + dy, height_));
            if (other_occ_[static_cast<std::size_t>(index(q))])
                ++crowd;
        }
    p += crowd * Config::P_CROWD_SEGMENT;
    return risk(p, death_cost());
}

// Portals: the risk of an exit we cannot see, how rich the far side is, loops.
int portal_value(Candidate const &c, Direction direction, bool blind, bool exit_occupied, int progress) const
{
    int const D = death_cost();
    int v = 0;
    if (stuck_in_room_ && !exit_occupied)
        v += Config::ROOM_EXIT_VALUE - risk(Config::P_ROOM_EXIT, D); // v23: the only way on
    else if (c.unknown_portal)
        v -= risk(portal_exploring() ? Config::P_WINDOW_PORTAL : blind_portal_p(), D);
    else
    {
        double const p = blind_portal_p(); // v24: by round, as measured (was 0.3 throughout)
        if (exit_occupied)
            v -= risk(Config::P_OCCUPIED_PORTAL, D);
        else if (blind)
        {
            CellMemory const &m = cell(c.destination);
            bool const seen_free = m.seen && !m.other_seen && game_.get_round_num() - m.last_seen <= Config::KNOWN_FREE_MEMORY;
            v -= risk(seen_free ? p / 2 : p, D);
        }
        // v24: the far side. Early, unexplored tiles count; late, only what we know is there.
        // Neither bonus below for an exit with no way on (a portal pocket, a dead end): there
        // the pearls are the bait (on Portals small workers went in, ate, and died in them).
        bool const way_on = c.pocket_plan || (c.exits > 0 && !c.walled && !c.region.trap(c.new_length + unknown_body()));
        bool const early = game_.get_round_num() < Config::PORTAL_EXPLORE_END_ROUND;
        if (early)
        {
            int const diff = region_value(c.destination) - region_value(controller_.get_position()); // pearls x100
            v += std::clamp(diff * Config::PORTAL_REGION_PER_PEARL / 100, -Config::PORTAL_REGION_CAP, Config::PORTAL_REGION_CAP);
        }
        else
        {
            // Late: what we know is there, and nothing for a crossing with no way on.
            int const diff = region_value(c.destination, 0) - region_value(controller_.get_position(), 0); // pearls x100
            if (diff > 0)
                v += way_on ? std::min(diff * Config::PORTAL_GOOD_PER_PEARL / 100, Config::PORTAL_GOOD_CAP) : 0;
            else
                v += std::max(diff * Config::PORTAL_REGION_PER_PEARL / 100, -Config::PORTAL_REGION_CAP);
        }
    }
    // Back through the pair we just used, with nothing to eat that way: a loop.
    Position const origin = controller_.get_position();
    int const id = cell(origin).edges[static_cast<std::size_t>(direction_index(direction))].portal_id;
    if (id >= 0 && id == last_portal_id_ &&
        game_.get_round_num() - last_portal_round_ <= Config::PORTAL_LOOP_ROUNDS && progress <= 0)
        v -= Config::PORTAL_LOOP;
    return v;
}

// Per step away from the nearest visible enemy head (outside fights).
int enemy_distance_step() const
{
    if (is_queen())
        return Config::QUEEN_ENEMY_DISTANCE_STEP; // v23
    return retaining_length() ? Config::KEEPER_ENEMY_DISTANCE_STEP : Config::ENEMY_DISTANCE_STEP;
}

// The whole value of a single step (see evaluate_single for how the candidate's
// facts are gathered).
int move_value(Candidate const &c, PostMove const &pm, Direction direction, Step const &step,
               bool blind_portal, bool exit_occupied, int trade) const
{
    bool const keeper = retaining_length();
    bool const fight = goal_ == Goal::FIGHT;
    int v = deterministic_jitter(direction) / 5; // 0-20: ties only
    if (direction == controller_.get_dir().get_opposite())
        v -= death_cost(); // into our own neck
    if (c.head_trade)
        return v + trade;
    // The goal.
    int const progress = goal_progress(fight ? base_goal_ : goal_, c, pm);
    v += fight ? static_cast<int>(progress * Config::FIGHT_BASE_PULL) : progress;
    // Food.
    if (c.direct_pearl)
        v += food_value(step.destination);
    if (c.portal)
        v += portal_value(c, direction, blind_portal, exit_occupied, progress);
    if (!step.destination_known)
        return v;
    // Room and risks where we end up, and the fight from there.
    v += position_value(c, pm, fight);
    if (fight)
        v += fight_value(pm, direction);
    // Small habits (old weights relative to a pearl).
    CellMemory const &destination = cell(step.destination);
    if (!good_bed(destination)) // v23: farming a good bed is not wandering in circles
        v -= destination.visits * (keeper ? Config::KEEPER_VISIT_COST : Config::VISIT_COST);
    if (!c.direct_pearl)
    {
        v += teammate_spacing_score(step.destination); // never talk a dragon out of an adjacent pearl
        if (unswbc::Tile const *tile = controller_.get_tile(step.destination))
        {
            int const countdown = tile->get_pearl_time();
            if (countdown >= 1 && countdown <= c.new_length - 1)
                v -= Config::BLOCKED_SPAWN; // our body would sit on it when it spawns
        }
    }
    if (!fight)
    {
        auto const [enemy_before, enemy_after] = nearest_visible_enemy_delta(controller_.get_position(), step.destination);
        if (enemy_before < INF)
            v += (enemy_after - enemy_before) * enemy_distance_step();
    }
    return v;
}

// ---------------------------------------------------------------------------
// Fights
// ---------------------------------------------------------------------------
struct FightTarget
{
    int id = -1;
    Position head{};
    Direction facing = Direction::NORTH;
    int length = 2;                   // lower bound: its segments free up no later than this says
    bool truncated = true;            // its body may run out of sight
    int value = 0;                    // kill_value
    int trade = 0;                    // trade_value
    bool tradeable = false;           // we may move into its head (eligible_target)
    std::unordered_map<int, int> own; // its visible segments: cell -> index
};
std::vector<FightTarget> fight_targets_;
mutable std::vector<int> fight_stamp_;
mutable int fight_gen_ = 0;

void prepare_fight()
{
    fight_targets_.clear();
    if (is_queen())
        return; // v23: she stays out of fights
    Position const here = controller_.get_position();
    std::vector<std::pair<int, SeenHead const *>> near;
    for (SeenHead const &h : heads_)
        if (h.team != controller_.get_team())
        {
            int const d = distance(here, h.position);
            if (d <= Config::FIGHT_RADIUS)
                near.push_back({d, &h});
        }
    std::sort(near.begin(), near.end(), [](auto const &a, auto const &b)
              { return a.first < b.first; });
    if (static_cast<int>(near.size()) > Config::FIGHT_MAX_ENEMIES)
        near.resize(static_cast<std::size_t>(Config::FIGHT_MAX_ENEMIES));
    for (auto const &[d, h] : near)
    {
        FightTarget t;
        t.id = h->id;
        t.head = h->position;
        t.facing = h->facing;
        t.length = std::max(2, h->lower_length);
        t.truncated = h->truncated;
        t.value = kill_value(*h);
        t.trade = trade_value(*h);
        t.tradeable = eligible_target(*h);
        BodyTrace const trace = trace_body(h->position, h->id);
        for (std::size_t i = 0; i < trace.segments.size(); ++i)
            t.own[index(trace.segments[i])] = static_cast<int>(i);
        fight_targets_.push_back(std::move(t));
    }
}

// Out of sight: a body seen there a moment ago is probably still there.
bool remembered_occupied(int c) const
{
    return body_remembered(memory_[static_cast<std::size_t>(c)]); // v21: for as long as it will be there
}

// Is `c` a wall for the enemy's next move, after our move left us as `pm`? Every
// segment still blocks (the tail moves only after the collision check).
bool blocked_for_enemy(FightTarget const &t, int c, PostMove const &pm) const
{
    return post_index(pm, c) >= 0 || t.own.count(c) || other_occ_[static_cast<std::size_t>(c)] || remembered_occupied(c);
}

// Is `c` a wall for our answer, after the enemy moved to `enemy_cell`?
bool blocked_for_us(FightTarget const &t, int c, PostMove const &pm) const
{
    return post_index(pm, c) >= 0 || t.own.count(c) || other_occ_[static_cast<std::size_t>(c)] || remembered_occupied(c);
}

// Room the enemy head has after its move to `enemy_cell` and our answer to `our_cell`,
// move by move (time-aware: its own segments and ours free up as each moves on), up
// to its length + 3.
int enemy_room_after(FightTarget const &t, int enemy_cell, Direction enemy_dir, PostMove const &pm, int our_cell) const
{
    if (fight_stamp_.size() != memory_.size())
        fight_stamp_.assign(memory_.size(), 0);
    ++fight_gen_;
    int const gen = fight_gen_;
    int const cap = t.length + 3;
    auto our_index = [&](int c)
    {
        if (c == our_cell)
            return 0;
        int const j = post_index(pm, c);
        return j >= 0 && j + 1 < pm.length ? j + 1 : -1;
    };
    auto its_index = [&](int c)
    {
        if (c == enemy_cell)
            return 0;
        auto const it = t.own.find(c);
        return it != t.own.end() && it->second + 1 < t.length ? it->second + 1 : -1;
    };
    std::deque<std::pair<int, int>> queue{{enemy_cell, 0}};
    fight_stamp_[static_cast<std::size_t>(enemy_cell)] = gen;
    int room = 0;
    while (!queue.empty() && room < cap)
    {
        auto const [c, k0] = queue.front();
        queue.pop_front();
        int const k = k0 + 1;
        for (Direction d : Direction::get_direction_list())
        {
            if (k0 == 0 && d == enemy_dir.get_opposite())
                continue;
            Step const s = step_from(position_of(c), d);
            if (!s.passable || !s.destination_known)
                continue;
            int const n = index(s.destination);
            if (fight_stamp_[static_cast<std::size_t>(n)] == gen)
                continue;
            int const j = our_index(n);
            if (j >= 0 && k < pm.length - j + 1)
                continue; // not free yet: perhaps later, by another way
            int const i = its_index(n);
            if (i >= 0 && k < t.length - i + 1)
                continue;
            if (j < 0 && i < 0 && !t.own.count(n) && (other_occ_[static_cast<std::size_t>(n)] || remembered_occupied(n)))
                continue;
            fight_stamp_[static_cast<std::size_t>(n)] = gen;
            ++room;
            queue.push_back({n, k});
        }
    }
    return room;
}

// After the enemy moved to `enemy_cell` (heading `enemy_dir`) and we answered to
// `our_cell`: can it ram us next, has it room to live, have we a way on?
int fight_leaf(FightTarget const &t, PostMove const &pm, int enemy_cell, Direction enemy_dir, int our_cell) const
{
    int const D = death_cost();
    bool const kills_count = game_.get_round_num() >= Config::AGGRESSION_START_ROUND;
    int v = 0;
    // It can step onto our head next: it will if the trade pays for it.
    for (Direction d : Direction::get_direction_list())
    {
        if (d == enemy_dir.get_opposite())
            continue;
        Step const s = step_from(position_of(enemy_cell), d);
        if (s.passable && s.destination_known && index(s.destination) == our_cell)
        {
            // NB4: defending, a ram is our loss whatever it costs them (they can afford it).
            int const loss = defending() ? D : D - t.value; // what the trade costs us
            if (loss > 0)
                v -= risk(defending() ? Config::P_DEFENCE_ADJACENT : Config::P_RAM_PROFITABLE, loss);
            break;
        }
    }
    // Its room: too little to live in is a kill without a trade; less room is pressure.
    if (kills_count)
    {
        int const room = enemy_room_after(t, enemy_cell, enemy_dir, pm, our_cell);
        if (room < t.length)
            v += risk(Config::P_TRAPPED_KILL, t.value);
        else
            v += (t.length + 3 - std::min(room, t.length + 3)) * Config::FIGHT_PRESSURE_PER_CELL;
    }
    // Our way on from there.
    int exits = 0;
    for (Direction d : Direction::get_direction_list())
    {
        Step const s = step_from(position_of(our_cell), d);
        if (!s.passable || !s.destination_known)
            continue;
        int const n = index(s.destination);
        if (n == our_cell || n == enemy_cell || index(pm.head) == n || blocked_for_us(t, n, pm))
            continue;
        ++exits;
    }
    if (exits == 0)
        v -= risk(Config::P_NO_EXIT, trap_cost());
    return v;
}

// Can the enemy head reach `target` (our head) in 2..reach steps through free tiles?
bool enemy_can_sprint_to(FightTarget const &t, PostMove const &pm, int target, int reach) const
{
    std::vector<int> frontier{index(t.head)}, seen{index(t.head)};
    for (int depth = 1; depth <= reach && !frontier.empty(); ++depth)
    {
        std::vector<int> next;
        for (int c : frontier)
            for (Direction d : Direction::get_direction_list())
            {
                if (depth == 1 && d == t.facing.get_opposite())
                    continue;
                Step const s = step_from(position_of(c), d);
                if (!s.passable || !s.destination_known)
                    continue;
                int const n = index(s.destination);
                if (n == target)
                {
                    if (depth >= 2)
                        return true;
                    continue;
                }
                if (std::find(seen.begin(), seen.end(), n) != seen.end() || blocked_for_enemy(t, n, pm))
                    continue;
                seen.push_back(n);
                next.push_back(n);
            }
        frontier = std::move(next);
    }
    return false;
}

// One enemy: each of its replies (the ones that do not kill it outright), our best
// answer to each, and the result weighted towards its best reply (worst for us).
int fight_value_one(FightTarget const &t, PostMove const &pm, Direction our_dir) const
{
    bool const kills_count = game_.get_round_num() >= Config::AGGRESSION_START_ROUND;
    int const kill = kills_count ? t.value : 0;
    int const our_head = index(pm.head);
    std::vector<int> outcomes;
    for (Direction d : Direction::get_direction_list())
    {
        if (d == t.facing.get_opposite())
            continue;
        Step const s = step_from(t.head, d);
        if (!s.passable)
            continue; // kelp: it would die, so it will not go there
        if (!s.destination_known)
        {
            outcomes.push_back(0); // through a portal we cannot follow: gone
            continue;
        }
        int const n = index(s.destination);
        if (n == our_head)
        {
            outcomes.push_back(t.trade); // it rams us: both die
            continue;
        }
        if (blocked_for_enemy(t, n, pm))
            continue; // it would die: it will not go there
        // Our answer.
        int best = std::numeric_limits<int>::min();
        for (Direction m : Direction::get_direction_list())
        {
            if (m == our_dir.get_opposite())
                continue;
            Step const s2 = step_from(pm.head, m);
            if (!s2.passable || !s2.destination_known)
                continue;
            int const n2 = index(s2.destination);
            if (n2 == n)
            {
                if (t.tradeable)
                    best = std::max(best, t.trade); // into its new head
                continue;
            }
            if (blocked_for_us(t, n2, pm))
                continue;
            best = std::max(best, fight_leaf(t, pm, n, d, n2));
        }
        if (best == std::numeric_limits<int>::min())
            best = -trap_cost(); // no way on: split out, or die
        outcomes.push_back(best);
    }
    // It can also sprint into our head (2-3 steps, a segment per paid step) when it
    // is long enough; a body running out of sight is assumed long enough for 2.
    // v21: its free steps reach that far at no cost, however many they are.
    int const len = t.truncated ? std::max(t.length + 1, 3) : t.length;
    int const reach = std::clamp(max_turn_steps(len), 1, std::max(Config::FIGHT_SPRINT_REACH, free_moves(len)));
    if (reach >= 2 && enemy_can_sprint_to(t, pm, our_head, reach))
        outcomes.push_back(t.trade);
    if (outcomes.empty())
        return kill; // every move kills it
    int const worst = *std::min_element(outcomes.begin(), outcomes.end());
    double mean = 0.0;
    for (int o : outcomes)
        mean += o;
    mean /= static_cast<double>(outcomes.size());
    return static_cast<int>(std::lround(Config::FIGHT_WORST_WEIGHT * worst + (1.0 - Config::FIGHT_WORST_WEIGHT) * mean));
}

// The fight from where a move leaves us (`pm`, arriving heading `our_dir`).
int fight_value(PostMove const &pm, Direction our_dir) const
{
    int total = 0;
    for (FightTarget const &t : fight_targets_)
        total += fight_value_one(t, pm, our_dir);
    return total;
}