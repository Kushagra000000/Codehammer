// v30 dragon: one process per dragon. What it remembers between turns, and the per-turn picture it builds.
#pragma once

#include "comms.hpp"
#include "core.hpp"
#include "mapmem.hpp"
#include "tuning.hpp"

namespace v30 {

// everything about the current turn
struct Turn {
    int rnd = 0, mydir = 0, len = 0, units = 0, tn = 0;
    std::vector<std::string> msgs;
    int hx = 0, hy = 0, hg = 0;
    int wg[49], wp[49], wcd[49];
    int occ[49];                 // 0 free, 1 body of another dragon, 2 head of another dragon, 3 me
    int own_face[49];
    int freeat[49];              // first step of this turn on which the tile may be entered
    bool plain = true;           // no kelp or portal edge in view
    int mv_buf[196], ext_buf[49];
    const int* mv = nullptr;     // mv[i*4+d]: view index, -1 kelp, -2 leaves the view, -3 blind portal
    const int* ext = nullptr;    // 1: the tile has a way out of the view
    std::vector<HeadSeen> heads;
    IdCount seg_n, seg_ring, head_face;
    std::vector<std::pair<int, int>> threading;   // (portal id, dragon id): a body going through a portal
    std::vector<Rival> enemies;
    std::vector<int> friends;
    int n_en = 0, n_fr = 0;
    double tpen[49];
    int treach[49];
    double late_round = 0, guard_round = 0, feed_round = 0, beacon_round = 0, ksplit_round = 0;
    uint64_t relay = 0;
    int kid = -1;
    bool lead = false;
    bool hungry = false, king_hungry = false, call_hungry = false;
    int headadj[49];
    std::vector<std::pair<int, std::vector<int>>> friend_exits;
    int qhead_i = -1;
    bool lane_on = false, nb_hold = false, lane_nosplit = false;
    int lane_dir = 0;
    double qsafe = 0;
};

struct Dragon {
    // ---- who and where
    int id = 0;
    std::string team;
    int tbit = 0;
    int W = 0, H = 0, N = 0, limit = 64;
    bool queen = false;
    MapMemory mem;
    Noise noise;
    int sdiv = 4;                             // free steps per turn = ceil(length / sdiv) (learnt from the engine)

    // ---- my body
    std::vector<int> trail;                   // my segments, head first, exact while I track my own moves
    bool planned = false;
    std::vector<int> plan;                    // the tiles my last move walks over
    long long work = 0;                       // search budget left this turn

    // ---- what I know of others
    std::map<int, std::pair<int, int>> elen;  // enemy id -> (length, round), late game
    IdCount vis_n;
    int friend_max = 0, friend_max_round = -999;
    int last_enemy = -1000;
    std::optional<Sighting> king_seen, king_pos, prey;
    int king_len = 0, king_len_round = -999, king_hungry = -1, king_id = -1, king_i = -1;
    bool lead = false;
    int kid = -1;
    int q_last = -1000;                       // the last round our queen was seen or heard alive
    bool qk = false;                          // this turn the queen is the king (QKING_FROM)

    // ---- clocks
    bool early_on = false;
    double early_e = 0;
    bool quiet = false;
    bool reg_late = false;
    std::vector<int> reg_hist;
    int reg_born = -1;
    double reg_est = -1.0;
    bool reg_on = false;
    uint64_t reg_out = 0;
    long long reg_kelp = 0, reg_edges = 0;
    double beacon_round = cfg::BEACON, ksplit_round = cfg::KING_SPLIT, guard_round_eff = cfg::GUARD;
    uint64_t beacon_out = 0;

    // ---- economy and history
    int cur_rnd = 0;
    int shed_run = 0, shed_last = -10;
    bool seen_pearl = false;
    int my_len = 0;
    int last_eat = -1;
    int food_goal = -1;
    int turns_played = 0;
    int home_x = -1, home_y = -1;
    std::vector<int> banned;                  // tile -> round until which it is not a goal
    std::vector<int> lane_occ_seen;           // tile -> round another dragon was last seen on it

