// Focused invariant fixtures for bot-v12.0-kushagra (and, for comparison, v10.3.3).
// Build from the repository root:
//   clang++ -std=c++20 -O2 -DV12 battlecode-automation/scripts/test_v12.cpp -o /tmp/test_v12 && /tmp/test_v12
//   clang++ -std=c++20 -O2 -DBOT_HELPER='"../../bots/current/bot-v10.3.3/helper.hpp"' -DBOT_SOURCE='"../../bots/current/bot-v10.3.3/main.cpp"' battlecode-automation/scripts/test_v12.cpp -o /tmp/test_v1033 && /tmp/test_v1033
// Each check prints PASS/FAIL; the exit status is the number of failures. The
// baseline is EXPECTED to fail the checks that demonstrate the fixed defects.
#ifndef BOT_HELPER
#define BOT_HELPER "../../bots/current/bot-v12.0-kushagra/helper.hpp"
#endif
#ifndef BOT_SOURCE
#define BOT_SOURCE "../../bots/current/bot-v12.0-kushagra/main.cpp"
#endif
#include BOT_HELPER
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <tuple>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#define main battlecode_program_main
#include BOT_SOURCE
#undef main
#undef private
using Claim = ReplayDerivedBot::Claim;
using CycleMap = ReplayDerivedBot::CycleMap;

static int failures = 0;
static void check(bool ok, char const *name)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        ++failures;
}

struct Fixture
{
    unswbc::Game game;
    unswbc::Controller ct;
    ReplayDerivedBot bot;
    explicit Fixture(int w = 32, int h = 32, int id = 100, int round = 300)
        : game(w, h, 64), ct(id, unswbc::Team::A, Direction::EAST, {}, 64), bot(ct, game)
    {
        unswbc::game = &game;
        unswbc::ct = &ct;
        game.round_num = round;
        ct.head.position = Position(5, 5);
        ct.length = 3;
        ct.unit_count = 20;
        bot.first_round_ = 1;
    }
    RememberedEdge &edge(Position p, Direction d) { return bot.cell(p).edges[static_cast<std::size_t>(direction_index(d))]; }
    // Known terrain everywhere: all tiles seen, every edge open.
    void know_all_open()
    {
        for (int y = 0; y < game.height; ++y)
            for (int x = 0; x < game.width; ++x)
            {
                auto &m = bot.cell(Position(x, y));
                m.seen = true;
                m.last_seen = game.round_num;
                for (auto &e : m.edges)
                    e = {true, EdgeType::EMPTY, -1};
            }
    }
    void set_kelp(Position p, Direction d)
    {
        edge(p, d) = {true, EdgeType::KELP, -1};
        edge(p.add_dir(d), d.get_opposite()) = {true, EdgeType::KELP, -1};
    }
};

static Direction dir_between(Position a, Position b, int W, int H)
{
    for (Direction d : Direction::get_direction_list())
    {
        auto [dx, dy] = d.get_offset();
        if (Position(((a.x + dx) % W + W) % W, ((a.y + dy) % H + H) % H) == b)
            return d;
    }
    return Direction::NORTH; // not adjacent: caller checks adjacency separately
}
static bool adjacent(Position a, Position b)
{
    int dx = std::abs(a.x - b.x), dy = std::abs(a.y - b.y);
    return dx + dy == 1;
}

// ---------------------------------------------------------------------------
// 1. Cycle generator: every variant is a Hamiltonian cycle of the rectangle.
// ---------------------------------------------------------------------------
static void cycle_variants_are_hamiltonian()
{
    Fixture f;
    bool ok = true;
    for (int w = 2; w <= 9; ++w)
        for (int h = 2; h <= 9; ++h)
            for (int v = 0; v < 4; ++v)
            {
                Claim r{};
                r.x = 3, r.y = 4, r.w = w, r.h = h;
                CycleMap cm;
                f.bot.make_cycle(r, v, cm);
                int const b = (v & 1) ? w : h;
                if (b % 2 != 0)
                {
                    ok = ok && cm.cells.empty();
                    continue;
                }
                if (static_cast<int>(cm.cells.size()) != w * h)
                {
                    ok = false;
                    continue;
                }
                std::set<std::pair<int, int>> seen;
                for (std::size_t i = 0; i < cm.cells.size(); ++i)
                {
                    Position p = cm.cells[i], q = cm.cells[(i + 1) % cm.cells.size()];
                    ok = ok && r.contains(p) && adjacent(p, q);
                    seen.insert({p.x, p.y});
                }
                ok = ok && static_cast<int>(seen.size()) == w * h;
            }
    check(ok, "cycle: all variants are Hamiltonian cycles inside the rectangle");
}

