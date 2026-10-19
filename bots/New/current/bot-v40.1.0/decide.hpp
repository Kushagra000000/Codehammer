// v30 the turn: perceive, then in order - kill, feed, lanes, split, move (scored first steps, navigation when
// nothing in view is worth eating, the queen's escape sprint, the escape split, free sprints).
#pragma once

#include "navigate.hpp"
#include "perceive.hpp"
#include "search.hpp"
#include "tactics.hpp"

namespace v30
{

    inline std::string Dragon::turn(const std::vector<std::string> &L)
    {
        why = "move";
        std::string out = decide(L);
        if (cfg::DEBUG_WHY)
            out = "INDICATOR " + why + "\n" + out;
        turns_played++;
        uint64_t b = beacon_out ? beacon_out : reg_out;
        beacon_out = 0;
        reg_out = 0;
        if (row_dirs && row_out)
        {
            std::string s;
            for (int d = 0; d < 4; d++)
            {
                uint64_t v = (row_dirs >> d) & 1 ? row_out : b;
                if (!v)
                    continue;
                char buf[48];
                snprintf(buf, sizeof buf, "SONAR %c %llu\n", DIR_CH[d], (unsigned long long)v);
                s += buf;
            }
            out = s + out;
        }
        else if (b)
        {
            char buf[128];
            unsigned long long v = (unsigned long long)b;
            snprintf(buf, sizeof buf, "SONAR N %llu\nSONAR E %llu\nSONAR S %llu\nSONAR W %llu\n", v, v, v, v);
            out = std::string(buf) + out;
        }
        row_out = 0;
        row_dirs = 0;
        return out;
    }

#ifdef V41_TRACE
    inline void v41_trace(int id, int rnd, const std::string &why, const std::string &act, int fs, int mh, int qp, int ep)
    {
        static FILE *tf = nullptr;
        if (!tf)
        {
            char nm[128];
            snprintf(nm, sizeof nm, "/tmp/claude-0/v41trace/t_%d.txt", id);
            tf = fopen(nm, "w");
        }
        if (tf)
        {
            fprintf(tf, "r%d %s | %s | farm%d maj%d qpath%d\n", rnd, why.c_str(), act.c_str(), fs, mh, qp);
            fflush(tf);
        }
    }
#endif
    inline std::string Dragon::decide_inner(const std::vector<std::string> &L)
    {
        Turn T;
        Turn &t = T;
        long long tiles0, bodies0, nbod, edges0;
        if (!read_turn(L, t, tiles0, bodies0, nbod, edges0))
            return "MOVE N";
        cur = &t;
        const int rnd = t.rnd, length = t.len, ucount = t.units;
        cur_rnd = rnd;
        row_on = cfg::ROW && (cfg::ROW_AREA_MAX <= 0 || (double)W * H < cfg::ROW_AREA_MAX);
        portal_early_now = cfg::PORTAL_EARLY > 0 && rnd < cfg::PORTAL_EARLY && length <= cfg::PORTAL_EARLY_LEN &&
                           (double)W * H < cfg::PORTAL_EARLY_AREA && !queen; // v40.1.0
        if (rnd == 0 && cfg::OE_NR > 0)
            oe_starter2 = (ucount <= 2 && (id == 2 || id == 3) && length >= 3 && (double)W * H >= cfg::OE_NR_AREA) ? 1 : 0;
        qs = queen && (double)W * H >= cfg::Q_AREA; // the queen's safety rules (big maps)
        surv_seen = qs;
        qnr = qs || (queen && cfg::QNR_SMALL);
        qk = cfg::QKING_FROM > 0 && t.rnd >= cfg::QKING_FROM && (double)W * H >= cfg::QKING_AREA &&
             (queen || t.rnd - q_last <= cfg::QKING_SILENT);

        // what my last sprint cost tells me of the free-step rule
        if (spr_r != -10)
        {
            int r0 = spr_r, k0 = spr_k, l0 = spr_l0, e0 = spr_e;
            spr_r = -10;
            if (r0 == rnd - 1 && k0 >= 2)
            {
                int paid = l0 + e0 - length;
                if (0 < paid && paid < k0 && ceil_div(l0, sdiv) != k0 - paid)
                {
                    int f0 = k0 - paid, bd = -1;
                    for (int d0 = 2; d0 <= 16; d0++)
                        if (ceil_div(l0, d0) == f0 && (bd < 0 || iabs(d0 - sdiv) < iabs(bd - sdiv)))
                            bd = d0;
                    if (bd > 0)
                        sdiv = bd;
                }
            }
        }
        if (home_x < 0)
        {
            home_x = t.hx;
            home_y = t.hy;
        }
        const int farm_len_before = my_len; // (v40.1.0 FARM) my length last turn
        if (length > my_len || last_eat < 0)
            last_eat = rnd; // (a newborn counts as fed)
        my_len = length;
        t.hungry = rnd - last_eat >= cfg::HUNGRY;
        t.king_hungry = rnd - last_eat >= cfg::KING_HUNGRY;
        t.call_hungry = rnd - last_eat >= 1;
        if (gi_set)
            for (int i = 0; i < 49; i++)
                gi[(size_t)gi_wg[i]] = -1;
        for (int i = 0; i < 49; i++)
        {
            gi[(size_t)t.wg[i]] = i;
            gi_wg[i] = t.wg[i];
        }
        gi_set = true;
        const int hg = t.hg;

        read_tiles(L, tiles0, t);
        {
            bool scarce = cfg::SCARCE_T > 0 && tiles_first >= cfg::SCARCE_MIN &&
                          tiles_productive < cfg::SCARCE_T * tiles_first;
            nav_gate_now = scarce ? cfg::SCARCE_GATE : cfg::NAV_GATE;
            unseen_cost_now = scarce ? cfg::SCARCE_UNSEEN : cfg::UNSEEN_COST;
        }
        spr_pearls.clear();
        for (int i = 0; i < 49; i++)
            if (t.wp[i])
                spr_pearls.push_back(t.wg[i]);
        if (rush_ok < 0)
            first_look(t);
        read_edges(L, edges0, t);
        const int *mv = t.mv;
        read_bodies(L, bodies0, nbod, t);
        contact_update(t);
        for (auto &tp : t.threading)
        {
            // the far side stays blocked until the whole of that dragon is through
            int n = t.seg_n.get(tp.second, 0) + 1;
            if (n < 3)
                n = 3;
            auto it = mem.portal_busy.find(tp.first);
            int curv = it == mem.portal_busy.end() ? -1 : it->second;
            if (curv < rnd + n)
                mem.portal_busy[tp.first] = rnd + n;
        }
        set_clock(t);
        if (!t.msgs.empty())
            t.relay = hear_beacons(t);
        if (row_on)
        {
            for (size_t k = 0; k < row_heard.size();)
                if (t.rnd - row_heard[k].second.second > 10)
                    row_heard.erase(row_heard.begin() + (long)k);
                else
                    k++;
            for (auto &m : t.msgs)
            {
                uint64_t v;
                int rid, rl, rf;
                if (!parse_u64(m, v) || !decode_row(v, tbit, rid, rl, rf))
                    continue;
                bool found = false;
                for (auto &h : row_heard)
                    if (h.first == rid)
                    {
                        h.second = {rl, t.rnd};
                        found = true;
                    }
                if (!found)
                    row_heard.push_back({rid, {rl, t.rnd}});
                if ((rf & 1) && (rf & 2))
                    qstuck_round = t.rnd;
            }
        }
        find_king(t);
        const bool big_map = (double)W * H >= cfg::GATHER_AREA;
        if (cfg::GATHER_FEED && big_map && t.feed_round > cfg::GATHER_FEED)
            t.feed_round = cfg::GATHER_FEED;
        track_body(t);
        const int tn = t.tn;
        assess_threats(t);
        const int kid_ = t.kid;
        const bool lead_ = t.lead;
        const std::optional<Sighting> kp = king_pos;

        // ---------------------------------------------------------------- 1. kill
        if (!t.enemies.empty() && ucount >= 2)
        {
            std::string act;
            if (try_kill(t, act))
                return act; // (why set inside)
        }

        // ---------------------------------------------------------------- 1b. feed the king
        {
            bool khc = (king_hungry >= 0 && rnd - king_len_round <= 20) ? (king_hungry & 1) != 0 : t.hungry;
            bool gate_ok = length >= 4 || rnd >= cfg::FEED_LATE || ucount >= limit || khc;
            if (rnd >= t.feed_round && t.enemies.empty() && !t.friends.empty() && length >= 2 && ucount >= 4 && gate_ok)
            {
                std::string act;
                if (try_feed(t, act))
                {
                    why = "feed";
                    return act;
                }
            }
        }

        // tiles another head could step onto before my next turn, and the real exits of every friendly head
        for (int i = 0; i < 49; i++)
            t.headadj[i] = 0;
        for (auto &h : t.heads)
        {
            int hi = h.i;
            if (!h.enemy && (h.id == 0 || h.id == 1))
                t.qhead_i = hi;
            std::vector<int> ex;
            for (int k = hi * 4; k < hi * 4 + 4; k++)
            {
                int x = mv[k];
                if (x >= 0)
                {
                    t.headadj[x] += 1;
                    if (!t.occ[x] && !h.enemy)
                        for (int kk = x * 4; kk < x * 4 + 4; kk++)
                        {
                            int y = mv[kk];
                            if (y <= -2 || (y >= 0 && y != hi && !t.occ[y]))
                            {
                                ex.push_back(x);
                                break;
                            }
                        }
                }
                else if (x == -2)
                {
                    ex.push_back(-2);
                }
            }
            if (!h.enemy)
                t.friend_exits.push_back({hi, ex});
        }

        // ---------------------------------------------------------------- right of way: our queen is boxed in
        if (row_on)
        {
            int my_open = 0;
            for (int k = HEAD * 4; k < HEAD * 4 + 4; k++)
                if (mv[k] != -1)
                    my_open++;
            if (my_open <= 2)           // in a 1-wide passage: my length, at a friend's head
                for (auto &h : t.heads) // next to mine (the beacon goes everywhere else)
                    if (!h.enemy && vman(h.i, HEAD) == 1)
                    {
                        int d = row_dir_to(h.i);
                        if (d >= 0)
                            row_dirs |= 1 << d;
                    }
            if (row_dirs)
                row_out = encode_row(tbit, id, length, queen ? 1 : 0);
        }
        if ((row_on || cfg::QPATH_W > 0) && !queen && t.qhead_i >= 0)
        { // (v40.1.0 QPATH: on every map, not only under ROW)
            const int qh = t.qhead_i;
            bool stuck = rnd - qstuck_round <= 1;
            if (!stuck)
            {
                int open = 0, unknown = 0;
                for (int k = qh * 4; k < qh * 4 + 4; k++)
                {
                    int x = mv[k];
                    if (x == -1)
                        continue;
                    if (x <= -2)
                        unknown++;
                    else if (!t.occ[x] || t.freeat[x] <= 1)
                        open++;
                }
                stuck = open == 0 && unknown == 0;
            }
            if (stuck)
            {
                int jmin = 1 << 20; // my blocking segment nearest my head
                for (int k = qh * 4; k < qh * 4 + 4; k++)
                {
                    int x = mv[k];
                    if (x < 0 || t.occ[x] != 3)
                        continue;
                    for (int j = 0; j < tn && j < length; j++)
                        if (trail[(size_t)j] == t.wg[x])
                        {
                            if (j < jmin)
                                jmin = j;
                            break;
                        }
                }
                int qid = -1;
                for (auto &h : t.heads)
                    if (h.i == qh)
                        qid = h.id;
                int ql = 2; // how many turns can she stall (SPLIT 2 each)?
                if (qid >= 0 && t.seg_n.has(qid) && !t.seg_ring.has(qid))
                    ql = t.seg_n.get(qid, 2);
                for (auto &h : row_heard)
                    if (h.first == qid && rnd - h.second.second <= 2 && h.second.first > ql)
                        ql = h.second.first;
                int can_wait = ql >= 4 ? (ql - 2) / 2 : 0;
                if (jmin < (1 << 20) && length - jmin > 1 + can_wait)
                {
                    std::string act;
                    if (row_die(t, act))
                    {
                        why = "row-free-queen";
                        return act;
                    }
                }
            }
        }

        // ---------------------------------------------------------------- lanes (small maps)
        const bool lane_map = !((double)W * H >= cfg::LANE_AREA);
        t.nb_hold = lane_map && turns_played == 0 && rnd > 0; // a newborn's tail touches its parent
        t.lane_on = !qs && lane_map;
        if (t.lane_on)
        {
            if (lane_occ_seen.empty())
                lane_occ_seen.assign((size_t)N, -1000);
            for (int i = 0; i < 49; i++)
                lane_occ_seen[(size_t)t.wg[i]] = (t.occ[i] == 1 || t.occ[i] == 2) ? rnd : -1000;
            mem.build_lanes();
            const int dh = mem.lane_depth[(size_t)hg];
            if (dh > 0 && tn >= 2)
            {
                int dn = mem.lane_depth[(size_t)trail[1]];
                if (dn == 0 || dh > dn)
                    t.lane_dir = 1;
                else if (dh < dn)
                    t.lane_dir = -1;
            }
            if (t.lane_dir == 1)
            {
                bool busy = false;
                int ahead = lane_food(hg, 0, &busy, 60);
                t.lane_nosplit = true; // walking in: no routine split
                if (length >= 4 && ucount < limit && !t.nb_hold && ahead == 0 && !(cfg::QNOLANE && queen))
                {
                    std::string alt; // nothing left ahead: all but the front two walk out
                    if (reverse_split(t, alt))
                    {
                        why = "lane-out";
                        return alt;
                    }
                }
            }
            else if (tn >= length && length >= 4)
            {
                int tl = trail[(size_t)(length - 1)], tp = trail[(size_t)(length - 2)];
                int dtl = mem.lane_depth[(size_t)tl];
                if (dtl > 0 && dtl > mem.lane_depth[(size_t)tp])
                {
                    // my tail is in a lane facing deeper: a 2-long child born there farms it
                    bool busy = false;
                    int fb = lane_food(tl, 0, &busy, 60);
                    if (!busy && fb >= cfg::LANE_RELAY_F && ucount < limit && !t.nb_hold)
                    {
                        int safe = 0, free = 0;
                        for (int d = 0; d < 4; d++)
                        {
                            int v = mv[d + 96];
                            if (v == -2)
                                safe++;
                            else if (v >= 0 && !t.occ[v])
                            {
                                free++;
                                if (t.headadj[v] == 0)
                                    safe++;
                            }
                        }
                        if (safe > 0 || free >= 2)
                        {
                            why = "lane-relay";
                            return "SPLIT 2";
                        }
                    }
                    if (busy || fb < 2)
                        t.lane_nosplit = true;
                }
            }
        }

        // ---------------------------------------------------------------- v40.1.0 FARM: leaving a lane I ate in
        if (cfg::FARM)
        {
            const bool here_lane = lane_tile(hg);
#ifdef V41_TRACE
            {
                static FILE *lf = nullptr;
                if (!lf)
                {
                    char nm[128];
                    snprintf(nm, sizeof nm, "/tmp/claude-0/v41trace/lane_%d.txt", id);
                    lf = fopen(nm, "w");
                }
                if (lf)
                {
                    const auto &a = mem.adj[(size_t)hg];
                    fprintf(lf, "r%d at(%d,%d) adj %d %d %d %d lane%d in%d eaten%d len%d tn%d\n", rnd, hg % W, hg / W, a[0], a[1], a[2], a[3], (int)here_lane, (int)farm_in, farm_eaten, length, tn);
                    fflush(lf);
                }
            }
#endif
            if (here_lane)
            {
                if (length > farm_len_before)
                    farm_eaten++;
                if (mem.fastbed[(size_t)hg] || mem.fountain[(size_t)hg])
                    farm_fast = true; // a fast bed in this lane
                farm_in = true;
            }
            else if (farm_in)
            {
                farm_in = false;
                bool tail_in = tn >= length && length >= 4 && lane_tile(trail[(size_t)(length - 1)]) &&
                               lane_tile(trail[(size_t)(length - 2)]);
                if ((farm_eaten >= cfg::FARM_MIN_EAT || farm_fast) && tail_in && ucount < limit && !queen && !t.nb_hold)
                {
                    int safe = 0, free = 0; // (as the lane relay) the child must have a way on
                    for (int d = 0; d < 4; d++)
                    {
                        int v = mv[d + 96];
                        if (v == -2)
                            safe++;
                        else if (v >= 0 && !t.occ[v])
                        {
                            free++;
                            if (t.headadj[v] == 0)
                                safe++;
                        }
                    }
                    if (safe > 0 || free >= 2)
                    {
                        farm_eaten = 0;
                        farm_fast = false;
                        farm_splits++;
                        why = "farm-split";
                        return "SPLIT 2";
                    }
                }
                farm_eaten = 0;
                farm_fast = false;
            }
        }

        // ---------------------------------------------------------------- 2. split
        bool qsplit_hold = false;
        if (cfg::QSPLIT_SAFE && queen && !qs)
            for (auto &en : t.enemies)
            {
                int reach = ceil_div(en.est, sdiv) + en.est - 2;
                if (reach < 1)
                    reach = 1;
                if (vman(en.i, HEAD) <= reach)
                    qsplit_hold = true; // standing still here, she could be rammed
            }
        if (length >= cfg::SPLIT_MIN && ucount < limit && !t.lane_nosplit && !t.nb_hold && !qsplit_hold && want_split(t))
        {
            int child = ucount * 2 <= limit ? length / 2 : cfg::BUD;
            if (length - child < 2)
                child = length - 2;
            bool ok = true;
            if (qs && length - child < cfg::Q_KEEP)
            {
                child = length - cfg::Q_KEEP; // the big-map queen keeps Q_KEEP segments
                ok = child >= 2;
            }
            if (ok && tn >= length)
            {
                int ti = vi(trail[(size_t)(length - 1)]);
                if (ti >= 0)
                {
                    ok = false; // the child is born on my tail and moves this round
                    for (int k = ti * 4; k < ti * 4 + 4; k++)
                    {
                        int v = mv[k];
                        if (v == -2 || (v >= 0 && !t.occ[v]))
                            ok = true;
                    }
                }
            }
            if (ok)
            {
                // I stand still this turn: my head needs a way on that nobody can take first
                int safe = 0, free = 0;
                for (int d = 0; d < 4; d++)
                {
                    int v = mv[d + 96];
                    if (v == -2)
                        safe++;
                    else if (v >= 0 && !t.occ[v])
                    {
                        free++;
                        if (t.headadj[v] == 0)
                            safe++;
                    }
                }
                if (safe == 0 && free < 2)
                    ok = false;
            }
            if (ok)
            {
                why = "split";
                return "SPLIT " + std::to_string(child);
            }
        }

        // ---------------------------------------------------------------- 3. move
        std::vector<Option> cands;
        const auto nbh = mem.adj[(size_t)hg];
        for (int d = 0; d < 4; d++)
        {
            int v = mv[d + 96];
            if (v == -1)
                continue;
            int g2 = nbh[(size_t)d];
            if (v >= 0)
            {
                if (t.freeat[v] > 1)
                    continue;
                cands.push_back({0.0, d, v, g2, 0});
            }
            else if (v == -2)
            {
                int bg = body_at.get(g2);
                if (g2 >= 0 && (bg == NONE ? 0 : bg) <= 1 && tn >= length)
                    cands.push_back({0.0, d, -1, g2, 1});
            }
            else if (tn >= length)
            {
                cands.push_back({0.0, d, -1, -1, 2}); // blind portal
            }
        }
        const bool qnp = qs; // the big-map queen never takes a dead-end harvest
        if (cands.empty())
        {
            std::string alt;
            if (row_on)
            {
                if (queen)
                {
                    // the queen stalls (splits 2 off her back) and calls for room on all four sonars
                    row_out = encode_row(tbit, id, length, 3);
                    row_dirs = 15;
                    if (length >= 4 && ucount < limit)
                    {
                        why = "q-stall";
                        return "SPLIT 2";
                    }
                    why = "nocand-boxed";
                    return boxed_in(t, t.mydir);
                }
                bool yield = false, hold = false;
                for (auto &h : t.heads)
                {
                    if (h.enemy || vman(h.i, HEAD) != 1)
                        continue;
                    if (friend_outranks(t, h.id))
                        yield = true;
                    else
                        hold = true;
                }
                if (yield)
                {
                    std::string act;
                    if (row_die(t, act))
                    {
                        why = "row-yield";
                        return act;
                    }
                }
                if (hold && length >= 4 && ucount < limit)
                {
                    why = "row-hold"; // it goes back or dies: wait for it
                    return "SPLIT 2";
                }
            }
            why = "nocand-reverse";
            if (!qnp && reverse_split(t, alt))
                return alt;
            why = "nocand-boxed";
            return boxed_in(t, t.mydir);
        }

        // how fast the other heads in view get to each tile (territory)
        bool has_odist = false;
        int odist[49];
        {
            std::vector<HeadSeen> th;
            bool king_terr = rnd >= t.guard_round && length >= cfg::GUARD_KING && lead_;
            for (auto &h : t.heads)
                if (!king_terr || h.enemy)
                    th.push_back(h);
            if (!t.heads.empty() && !th.empty())
            {
                has_odist = true;
                for (int i = 0; i < 49; i++)
                    odist[i] = -1;
                std::vector<int> q;
                for (auto &h : th)
                {
                    odist[h.i] = 0;
                    q.push_back(h.i);
                }
                for (size_t qi = 0; qi < q.size(); qi++)
                {
                    int u = q[qi];
                    for (int k = u * 4; k < u * 4 + 4; k++)
                    {
                        int x = mv[k];
                        if (x >= 0 && odist[x] < 0 && !t.occ[x])
                        {
                            odist[x] = odist[u] + 1;
                            q.push_back(x);
                        }
                    }
                }
            }
        }

        // food: pearls, and tiles that spawn one within 12 rounds; dead-end pockets in view are bait unless I can
        // harvest them and split out
        const bool pocket_run = ucount < limit - 1 && !qnp && !(queen && cfg::QNOPOCKET_RUN);
        const bool fount = length <= cfg::FOUNT_MAXLEN;
        const double terr_food = length <= cfg::FOUNT_MAXLEN ? cfg::TERR_FOOD : 0.0;
        bool prun = false;
        bool has_pocket = false;
        int pocket[49];
        if (!t.plain)
        {
            int deg[49];
            for (int i = 0; i < 49; i++)
            {
                int n = 0;
                for (int k = i * 4; k < i * 4 + 4; k++)
                {
                    int x = mv[k];
                    if (x >= 0)
                        n += 1;
                    else if (x <= -2)
                        n += 2;
                }
                deg[i] = n;
                pocket[i] = 0;
            }
            has_pocket = true;
            std::vector<int> stack;
            for (int i = 0; i < 49; i++)
                if (deg[i] <= 1)
                    stack.push_back(i);
            while (!stack.empty())
            {
                int i = stack.back();
                stack.pop_back();
                if (pocket[i])
                    continue;
                pocket[i] = 1;
                for (int k = i * 4; k < i * 4 + 4; k++)
                {
                    int x = mv[k];
                    if (x >= 0 && !pocket[x])
                    {
                        deg[x] -= 1;
                        if (deg[x] <= 1)
                            stack.push_back(x);
                    }
                }
            }
            int npk = 0;
            for (int i = 0; i < 49; i++)
                if (pocket[i] && (t.wp[i] || (fount && 0 < t.wcd[i] && t.wcd[i] <= cfg::FOUNT_CD)))
                    npk++;
            if (!(pocket_run && npk >= cfg::POCKET_MIN && length + npk >= 4))
            {
                for (int i = 0; i < 49; i++)
                    if (pocket[i] && (t.wp[i] || t.wcd[i] > 0) && !cfg::KEEP_POCKET_MEM)
                        mem.drop_due(t.wg[i]);
            }
            else
            {
                has_pocket = false;
                prun = fount;
            }
        }
        std::vector<int> targets;
        for (int i = 0; i < 49; i++)
        {
            if (!((t.wp[i] || (0 < t.wcd[i] && t.wcd[i] <= 12)) && !t.occ[i]))
                continue;
            int i4 = i * 4;
            if ((mv[i4] == -1) + (mv[i4 + 1] == -1) + (mv[i4 + 2] == -1) + (mv[i4 + 3] == -1) >= 3 && !prun)
            {
                if (!cfg::KEEP_POCKET_MEM)
                    mem.drop_due(t.wg[i]); // a pearl in a dead end is bait, not food
                continue;
            }
            if (has_pocket && pocket[i])
                continue;
            if (banned[(size_t)t.wg[i]] > rnd)
                continue;
            targets.push_back(i);
        }
        std::vector<int> rivals;
        for (auto &h : t.heads)
            rivals.push_back(h.i);
        pearls.clear();
        for (int i = 0; i < 49; i++)
            if (t.wp[i])
                pearls.add(t.wg[i]);
        int need_depth = length + 4;
        if (need_depth < 10)
            need_depth = 10;
        else if (need_depth > cfg::SURV_CAP)
            need_depth = cfg::SURV_CAP;
        if (cfg::QNEED > need_depth && queen && !qs)
            need_depth = cfg::QNEED < cfg::SURV_CAP ? cfg::QNEED : cfg::SURV_CAP;

        // roles late in the game: guards near the king, the king's food is the king's
        int king_i_ = -1;
        int gmax = cfg::GUARD_MAXLEN;
        if (kid_ >= 0 && !lead_)
            gmax = 999;
        if (rnd >= t.guard_round && length <= gmax && !t.friends.empty())
        {
            if (kid_ >= 0)
            {
                if (!lead_)
                    king_i_ = king_i;
            }
            else
            {
                int kb2 = cfg::GUARD_KING - 1;
                for (auto &h : t.heads)
                    if (!h.enemy && t.seg_n.get(h.id, 0) > kb2)
                    {
                        kb2 = t.seg_n.get(h.id, 0);
                        king_i_ = h.i;
                    }
            }
        }
        guard_now = rnd >= t.guard_round && length <= gmax && king_i_ < 0 && !(kid_ >= 0 && lead_);
        if (guard_now && kid_ >= 0 && mem.wrap_dist(kp->tile, hg) > 499 - rnd)
            guard_now = false; // too far to matter
        // big-map gather: everyone who knows the king (and is not it) heads there, food or not
        const bool gather = cfg::GATHER_BIG && big_map && rnd >= cfg::GATHER_BIG && kid_ >= 0 && !lead_ && king_i_ < 0 &&
                            king_seen && rnd - king_seen->round <= cfg::GATHER_MEM;
        if (gather)
            guard_now = true;
        gathering = gather;
        int defer_i = -1;
        if (rnd >= t.feed_round && !t.friends.empty())
        {
            if (kid_ >= 0)
            {
                if (!lead_)
                    defer_i = king_i;
            }
            else
            {
                int kb2 = length;
                for (auto &h : t.heads)
                    if (!h.enemy && t.seg_n.get(h.id, 0) > kb2)
                    {
                        kb2 = t.seg_n.get(h.id, 0);
                        defer_i = h.i;
                    }
            }
        }
        double king_stay = 0.0;
        bool i_am_king = false;
        const bool is_king = qk ? (rnd >= t.guard_round && length >= cfg::GUARD_KING && lead_)
                                : (rnd >= t.guard_round && length >= cfg::GUARD_KING && length >= friend_max && (kid_ < 0 || lead_));
        if (is_king && !t.friends.empty() && (t.king_hungry || rnd >= cfg::FEED_LATE))
        {
            i_am_king = true;
            king_stay = cfg::KING_STAY_W;
        }
        if (rnd >= t.beacon_round || qk)
        {
            if (qk ? (queen && length >= 2) : (length >= cfg::BEACON_LEN && lead_))
            {
                long long hbits = (t.call_hungry ? 1 : 0) | (t.king_hungry ? 2 : 0);
                beacon_out = encode_beacon(W, hg, length, 0, 0, hbits, id, tbit); // I am the king as far as I know
            }
            else
            {
                int big = -1, bseg = cfg::HUNT_SEG - 1;
                if (rnd >= cfg::HUNT_ROUND)
                    for (auto &h : t.heads)
                        if (h.enemy && t.seg_n.get(h.id, 0) > bseg)
                        {
                            bseg = t.seg_n.get(h.id, 0);
                            big = h.i;
                        }
                const Rival *qen = nullptr; // v40 QHUNT: their queen's head in view, late: say where
                if (cfg::QHUNT && rnd >= cfg::Q_HUNT_FROM)
                    for (auto &en : t.enemies)
                        if (en.id == 0 || en.id == 1)
                            qen = &en;
                if (qen)
                    beacon_out = encode_beacon(W, t.wg[qen->i], qen->len > 2 ? qen->len : 2, 0, 1, 0, 1, tbit);
                else if (big >= 0)
                    beacon_out = encode_beacon(W, t.wg[big], bseg, 0, 1, 0, 0, tbit); // a long enemy here
                else if (t.relay)
                    beacon_out = t.relay;
            }
        }
        hunt_now = rnd >= cfg::HUNT_ROUND && length <= cfg::HUNT_MAXLEN && prey && rnd - prey->round <= cfg::HUNT_MEM &&
                   t.enemies.empty();
        // v40 QHUNT (the top team's late queen hunt): short dragons go for their queen where she was seen lately, while our
        // king is at least as long as the longest enemy about (her death then wins the tiebreak)
        if (cfg::QHUNT && !queen && rnd >= cfg::Q_HUNT_FROM && qprey)
        {
            if (rnd - qprey->round > cfg::Q_HUNT_MEM)
                qprey.reset();
            else if (gi[(size_t)qprey->tile] >= 0 && qprey->round < rnd)
                qprey.reset(); // her spot is in view and she is not there
            else if (length <= cfg::Q_HUNT_MAXLEN && qprey->tile != hg &&
                     (!cfg::Q_HUNT_LEAD || (rnd - king_len_round <= 30 && king_len >= elong)))
            {
                prey = qprey;
                hunt_now = true;
            }
        }

        // ---------------------------------------------------------------- scoring the first steps
        double food_best = 0.0;
        std::vector<Option> scored;
        work = cfg::WORK;
        const double hunt_w = (ucount >= 2 && length <= cfg::FIGHT_MAX_LEN) ? cfg::HUNT_W : 0.0;
        double space_w = length >= 6 ? cfg::SPACE_W * (length < 30 ? length : 30) / 10.0 : 0.0;
        if (is_king && t.enemies.empty())
            space_w *= cfg::KING_SPACE;
        double soft_w = cfg::SOFT_W;
        if (qs)
            soft_w *= cfg::Q_JAM;
        const double qexit = queen ? 0.0 : cfg::Q_EXIT;
        double threat_w = cfg::THREAT_W;
        const double pocket_f = 1.0 + cfg::POCKET_L2 * length * length;
        if (rnd >= cfg::BANK && length >= cfg::BANK_LEN)
        {
            soft_w *= cfg::BANK_SAFETY;
            threat_w *= cfg::BANK_SAFETY;
        }
        int best_t[4] = {-1, -1, -1, -1};
        int surv_of[4] = {NONE, NONE, NONE, NONE};
        const bool has_ptiles = mem.portal_tiles > 0;
        for (auto &cd : cands)
        {
            const int d = cd.d, v = cd.v, g2 = cd.g, kind = cd.kind;
            double s = 0.0, food = 0.0, cbest_fv = 0.0;
            int cbest_t = -1;
            if (qs && home_x >= 0)
            {
                // the big-map queen stays near where she started
                int dxh = std::abs(g2 % W - home_x);
                if (dxh > W - dxh)
                    dxh = W - dxh;
                int dyh = std::abs(g2 / W - home_y);
                if (dyh > H - dyh)
                    dyh = H - dyh;
                if (dxh + dyh > cfg::Q_HOME)
                    s -= cfg::Q_HOME_W * (dxh + dyh - cfg::Q_HOME);
            }
            if (kind == 0)
            {
                const int grow = t.wp[v];
                if (cfg::Q_TUNNEL > 0 && queen && (double)W * H < cfg::Q_TUNNEL_AREA)
                {
                    // a 1-wide corridor from v (tiles with one way on) with food in it that runs out of sight: bait
                    int prev = HEAD, cur = v, tl = 0;
                    bool blind_end = false, cfood = t.wp[v] || (0 < t.wcd[v] && t.wcd[v] <= 12);
                    for (int st = 0; st < 12; st++)
                    {
                        int exits = 0, nxt = -1;
                        bool unk = false;
                        for (int k = cur * 4; k < cur * 4 + 4; k++)
                        {
                            int x = mv[k];
                            if (x == -1 || x == prev)
                                continue;
                            exits++;
                            if (x < 0)
                                unk = true;
                            else
                                nxt = x;
                        }
                        if (exits != 1)
                            break;
                        tl++;
                        if (unk)
                        {
                            blind_end = true;
                            break;
                        }
                        prev = cur;
                        cur = nxt;
                        if (t.wp[cur] || (0 < t.wcd[cur] && t.wcd[cur] <= 12))
                            cfood = true;
                    }
                    if ((blind_end || tl >= cfg::Q_TUNNEL_MIN) && cfood)
                        s -= cfg::Q_TUNNEL;
                }
                if (t.lane_on && g2 >= 0 && mem.lane_depth[(size_t)g2] > mem.lane_depth[(size_t)hg] && queen &&
                    (mem.lane_depth[(size_t)hg] == 0 || cfg::QNOLANE))
                    s -= 1000.0; // the queen never steps into a lane
                // distances from the tile I would step on (my body frees up as I go)
                int dist[49], q[49], qn = 0;
                for (int i = 0; i < 49; i++)
                    dist[i] = -1;
                dist[v] = 0;
                q[qn++] = v;
                const int off = 1 - grow;
                for (int qi = 0; qi < qn; qi++)
                {
                    int u = q[qi];
                    int nd = dist[u] + 1;
                    for (int k = u * 4; k < u * 4 + 4; k++)
                    {
                        int x = mv[k];
                        if (x >= 0 && dist[x] < 0 && t.freeat[x] <= nd + off)
                        {
                            dist[x] = nd;
                            q[qn++] = x;
                        }
                    }
                }
                // food: 1 / (turns until I eat it)^2, less if another head is nearer
                for (int tg : targets)
                {
                    int dt = dist[tg];
                    if (dt < 0)
                        continue;
                    int arr = dt + 1;
                    if (!t.wp[tg])
                    {
                        if (tg == v)
                            continue;
                        int c = t.wcd[tg] + 1;
                        if (c > arr)
                            arr = c;
                    }
                    double val = 1.0;
                    for (int r : rivals)
                        if (vman(r, tg) < arr)
                        {
                            val = 0.3;
                            break;
                        }
                    if (king_i_ >= 0 && vman(king_i_, tg) <= 3)
                        val = cfg::GUARD_FOOD;
                    else if (defer_i >= 0 && vman(defer_i, tg) <= 3)
                        val = cfg::GUARD_FOOD;
                    double fv = val / (double)(arr * arr);
                    food += fv;
                    if (fv > cbest_fv)
                    {
                        cbest_fv = fv;
                        cbest_t = tg;
                    }
                }
                if (!grow && 1 <= t.wcd[v] && t.wcd[v] <= length + 1)
                    food -= 0.4; // it fires while I am on it
                if (hunt_w != 0.0)
                    for (auto &en : t.enemies)
                    {
                        if (en.len <= length)
                            continue;
                        int bd = 99;
                        for (int k = en.i * 4; k < en.i * 4 + 4; k++)
                        {
                            int x = mv[k];
                            if (x >= 0 && 0 <= dist[x] && dist[x] < bd)
                                bd = dist[x];
                        }
                        if (bd < 99)
                        {
                            int gain = en.len - length;
                            if (gain > 6)
                                gain = 6;
                            s += hunt_w * gain / (bd + 2.0);
                        }
                    }
                if (has_ptiles && mem.has_portal_tile(g2) && length >= cfg::PORTAL_CLEAR_LEN)
                    s -= cfg::PORTAL_CLEAR_LONG;
                // long dragons need elbow room
                if (space_w != 0.0)
                {
                    int sp = 0;
                    for (int qi = 0; qi < qn; qi++)
                        if (t.freeat[q[qi]] == 0)
                            sp += t.ext[q[qi]] ? 3 : 1;
                    if (sp < 60)
                        s -= space_w * (60 - sp) / 60.0;
                }
                // territory: tiles I reach strictly before any other head in view
                double terr_pen = 0.0;
                if (has_odist)
                {
                    int terr = 0, tfood = 0;
                    for (int qi = 0; qi < qn; qi++)
                    {
                        int x = q[qi];
                        int od = odist[x];
                        if (od < 0 || dist[x] < od)
                        {
                            terr += t.ext[x] ? 3 : 1;
                            if (terr_food != 0.0 && (t.wp[x] || (0 < t.wcd[x] && t.wcd[x] <= cfg::FOUNT_CD)))
                                tfood++;
                        }
                    }
                    int need = 2 * length + 4;
                    if (need > 30)
                        need = 30;
                    if (terr < need)
                    {
                        terr_pen = cfg::TERR_W * (need - terr) / need;
                        if (tfood)
                        {
                            terr_pen -= terr_food * tfood;
                            if (terr_pen < 0.0)
                                terr_pen = 0.0;
                        }
                        s -= terr_pen;
                    }
                }
                // can I keep moving from there? First with every other dragon frozen; if not, with only kelp and me
                int got = survive(g2, need_depth, body_at, grow, length, &others, &pearls);
                surv_of[d] = got;
                const bool lane_commit = t.lane_on && t.lane_dir == 1 && g2 >= 0 && !(cfg::QNOLANE && queen) &&
                                         mem.lane_depth[(size_t)g2] > mem.lane_depth[(size_t)hg];
                if (lane_commit)
                {
                    // walking into a lane: the lane split gets me out
                }
                else if (got < need_depth)
                {
                    int hard = got;
                    if (!others.empty())
                        hard = survive(g2, need_depth, body_at, grow, length, nullptr, &pearls);
                    surv_of[d] = hard;
                    if (hard < need_depth)
                    {
                        // a real dead end: with food in it and a split to come out with, it is a harvest run
                        double pk = 0;
                        if (pocket_run)
                        {
                            int lim = hard;
                            for (int tg : targets)
                            {
                                int dt = dist[tg];
                                if (0 <= dt && dt < lim && (t.wp[tg] || (fount && 0 < t.wcd[tg] && t.wcd[tg] <= dt + 1)))
                                    pk += 1;
                            }
                            if (pk < cfg::FOUNT_PKMIN)
                                pk += pocket_mem(g2, lim, body_at, grow, &others, rnd);
                        }
                        if (pk >= cfg::POCKET_MIN && length + pk >= 4)
                        {
                            s -= cfg::POCKET_RUN_COST;
                            if (fount && pk >= cfg::FOUNT_PKMIN)
                                s += cfg::FOUNT_W * pk + terr_pen;
                        }
                        else
                        {
                            s -= 1000.0 * (1.0 - (double)hard / need_depth);
                            if (grow)
                                banned[(size_t)g2] = rnd + cfg::GOAL_BAN;
                        }
                    }
                    else
                    {
                        s -= soft_w * (1.0 - (double)got / need_depth) * pocket_f; // a jam that may clear
                    }
                }
                // exits that no other head can take from me
                int safe = 0;
                for (int k = v * 4; k < v * 4 + 4; k++)
                {
                    int x = mv[k];
                    if (x <= -2)
                        safe++;
                    else if (x >= 0 && x != HEAD && t.freeat[x] <= 2 && t.headadj[x] == 0)
                        safe++;
                }
                if (safe == 0)
                    s -= cfg::SAFE0;
                // never take a friend's last exit; keep out of the queen's way
                for (auto &fe : t.friend_exits)
                {
                    auto &ex = fe.second;
                    if (std::find(ex.begin(), ex.end(), v) == ex.end())
                        continue;
                    int n = (int)ex.size();
                    if (n == 1)
                        s -= 60.0;
                    else if (qexit > 0 && fe.first == t.qhead_i)
                        s -= qexit / (n - 1);
                    else if (n == 2)
                        s -= 0.8;
                }
                if (row_on && !queen)
                {
                    // not onto the 1-wide passage ahead of a friend with priority, nor next to its head there
                    for (auto &h : t.heads)
                    {
                        if (h.enemy || !friend_outranks(t, h.id))
                            continue;
                        int f = t.head_face.get(h.id, -1);
                        bool hit = false;
                        if (f >= 0)
                        {
                            int prev = h.i, u = mv[h.i * 4 + f];
                            for (int s2 = 0; s2 < cfg::ROW_AHEAD && u >= 0; s2++)
                            {
                                if (u == v)
                                {
                                    hit = true;
                                    break;
                                }
                                int nopen = 0, nxt = -1;
                                for (int k = u * 4; k < u * 4 + 4; k++)
                                {
                                    int x = mv[k];
                                    if (x == -1)
                                        continue;
                                    nopen++;
                                    if (x >= 0 && x != prev)
                                        nxt = x;
                                }
                                if (nopen != 2)
                                    break;
                                prev = u;
                                u = nxt;
                            }
                        }
                        if (hit)
                            s -= cfg::ROW_W;
                        else if (vman(h.i, v) == 1)
                        {
                            int nopen = 0;
                            for (int k = v * 4; k < v * 4 + 4; k++)
                                if (mv[k] != -1)
                                    nopen++;
                            if (nopen <= 2)
                                s -= 0.5 * cfg::ROW_W;
                        }
                    }
                }
                // v40.1.0 QPATH: our queen moved before me; her way ahead is hers
                if (cfg::QPATH_W > 0 && !queen && t.qhead_i >= 0 && v >= 0)
                {
                    int qf = -1;
                    for (auto &h : t.heads)
                        if (h.i == t.qhead_i)
                            qf = t.head_face.get(h.id, -1);
                    if (qf >= 0)
                    {
                        int a1 = mv[t.qhead_i * 4 + qf];
                        int a2 = a1 >= 0 ? mv[a1 * 4 + qf] : -1;
                        int sl = mv[t.qhead_i * 4 + ((qf + 1) & 3)], sr = mv[t.qhead_i * 4 + ((qf + 3) & 3)];
                        if (v == a1)
                        {
                            s -= cfg::QPATH_W;
                            qpath_hits++;
                        }
                        else if (v == a2 || v == sl || v == sr)
                        {
                            s -= cfg::QPATH_W / 2;
                            qpath_hits++;
                        }
                    }
                }
                // an enemy that could ram me there
                if (t.tpen[v] != 0.0)
                    s -= threat_w * t.tpen[v];
                if (cfg::QFLEE_W > 0 && queen && !qs && !t.enemies.empty())
                {
                    int before = 99, after = 99;
                    for (auto &en : t.enemies)
                    {
                        int b0 = vman(en.i, HEAD), a0 = vman(en.i, v);
                        if (b0 < before)
                            before = b0;
                        if (a0 < after)
                            after = a0;
                    }
                    s += cfg::QFLEE_W * (after - before);
                    if (cfg::QFLEE_CROWD > 0)
                    {
                        int nb = 0, na = 0;
                        for (auto &en : t.enemies)
                        {
                            if (vman(en.i, HEAD) <= 3)
                                nb++;
                            if (vman(en.i, v) <= 3)
                                na++;
                        }
                        s -= cfg::QFLEE_CROWD * (na - nb);
                    }
                }
                // spread out (but stay with the king)
                for (int fi : t.friends)
                {
                    int md = vman(fi, v);
                    if (fi == king_i_)
                    {
                        if (md <= 1)
                            s -= 0.3;
                        else
                            s += cfg::GUARD_W / md;
                        continue;
                    }
                    if (king_stay != 0.0)
                    {
                        if (md <= 1)
                            s -= 0.3;
                        else
                            s += king_stay / md;
                        continue;
                    }
                    if (md <= 1)
                        s -= 0.3;
                    else if (md == 2)
                        s -= 0.15;
                    else if (md == 3)
                        s -= 0.05;
                    else if (md == 4)
                        s -= 0.02;
                }
            }
            else
            {
                // the far side of a portal is out of sight and the landing tile may be taken
                auto it = mem.portal_busy.find(mem.portal_id(hg, d));
                int busy = it == mem.portal_busy.end() ? -1 : it->second;
                if (busy >= rnd || ucount < 2)
                    s -= 50.0;
                else if (kind == 1 && survive(g2, need_depth, body_at, 0, length, nullptr, &pearls) < need_depth)
                    s -= 900.0; // what I remember of the far side is a dead end
                else if (portal_early_now)
                    s -= 0.15; // v40.1.0: early on a small map, go
                else if (length <= 3)
                    s -= kind == 1 ? 0.15 : 0.3;
                else if (length <= 6)
                    s -= 1.0;
                else
                    s -= 6.0;
                if (cfg::QPORTAL_PEN > 0 && queen && !qs)
                    s -= kind == 1 ? cfg::QPORTAL_PEN : 2.0 * cfg::QPORTAL_PEN;
                if (cfg::OWPORT_W > 0 && kind == 1 && g2 >= 0 && g2 < hg)
                    s -= cfg::OWPORT_W;
                if (cfg::PRISK_W > 0)
                {
                    double p = rnd <= 40    ? cfg::PRISK_EARLY
                               : rnd >= 150 ? cfg::PRISK_LATE
                                            : cfg::PRISK_EARLY + (cfg::PRISK_LATE - cfg::PRISK_EARLY) * (rnd - 40) / 110.0;
                    s -= cfg::PRISK_W * p * (length + cfg::PRISK_BASE);
                }
            }
            if (cfg::OWP > 0 && kind == 0 && (queen || !cfg::OWP_QUEEN_ONLY))
            {
                // a 1-wide passage (kelp on both sides of v): our dragons use it one way only, so two of ours never meet in it
                if (mv[v * 4 + ((d + 1) & 3)] == -1 && mv[v * 4 + ((d + 3) & 3)] == -1)
                {
                    int cdir = (d == 0 || d == 2) ? (((g2 % W) % 2 == 0) ? 0 : 2) : (((g2 / W) % 2 == 0) ? 1 : 3);
                    if (d != cdir)
                        s -= cfg::OWP;
                }
            }
            if (cfg::CY > 0 && kind == 0 && (queen || !cfg::CY_QUEEN_ONLY))
            {
                // a 1-wide passage ahead (kelp on both sides of v): a friend's head coming down it towards me is a jam
                const int dl = (d + 1) & 3, dr = (d + 3) & 3;
                if (mv[v * 4 + dl] == -1 && mv[v * 4 + dr] == -1)
                {
                    int u = v;
                    for (int st = 0; st < 3; st++)
                    {
                        int x = mv[u * 4 + d];
                        if (x < 0)
                            break;
                        if (t.occ[x])
                        {
                            if (t.occ[x] == 2)
                                for (auto &h : t.heads)
                                    if (h.i == x && !h.enemy && t.head_face.get(h.id, -1) == BACK[d])
                                        s -= cfg::CY;
                            break;
                        }
                        if (mv[x * 4 + dl] != -1 || mv[x * 4 + dr] != -1)
                            break; // the passage opens: room to pass
                        u = x;
                    }
                }
            }
            if (d == t.mydir)
                s += 0.02;
            s += food;
            if (food > food_best)
                food_best = food;
            if (cfg::QPORTAL_HARD && queen && (kind != 0 || mem.portal_side(hg, d)) && s > -950.0)
                s = -950.0; // v40.1.0: her portal step only when every other step is certain death (-1000) and no split or stall is left
            scored.push_back({s, d, v, g2, kind});
            if (cbest_t >= 0)
                best_t[d] = t.wg[cbest_t];
        }

        // v40.1.0 MAJ_HUNT: a free worker in a local majority goes for the nearest enemy head
        if (cfg::MAJ_HUNT && !queen && !qnr && length <= cfg::MAJ_MAXLEN && food_best < nav_gate_now && !i_am_king &&
            !t.enemies.empty() && t.n_fr + 1 > t.n_en + cfg::MAJ_MARGIN)
        { // (n_fr: the other friends in view)
            const Rival *near = nullptr;
            int nd = 99;
            for (auto &en : t.enemies)
            {
                int d0 = vman(en.i, HEAD);
                if (d0 < nd)
                {
                    nd = d0;
                    near = &en;
                }
            }
            if (near && near->i != HEAD)
            {
                prey = Sighting{t.wg[near->i], rnd, near->len};
                hunt_now = true;
                maj_hunts++;
                why = "maj-hunt";
            }
        }
        int nav_dir = -9;
        if ((food_best < nav_gate_now || gather) && !i_am_king)
        {
            // nothing worth eating in sight: head for remembered pearls / unseen water
            nav_w = 0.5;
            nav_hold = -1;
            int nd = navigate(t, scored, (length <= 3 || portal_early_now) && ucount >= 2 && !(cfg::QPORTAL_HARD && queen)); // v40.1.0
            last_nav = nd;
            nav_dir = nd;
            if (gather && nav_w < cfg::GATHER_W)
                nav_w = cfg::GATHER_W;
            for (auto &sc : scored)
            {
                if (sc.d != nd)
                    continue;
                sc.s += nav_w;
                if (sc.s < -500.0 && last_goal >= 0)
                {
                    // the way to my goal starts with a fatal step: not worth planning for
                    banned[(size_t)last_goal] = rnd + cfg::GOAL_BAN;
                    has_route = false;
                    route.clear();
                }
            }
        }
        const Option *best = nullptr;
        double best_s = -1e18;
        for (auto &sc : scored)
        {
            double s = sc.s + noise.next() * 0.01;
            if (s > best_s)
            {
                best_s = s;
                best = &sc;
            }
        }
        if ((qs || (queen && cfg::QESC_SMALL && t.qsafe > 0)) && !t.enemies.empty() && best != nullptr && tn >= length &&
            (best_s <= -500.0 || (best->v >= 0 && t.treach[best->v])))
        {
            // the queen's best step ends within a rammer's reach (or every step looks fatal): sprint out of it
            std::string alt;
            if (escape_sprint(t, *best, threat_w, need_depth, (best_s <= -500.0 || best->v < 0) ? 1 : 0, alt))
            {
                why = "q-sprint";
                return alt;
            }
        }
        if (length >= cfg::ESC_LEN && !t.enemies.empty() && ucount < limit && best_s > -500.0 && !qnp)
        {
            // hunted: split off everything but two segments and the body walks away from the tail end
            double tmin = 1e9;
            for (auto &sc : scored)
                if (sc.s > -500.0 && sc.v >= 0 && t.tpen[sc.v] < tmin)
                    tmin = t.tpen[sc.v];
            if (tmin * threat_w >= cfg::ESC_PEN)
            {
                std::string alt;
                if (escape_split(t, alt))
                {
                    why = "esc-split";
                    return alt;
                }
            }
        }
        if (best_s < -500.0)
        {
            // every move looks fatal
            std::string alt;
            bool got_alt = !qnp && reverse_split(t, alt);
            if (!got_alt)
            {
                alt = boxed_in(t, -1);
                got_alt = !alt.empty();
            }
            if (got_alt)
            {
                why = "fatal-split";
                return alt;
            }
            why = "fatal-move";
        }
        food_goal = best_t[best->d];
        if (cfg::DEBUG_WHY)
        {
            char buf[64];
            snprintf(buf, sizeof buf, "%s s=%.2f", best->d == nav_dir ? "nav" : "food", best_s);
            why = buf;
        }
        if (best->v >= 0 && best_s > -500.0 && tn >= length && (!qs || (cfg::Q_KING_SPRINT && qk)))
        { // v31.0.0: the crowned queen too
            std::vector<int> path;
            if (free_sprint(t, *best, threat_w, need_depth, path))
            {
                why = "fs";
                return emit_move(path, hg);
            }
        }
        return emit_move({best->d}, hg);
    }

} // namespace v30

namespace v30
{
    inline std::string Dragon::decide(const std::vector<std::string> &L)
    {
        std::string act = decide_inner(L);
#ifdef V41_TRACE
        v41_trace(id, cur_rnd, why, act, farm_splits, maj_hunts, qpath_hits, early_portals);
#endif
        return act;
    }
} // namespace v30