    // ---- sprints
    int spr_r = -10, spr_k = 0, spr_l0 = 0, spr_e = 0;
    std::vector<int> spr_pearls;

    // ---- the opening rush to the centre
    int rush_ok = -1;                         // -1 undecided, 0 no, 1 rushing, 2 over, 3 done
    int rush_home = -1, rush_target = 0;

    // ---- routes
    bool has_route = false;
    std::vector<int> route;
    int route_goal = -1, route_kind = 0, route_round = -999, route_kpos = -1;
    bool route_rush = false, route_king = false, route_part = false;
    double nav_w = 0.5, nav_credit = cfg::NAV_MAX;
    int nav_hold = -1, nav_hold_m = 0;
    int pref = -1, pref2 = -1, pref_round = 0, last_nav = -1, last_goal = -1;
    bool guard_now = false, hunt_now = false, gathering = false;

    // ---- this turn's flags
    bool qs = false, qnr = false, surv_seen = false;

    // ---- scratch
    std::vector<int> gi;                      // tile -> view index (-1: not in view)
    int gi_wg[49];
    bool gi_set = false;
    Marks others, pearls, nav_blocked, mine_set;
    TileInts body_at, ba_scratch;
    std::vector<int> surv_used, room_dist, nav_par, nav_par_keys;
    const Turn* cur = nullptr;                // for the lane helpers
    std::string why;                          // (debug builds) what the last decision was

    Dragon(int did, const std::string& tm, int w, int h, int lim);
    std::string turn(const std::vector<std::string>& L);

    // perception (perceive.hpp)
    bool read_turn(const std::vector<std::string>& L, Turn& t, long long& tiles0, long long& bodies0, long long& nbod,
                   long long& edges0);
    void read_tiles(const std::vector<std::string>& L, long long tiles0, Turn& t);
    void first_look(Turn& t);
    void read_edges(const std::vector<std::string>& L, long long e0, Turn& t);
    void read_bodies(const std::vector<std::string>& L, long long b0, long long nbod, Turn& t);
    void contact_update(Turn& t);
    void set_clock(Turn& t);
    uint64_t hear_beacons(const Turn& t);
    void find_king(Turn& t);
    void track_body(Turn& t);
    void assess_threats(Turn& t);
    int vi(int g) const { return (g >= 0 && g < N) ? gi[(size_t)g] : -1; }

    // tactics (tactics.hpp)
    bool want_split(const Turn& t) const;
    double early_round() const;
    bool try_kill(const Turn& t, std::string& act);
    bool try_feed(const Turn& t, std::string& act);
    bool reverse_split(const Turn& t, std::string& act);
    std::string boxed_in(const Turn& t, int mydir);
    bool escape_split(const Turn& t, std::string& act);
    bool escape_sprint(const Turn& t, const Option& best, double threat_w, int need_depth, int qfatal, std::string& act);
    bool free_sprint(const Turn& t, const Option& best, double threat_w, int need_depth, std::vector<int>& path);
    bool sprint_ok(const std::vector<int>& steps, int length) const;
    std::string emit_move(const std::vector<int>& path, int hg);
    int lane_food(int u, int k, bool* busy, int lim);
    bool lane_pearl(int g, int k);

    // searches (search.hpp)
    int survive(int start, int need, const TileInts& bat, int grow, int length, const Marks* oth, const Marks* prl);
    int room(int start, int need, const TileInts& bat, int grow, const Marks* oth, int best);
    int pocket_mem(int start, int lim, const TileInts& bat, int grow, const Marks* oth, int rnd);
    int tail_room(int tg, int need, const int* occ);

    // navigation (navigate.hpp)
    int dir_to(int a, int b) const;
    int follow(int hg, const int* legal, int rnd);
    int navigate(const Turn& t, std::vector<Option>& scored, bool blind_ok);

    // the decision (decide.hpp)
    std::string decide(const std::vector<std::string>& L);
};

}  // namespace v30