// ---------------------------------------------------------------------------
// 2. Territory validation certifies EVERY edge of the chosen cycle (randomised).
// ---------------------------------------------------------------------------
static void claimed_cycle_edges_all_clear()
{
    std::mt19937 rng(12345);
    int bad = 0, claims = 0;
    for (int trial = 0; trial < 400; ++trial)
    {
        Fixture f(20, 20);
        f.know_all_open();
        f.ct.length = 10;
        f.ct.head.position = Position(10, 10);
        f.bot.retaining_now_ = true;
        std::uniform_int_distribution<int> coord(0, 19), dir(0, 3), count(1, 25);
        int const n = count(rng);
        for (int k = 0; k < n; ++k)
            f.set_kelp(Position(coord(rng), coord(rng)), Direction::get_direction_list()[static_cast<std::size_t>(dir(rng))]);
        f.bot.map_territory();
        if (!f.bot.territory_.w)
            continue;
        ++claims;
        CycleMap const &cm = f.bot.patrol_cycle_;
        for (std::size_t i = 0; i < cm.cells.size(); ++i)
        {
            Position p = cm.cells[i], q = cm.cells[(i + 1) % cm.cells.size()];
            if (!f.bot.edge_clear(p, dir_between(p, q, 20, 20)))
            {
                ++bad;
                break;
            }
        }
    }
    std::printf("  (claims=%d, claims with a blocked cycle edge=%d)\n", claims, bad);
    check(claims > 50 && bad == 0, "territory: every edge of a claimed cycle is known clear");
}

// Specific fixture: kelp on the edge between (1,b-1) and (0,b-1) of the layout.
static void closing_edge_kelp_rejected()
{
    Fixture f(20, 20);
    f.know_all_open();
    f.ct.length = 10;
    f.ct.head.position = Position(10, 10);
    f.bot.retaining_now_ = true;
    // Make kelp between every cell and its west neighbour in column x -> blocks
    // many "closing" edges for variant 0 anchored at x0 = x-1. Simplest: rerun the
    // generic invariant on one hand-built case that 10.3.3 misses.
    // Rectangle chosen by baseline (found empirically in the randomised test) is
    // checked there; here we assert the validator itself on a direct 4x4 layout.
    Claim r{};
    r.x = 8, r.y = 8, r.w = 4, r.h = 4;
    CycleMap cm;
    f.bot.make_cycle(r, 0, cm);
    // Closing edge for variant 0: between at(1,b-1) and at(0,b-1) = (9,11)-(8,11).
    f.set_kelp(Position(9, 11), Direction::WEST);
    f.bot.map_territory();
    bool uses_blocked = false;
    CycleMap const &pc = f.bot.patrol_cycle_;
    for (std::size_t i = 0; i < pc.cells.size(); ++i)
    {
        Position p = pc.cells[i], q = pc.cells[(i + 1) % pc.cells.size()];
        if (!f.bot.edge_clear(p, dir_between(p, q, 20, 20)))
            uses_blocked = true;
    }
    check(!uses_blocked, "territory: cycle never crosses the kelp closing edge (1,b-1)-(0,b-1)");
}

// ---------------------------------------------------------------------------
// 3. Truthful terrain sharing.
// ---------------------------------------------------------------------------
static void share(ReplayDerivedBot &bot, Claim const &c, int variant)
{
#ifdef V12
    bot.share_terrain(c, variant);
#else
    (void)variant;
    bot.share_terrain(c);
#endif
}

static void terrain_sharing_is_truthful()
{
    for (int v = 0; v < 4; ++v)
    {
        Fixture f(20, 20);
        Claim c{};
        c.x = 4, c.y = 4, c.w = 4, c.h = 4;
        // Receiver knows nothing. After sharing, exactly the cycle edges are known clear.
        share(f.bot, c, v);
        CycleMap cm;
        f.bot.make_cycle(c, v, cm);
        std::set<std::tuple<int, int, int>> cyc; // (cell index, dir index)
        for (std::size_t i = 0; i < cm.cells.size(); ++i)
        {
            Position p = cm.cells[i], q = cm.cells[(i + 1) % cm.cells.size()];
            Direction d = dir_between(p, q, 20, 20);
            cyc.insert({p.x, p.y, direction_index(d)});
            cyc.insert({q.x, q.y, direction_index(d.get_opposite())});
        }
        bool cycle_known = true, chord_invented = false;
        for (int y = c.y; y < c.y + c.h; ++y)
            for (int x = c.x; x < c.x + c.w; ++x)
                for (Direction d : Direction::get_direction_list())
                {
                    Position q = Position(x, y).add_dir(d);
                    if (!c.contains(q))
                        continue;
                    auto const &e = f.edge(Position(x, y), d);
                    bool const on_cycle = cyc.count({x, y, direction_index(d)}) > 0;
                    if (on_cycle && !(e.known && e.type == EdgeType::EMPTY))
                        cycle_known = false;
                    if (!on_cycle && e.known)
                        chord_invented = true;
                }
        std::string name = "terrain: variant " + std::to_string(v) + " shares only cycle edges (no invented chords)";
        check(cycle_known && !chord_invented, name.c_str());
    }
    // Unused interior kelp chord: the sender's cycle is valid, the chord is kelp.
    // A fresh child must NOT learn that chord as open.
    {
        Fixture f(20, 20);
        Claim c{};
        c.x = 4, c.y = 4, c.w = 4, c.h = 4;
        CycleMap cm;
        f.bot.make_cycle(c, 0, cm);
        // For variant 0, the vertical link between rows 1 and 2 at x=6 (u=2) is unused.
        Position a(6, 5), b(6, 6);
        bool used = false;
        for (std::size_t i = 0; i < cm.cells.size(); ++i)
        {
            Position p = cm.cells[i], q = cm.cells[(i + 1) % cm.cells.size()];
            if ((p == a && q == b) || (p == b && q == a))
                used = true;
        }
        share(f.bot, c, 0);
        auto const &e = f.edge(a, Direction::SOUTH);
        check(!used && !(e.known && e.type == EdgeType::EMPTY), "terrain: unused kelp chord is not certified open to a fresh child");
    }
    // Stronger direct observation is never overwritten.
    {
        Fixture f(20, 20);
        Claim c{};
        c.x = 4, c.y = 4, c.w = 4, c.h = 4;
        f.set_kelp(Position(5, 4), Direction::EAST); // a cycle edge for variant 0
        share(f.bot, c, 0);
        check(f.edge(Position(5, 4), Direction::EAST).type == EdgeType::KELP, "terrain: observed kelp is never overwritten by inference");
    }
}

// ---------------------------------------------------------------------------
// 4. Packets: round trips and stale records.
// ---------------------------------------------------------------------------
static void beacon_round_trip()
{
    Fixture a(40, 40, 7, 390), b(40, 40, 9, 390);
    unswbc::game = &a.game;
    a.bot.king_ = decltype(a.bot.king_){7, 23, Position(12, 30), 389};
    std::uint64_t m = a.bot.beacon_message(false);
    unswbc::game = &b.game;
    b.bot.receive_beacon(m);
    check(b.bot.king_.id == 7 && b.bot.king_.length == 23 && b.bot.king_.pos == Position(12, 30) && b.bot.king_.obs == 389,
          "packet: king beacon round trip");
    a.bot.king_rect_ = decltype(a.bot.king_rect_){7, Claim{7, 23, 380, 10, 20, 6, 8}, 3, true, 388};
    unswbc::game = &a.game;
    std::uint64_t t = a.bot.beacon_message(true);
    unswbc::game = &b.game;
    b.bot.receive_beacon(t);
    auto it = b.bot.known_territories_.find(7);
    check(it != b.bot.known_territories_.end() && it->second.rect.x == 10 && it->second.rect.w == 6 &&
              it->second.variant == 3 && it->second.reverse && it->second.obs == 388,
          "packet: territory beacon round trip");
    // Corrupted packet rejected.
    Fixture c(40, 40, 11, 390);
    c.bot.receive_beacon(m ^ (1ULL << 40U));
    check(c.bot.king_.id == -1, "packet: corrupted beacon rejected by checksum");
}

// A split-escape heir must ignore its parent's pre-split king report, including
// one stamped with the heir's own birth round.
static void heir_ignores_parent_report_from_birth_round()
{
    Fixture parent(40, 40, 50, 420);
    parent.bot.king_ = decltype(parent.bot.king_){50, 30, Position(10, 10), 420};
    std::uint64_t m = parent.bot.beacon_message(false);
    Fixture child(40, 40, 60, 420);
    child.ct.length = 28;
    child.bot.first_round_ = 420;
    child.bot.heir_ = true;
    child.bot.birth_length_ = 28;
    child.bot.receive_beacon(m);
    check(child.bot.king_.id != 50, "escape: heir ignores parent's stale king beacon from its birth round");
}

// After a split escape the parent (now length 2) must not broadcast its old king state.
static void parent_does_not_advertise_old_king_after_escape()
{
    Fixture f(40, 40, 50, 300);
    int const L = 20;
    f.ct.length = L;
    f.ct.head.position = Position(20, 20);
    f.ct.head.dir = Direction::EAST;
    f.bot.retaining_now_ = true;
    f.bot.valuable_now_ = true;
    f.bot.founder_ = true;
    // Body lies west of the head in a straight line.
    f.bot.own_trail_.clear();
    for (int i = 0; i < L; ++i)
        f.bot.own_trail_.push_back(Position(20 - i, 20));
    f.bot.known_body_ = L;
    for (int i = 0; i < L; ++i)
    {
        int c = f.bot.index(Position(20 - i, 20));
        f.bot.own_idx_[static_cast<std::size_t>(c)] = i;
        f.bot.touched_.push_back(c);
    }
    f.bot.king_ = decltype(f.bot.king_){50, L, Position(20, 20), 300};
    std::vector<Candidate> none;
    std::ostringstream out;
    auto *old = std::cout.rdbuf(out.rdbuf());
    bool const split = f.bot.play_split_escape(none);
    std::cout.rdbuf(old);
    bool stale = false;
    std::istringstream in(out.str());
    for (std::string line; std::getline(in, line);)
    {
        if (line.rfind("SONAR ", 0) != 0)
            continue;
        std::uint64_t msg = std::stoull(line.substr(line.rfind(' ') + 1));
        if ((msg >> 60U) == 0xEULL && ((msg >> 58U) & 1ULL) == 0)
        {
            int const id = static_cast<int>((msg >> 46U) & 4095ULL), len = static_cast<int>((msg >> 37U) & 511ULL);
            if (id == 50 && len > 2)
                stale = true;
        }
    }
    check(split, "escape: long cornered grower splits (behaviour preserved)");
    check(!stale, "escape: parent does not beacon its pre-split king length");
}

// Visible truncated head must not keep an old exact length fresh forever.
static void stale_length_not_renewed_by_sighting()
{
    Fixture f(40, 40, 9, 300);
    TeamSignal s{30, Position(10, 10), 25, 290, true};
#ifdef V12
    s.length_round = 290;
#endif
    f.bot.signals_.push_back(s);
    SeenHead h;
    h.id = 30;
    h.team = unswbc::Team::A;
    h.position = Position(11, 10);
    h.visible_length = 3;
    h.lower_length = 3;
    h.truncated = true;
    f.bot.heads_.push_back(h);
    f.bot.update_coordination();
    auto const &r = f.bot.signals_.front();
    check(r.position == Position(11, 10), "freshness: sighting refreshes position");
    check(r.length < 25, "freshness: 10-round-old exact length not renewed by a truncated sighting");
    // A complete sighting gives the exact (shrunken) length.
    Fixture g(40, 40, 9, 300);
    g.bot.signals_.push_back(TeamSignal{30, Position(10, 10), 25, 299, true});
    h.truncated = false;
    h.lower_length = 2;
    h.visible_length = 2;
    g.bot.heads_.push_back(h);
    g.bot.update_coordination();
    check(g.bot.signals_.front().length == 2, "freshness: complete sighting records exact shrunken length");
    // King record: a truncated sighting of an old record must not refresh it as exact.
    Fixture k(40, 40, 9, 300);
    // Exact length heard 20 rounds ago; position last known 10 rounds ago (within TTL).
    k.bot.king_ = decltype(k.bot.king_){30, 25, Position(10, 10), 290};
#ifdef V12
    k.bot.king_.len_obs = 280;
#endif
    k.bot.heads_.push_back(SeenHead{30, unswbc::Team::A, Position(11, 10), Direction::EAST, 4, 4, true});
    k.bot.update_king_info();
    check(k.bot.king_.pos == Position(11, 10), "freshness: king sighting refreshes position");
    check(k.bot.king_.length < 25, "freshness: 20-round-old king length not renewed as exact by a truncated sighting");
}

// ---------------------------------------------------------------------------
// 5. Donations.
// ---------------------------------------------------------------------------
struct DonationFixture : Fixture
{
    DonationFixture(int round, int donor_len, int king_len, Position king_pos, int king_vis_len, bool trunc)
        : Fixture(40, 40, 70, round)
    {
        know_all_open();
        ct.length = donor_len;
        ct.head.position = Position(20, 20);
        ct.unit_count = 20;
        bot.retaining_now_ = false;
        bot.king_ = decltype(bot.king_){5, king_len, king_pos, round};
        bot.feeder_target_ = TeamSignal{5, king_pos, king_len, round, true};
        bot.king_head_estimate_ = king_pos;
        bot.king_cycle_index_ = -1;
        // Vision around the donor so the king head is "visible".
        std::vector<unswbc::Tile> tiles;
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
            {
                Position p(20 + dx, 20 + dy);
                std::optional<unswbc::DragonPart> part;
                if (p == king_pos)
                    part = unswbc::DragonPart(p, 5, unswbc::Team::A, Direction::NORTH, true);
                tiles.emplace_back(p, part, -1, false);
            }
        ct.vision = unswbc::Vision(tiles);
        bot.heads_.push_back(SeenHead{5, unswbc::Team::A, king_pos, Direction::NORTH, king_vis_len, king_vis_len, trunc});
        // Donor body: straight line west.
        for (int i = 0; i < donor_len; ++i)
            bot.own_trail_.push_back(Position(20 - i, 20));
        bot.known_body_ = donor_len;
    }
};

static void donation_checks()
{
    // Longer king 3 steps away with plenty of time: approved (baseline behaviour preserved).
    {
        DonationFixture f(420, 8, 25, Position(20, 23), 25, false);
        check(f.bot.good_sacrifice_spot(), "donation: short donor near a much longer visible king approved");
    }
    // Equal-length recipient: not a defensible sacrifice (r445 A721->562, 13 vs 13).
    {
        DonationFixture f(445, 13, 13, Position(20, 23), 13, false);
        check(!f.bot.good_sacrifice_spot(), "donation: equal-length recipient rejected");
    }
    // Deadline by ROUTE, not Manhattan: king 3 cells away by Manhattan but walled
    // off so the route is long; 4 rounds left.
    {
        DonationFixture f(496, 6, 25, Position(20, 23), 25, false);
        // Kelp under (20,21) and (21,21): the king's route is (20,22) (19,22)
        // (19,21) (20,21) (20,20) = 5 steps > 3 rounds usable, Manhattan is 3.
        for (int x = 20; x <= 21; ++x)
            f.set_kelp(Position(x, 21), Direction::SOUTH);
        check(!f.bot.good_sacrifice_spot(), "donation: route ETA beyond the deadline rejected (Manhattan would pass)");
    }
    // Deadline: direct route of 3 steps but only 3 rounds left -> the king cannot
    // also collect the drop cells behind the head in time.
    {
        DonationFixture f(498, 6, 25, Position(20, 23), 25, false);
        check(!f.bot.good_sacrifice_spot(), "donation: no harvest time left rejected");
    }
}

// Audit reconstructions on schooltime geometry (60x40), from the donor's own view:
// positions/lengths from death-audit.json; nothing omniscient is fed to the bot
// beyond what the donor could see (the king's head in its 7x7 window).
struct AuditDonation : Fixture
{
    AuditDonation(int round, int donor_id, int donor_len, std::vector<Position> trail, int king_id, int king_len,
                  Position king_pos, Direction king_dir, std::vector<std::pair<int, Position>> others)
        : Fixture(60, 40, donor_id, round)
    {
        know_all_open();
        ct.length = donor_len;
        ct.head.position = trail.front();
        ct.unit_count = 30;
        bot.retaining_now_ = false;
        bot.king_ = decltype(bot.king_){king_id, king_len, king_pos, round};
        bot.feeder_target_ = TeamSignal{king_id, king_pos, king_len, round, true};
        bot.king_head_estimate_ = king_pos;
        bot.king_cycle_index_ = -1;
        std::vector<unswbc::Tile> tiles;
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
            {
                Position p(trail.front().x + dx, trail.front().y + dy);
                std::optional<unswbc::DragonPart> part;
                if (p == king_pos)
                    part = unswbc::DragonPart(p, king_id, unswbc::Team::A, king_dir, true);
                tiles.emplace_back(p, part, -1, false);
            }
        ct.vision = unswbc::Vision(tiles);
        bot.heads_.push_back(SeenHead{king_id, unswbc::Team::A, king_pos, king_dir, king_len, king_len, true});
        for (auto const &[id, p] : others)
            bot.heads_.push_back(SeenHead{id, unswbc::Team::A, p, Direction::NORTH, 3, 3, true});
        for (Position const &p : trail)
            bot.own_trail_.push_back(p);
        bot.known_body_ = static_cast<int>(trail.size());
    }
};

static void audit_reconstructions()
{
    // r445: A721 (len 13) at (34,22), body east; "king" 562 len 13 at (35,25) heading W.
    // v10.3.3 fed (designated id collected 0/7). Equal length: not defensible.
    {
        std::vector<Position> trail;
        for (int i = 0; i < 13; ++i)
            trail.push_back(Position(34 + std::min(i, 3), 22 - std::max(0, i - 3)));
        AuditDonation f(445, 721, 13, trail, 562, 13, Position(35, 25), Direction::WEST, {{493, Position(32, 22)}});
        check(!f.bot.good_sacrifice_spot(), "audit r445: equal-length donation A721->562 rejected");
    }
    // r402: A676 (len 16) at (31,33) after N,N,W; body along x=32 southwards; king 617
    // len 23 at (31,36) heading N; 643 at (27,36). A longer king 3 cells away on a
    // clear route: still approved. Its 2/8 collection came from the king's route and
    // ally competition afterwards, which the donation test cannot see.
    {
        std::vector<Position> trail{Position(31, 33)};
        for (int i = 1; i < 16; ++i)
            trail.push_back(Position(32, 32 + i));
        AuditDonation f(402, 676, 16, trail, 617, 23, Position(31, 36), Direction::NORTH, {{643, Position(27, 36)}});
        check(f.bot.good_sacrifice_spot(), "audit r402: A676->617 (longer king, clear route) still approved");
    }
}

// ---------------------------------------------------------------------------
// 6. Worker survival after SACRIFICE_ROUND when no useful sacrifice exists.
// ---------------------------------------------------------------------------
static void late_worker_escape()
{
    Fixture f(40, 40, 80, 430);
    f.know_all_open();
    int const L = 14;
    f.ct.length = L;
    f.ct.head.position = Position(20, 20);
    f.bot.retaining_now_ = false;
    f.bot.valuable_now_ = false;
    for (int i = 0; i < L; ++i)
        f.bot.own_trail_.push_back(Position(20 - i, 20));
    f.bot.known_body_ = L;
    for (int i = 0; i < L; ++i)
    {
        int c = f.bot.index(Position(20 - i, 20));
        f.bot.own_idx_[static_cast<std::size_t>(c)] = i;
        f.bot.touched_.push_back(c);
    }
    // No king known at all -> no useful sacrifice.
    std::vector<Candidate> none;
    std::ostringstream out;
    auto *old = std::cout.rdbuf(out.rdbuf());
    bool const split = f.bot.play_split_escape(none);
    std::cout.rdbuf(old);
    check(split, "survival: trapped late worker with no useful sacrifice escapes by split");

    // With a useful sacrifice established (a longer king right here) it keeps feeding.
    DonationFixture g(430, 14, 30, Position(20, 23), 30, false);
    for (int i = 0; i < 14; ++i)
    {
        int c = g.bot.index(Position(20 - i, 20));
        g.bot.own_idx_[static_cast<std::size_t>(c)] = i;
        g.bot.touched_.push_back(c);
    }
    std::ostringstream out2;
    old = std::cout.rdbuf(out2.rdbuf());
    bool const split2 = g.bot.play_split_escape(none);
    std::cout.rdbuf(old);
    check(!split2, "survival: late worker at a useful sacrifice spot does not escape");
}

int main()
{
    cycle_variants_are_hamiltonian();
    claimed_cycle_edges_all_clear();
    closing_edge_kelp_rejected();
    terrain_sharing_is_truthful();
    beacon_round_trip();
    heir_ignores_parent_report_from_birth_round();
    parent_does_not_advertise_old_king_after_escape();
    stale_length_not_renewed_by_sighting();
    donation_checks();
    audit_reconstructions();
    late_worker_escape();
    std::printf("%d failure(s)\n", failures);
    return failures;
}
