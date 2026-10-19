// v30 tactics: rams and traps, feeding the king, the split rules and the emergency splits, sprints.
#pragma once

#include "dragon.hpp"

namespace v30
{

    inline bool Dragon::want_split(const Turn &t) const
    {
        if (qs && cfg::Q_SPLIT <= t.units)
            return false; // the big-map queen keeps her length
        int stop = reg_late ? cfg::REG_STOP : cfg::STOP_SPLIT;
        if (t.rnd >= stop)
            return false;
        int klen = reg_late ? cfg::REG_KLEN : cfg::BEACON_LEN;
        if (t.ksplit_round != 0 && t.rnd >= t.ksplit_round && (lead || (cfg::QK_KEEP_LONG && lead_long)) && t.len >= klen)
            return false; // the king keeps all
        if (t.units >= limit - cfg::CAP_MARGIN && t.len >= friend_max && t.rnd >= cfg::CAP_ROUND)
            return false;
        int fm = friend_max;
        if (king_len > fm && t.rnd - king_len_round <= 20)
            fm = king_len;
        if ((t.rnd >= t.late_round || (cfg::LATE_KEEP > 0 && t.rnd >= cfg::LATE_KEEP)) && 2 * t.len >= fm)
            return false;
        return true;
    }

    inline std::string Dragon::emit_move(const std::vector<int> &path, int hg)
    {
        std::vector<int> pl;
        bool ok = true;
        int g = hg;
        for (int d : path)
        {
            g = mem.adj[(size_t)g][(size_t)d];
            if (g < 0)
            {
                ok = false;
                break;
            }
            pl.push_back(g);
        }
        planned = ok;
        plan = ok ? pl : std::vector<int>();
        if (path.size() >= 2 && ok)
        {
            // remember the sprint, to learn the engine's free-step rule from what it costs
            int e = 0;
            std::vector<int> done;
            for (int x : pl)
                if (std::find(done.begin(), done.end(), x) == done.end())
                {
                    done.push_back(x);
                    if (std::find(spr_pearls.begin(), spr_pearls.end(), x) != spr_pearls.end())
                        e++;
                }
            spr_r = cur_rnd;
            spr_k = (int)path.size();
            spr_l0 = my_len;
            spr_e = e;
        }
        std::string s = "MOVE ";
        for (int d : path)
            s += DIR_CH[d];
        return s;
    }

    // rams: the enemy queen at any price; a free kill on a head with one way out; a head-on trade that pays
    inline bool Dragon::try_kill(const Turn &t, std::string &act)
    {
        const int *mv = t.mv;
        int hdist[49], hpar[49];
        for (int i = 0; i < 49; i++)
        {
            hdist[i] = -1;
            hpar[i] = -1;
        }
        hdist[HEAD] = 0;
        int q[49], qn = 0;
        q[qn++] = HEAD;
        int budget = ceil_div(t.len, sdiv) + t.len - 2; // free steps, then a segment a step
        for (int qi = 0; qi < qn; qi++)
        {
            int u = q[qi];
            int nd = hdist[u] + 1;
            if (nd >= budget)
                break;
            for (int k = u * 4; k < u * 4 + 4; k++)
            {
                int v = mv[k];
                if (v >= 0 && hdist[v] < 0 && t.freeat[v] <= nd)
                {
                    hdist[v] = nd;
                    hpar[v] = k;
                    q[qn++] = v;
                }
            }
        }
        auto path_to = [&](int k)
        {
            std::vector<int> path{k % 4};
            int u = k / 4;
            while (u != HEAD)
            {
                int kk = hpar[u];
                path.push_back(kk % 4);
                u = kk / 4;
            }
            std::reverse(path.begin(), path.end());
            return path;
        };
        // the best tile next to an enemy head from which a step goes into it: (steps, k)
        auto ram_way = [&](int ei, int &bd)
        {
            int bk = -1;
            bd = 99;
            for (int d = 0; d < 4; d++)
            {
                int u = mv[ei * 4 + d];
                if (u < 0)
                    continue;
                int du = hdist[u];
                if (du < 0 || du + 1 > budget || du + 1 >= bd)
                    continue;
                int k = u * 4 + BACK[d];
                if (mv[k] == ei)
                {
                    bd = du + 1;
                    bk = k;
                }
            }
            return bk;
        };
        if (!queen || (cfg::QQRAM && !qs))
        {
            for (auto &en : t.enemies)
            {
                if (en.id != 0 && en.id != 1)
                    continue;
                int bd;
                int bk = ram_way(en.i, bd);
                if (bk >= 0)
                {
                    why = "qram";
                    act = emit_move(path_to(bk), t.hg);
                    return true;
                }
            }
        }
        if (3 <= t.len && t.len <= cfg::FIGHT_MAX_LEN && !qnr)
        {
            for (auto &en : t.enemies)
            {
                int ei = en.i;
                int only = -1, n_ex = 0;
                for (int k = ei * 4; k < ei * 4 + 4; k++)
                {
                    int x = mv[k];
                    if (x <= -2)
                        n_ex += 2;
                    else if (x >= 0 && !t.occ[x])
                    {
                        n_ex += 1;
                        only = x;
                    }
                }
                if (n_ex != 1)
                    continue;
                int k1 = hdist[only];
                if (k1 < 1 || k1 + 1 > budget || k1 > en.len)
                    continue;
                for (int dd = 0; dd < 4; dd++)
                {
                    int y = mv[only * 4 + dd];
                    if (y < 0 || y == ei || t.freeat[y] > k1 + 1 || hpar[only] / 4 == y)
                        continue;
                    bool out = false;
                    for (int kk = y * 4; kk < y * 4 + 4; kk++)
                    {
                        int z = mv[kk];
                        if (z <= -2 || (z >= 0 && z != only && !t.occ[z]))
                        {
                            out = true;
                            break;
                        }
                    }
                    if (!out)
                        continue;
                    // walk through its only exit and on: it is shut in behind me
                    std::vector<int> path{dd};
                    int u = only;
                    while (u != HEAD)
                    {
                        int kk = hpar[u];
                        path.push_back(kk % 4);
                        u = kk / 4;
                    }
                    std::reverse(path.begin(), path.end());
                    why = "trap";
                    act = emit_move(path, t.hg);
                    return true;
                }
            }
        }
        if (qnr)
            return false; // the big-map queen trades herself with nobody
        std::vector<Rival> ens = t.enemies;
        if (t.len >= 12 && 2 * t.len >= friend_max)
            for (auto &en : ens)
                en.len = vis_n.get(en.id, en.len); // a long dragon trusts only what it sees
        bool even_ok = true;
        if (t.rnd < 490)
            even_ok = (int)t.friends.size() > (int)ens.size() - 1 + cfg::EVEN_MARGIN;
        bool have = false;
        int best_sc = 0, best_k = -1;
        const bool maj_now = cfg::MAJ_HUNT && !queen && t.len <= cfg::MAJ_MAXLEN && t.n_fr + 1 > t.n_en + cfg::MAJ_MARGIN; // v40.1.0 (me and the friends in view outnumber them)
        for (auto &en : ens)
        {
            int ln = en.len;
            if (ln < t.len && !(ln >= t.len - 1 && t.len <= cfg::RAM_DOWN_MAXLEN && even_ok) &&
                !(maj_now && ln >= t.len - cfg::MAJ_RAM_DOWN))
                continue; // v40.1.0: in a local majority a short worker trades down
            if (ln == t.len && !even_ok)
                continue;
            int bd;
            int bk = ram_way(en.i, bd);
            if (bk < 0)
                continue;
            int sc = ln * 10 - bd;
            if (!have || sc > best_sc)
            {
                have = true;
                best_sc = sc;
                best_k = bk;
            }
        }
        if (!have)
            return false;
        why = "ram";
        act = emit_move(path_to(best_k), t.hg);
        return true;
    }

    // late in the game a short dragon next to a much longer friend steps into its own neck: the friend eats its way
    // along the drops
    inline bool Dragon::try_feed(const Turn &t, std::string &act)
    {
        if (trail.size() < 2 || qnr || (qk && queen) || (queen && cfg::Q_NOFEED))
            return false;
        int left = 499 - t.rnd;
        int drops[49];
        for (int i = 0; i < 49; i++)
            drops[i] = -1;
        int ndrops = 0;
        for (size_t k = 0; k < trail.size(); k += 2)
        {
            int i = vi(trail[k]);
            if (i >= 0)
            {
                if (drops[i] < 0)
                    ndrops++;
                drops[i] = (int)k;
            }
        }
        if (!ndrops)
            return false;
        bool mine[49] = {false};
        for (int g : trail)
        {
            int i = vi(g);
            if (i >= 0)
                mine[i] = true;
        }
        int top = 0;
        for (auto &h : t.heads)
            if (!h.enemy && t.seg_n.get(h.id, 0) > top)
                top = t.seg_n.get(h.id, 0);
        if (friend_max - 1 > top)
            top = friend_max - 1;
        std::optional<Sighting> kp = king_pos;
        int kid_ = kid;
        if (kid_ >= 0)
        {
            if (lead)
                return false;
            int margin = t.len >= cfg::FEED_KING_MARGIN_LEN ? cfg::FEED_KING_MARGIN : 0;
            if (t.rnd - kp->round > cfg::FEED_FRESH)
                return false;
            if (qk)
            {
                if (!cfg::QKING_FEED_LONGER && kp->len < t.len)
                    return false; // only shorter dragons feed the crowned queen
            }
            else if (kp->len < t.len + margin || kp->len < t.len)
            {
                return false;
            }
        }
        else if (kp && t.rnd - kp->round > 4)
        {
            kp.reset();
        }
        int best_gain = 0;
        for (auto &h : t.heads)
        {
            if (h.enemy)
                continue;
            int hi = h.i;
            int n = t.seg_n.get(h.id, 0);
            if (kid_ >= 0)
            {
                if ((h.id & 0xFFFF) != kid_)
                    continue;
            }
            else if (kp)
            {
                if (mem.wrap_dist(t.wg[hi], kp->tile) > 3 || kp->len < cfg::FEED_RATIO * t.len)
                    continue;
            }
            else if (n < cfg::FEED_KING_MIN || n < cfg::FEED_RATIO * t.len || n < top)
            {
                continue;
            }
            // steps from its head to the nearest drop (my body turns into open water)
            int dist[49];
            for (int i = 0; i < 49; i++)
                dist[i] = -1;
            dist[hi] = 0;
            std::vector<int> q{hi};
            int dmin = -1;
            for (size_t qi = 0; qi < q.size() && dmin < 0; qi++)
            {
                int u = q[qi];
                int nd = dist[u] + 1;
                if (nd > cfg::FEED_DIST)
                    break;
                for (int k = u * 4; k < u * 4 + 4; k++)
                {
                    int v = t.mv[k];
                    if (v < 0 || dist[v] >= 0)
                        continue;
                    if (t.occ[v] && !mine[v])
                        continue;
                    dist[v] = nd;
                    if (drops[v] >= 0)
                    {
                        dmin = nd;
                        break;
                    }
                    q.push_back(v);
                }
            }
            if (dmin < 0 || dmin > left)
                continue;
            int gain = 1 + (left - dmin) / 2;
            if (gain > ndrops)
                gain = ndrops;
            if (gain > best_gain)
                best_gain = gain;
        }
        if (best_gain < 1)
            return false;
        int neck = trail[1];
        const auto &nl = mem.adj[(size_t)t.hg];
        for (int d = 0; d < 4; d++)
            if (nl[(size_t)d] == neck)
            {
                act = std::string("MOVE ") + DIR_CH[d];
                return true;
            }
        return false;
    }

    // stuck head first: keep two segments, the rest walks away from the tail end
    inline bool Dragon::reverse_split(const Turn &t, std::string &act)
    {
        if (t.len < 4 || t.units >= limit || t.tn < t.len)
            return false;
        int ti = vi(trail[(size_t)(t.len - 1)]);
        if (ti >= 0)
        {
            bool any = false;
            for (int k = ti * 4; k < ti * 4 + 4; k++)
            {
                int v = t.mv[k];
                if (v == -2 || (v >= 0 && !t.occ[v]))
                {
                    any = true;
                    break;
                }
            }
            if (!any)
                return false;
        }
        act = "SPLIT " + std::to_string(t.len - 2);
        return true;
    }

    inline std::string Dragon::boxed_in(const Turn &t, int mydir)
    {
        int best = -1;
        for (int d = 0; d < 4; d++)
        {
            int v = t.mv[d + 96];
            if (v >= 0 && t.occ[v] == 2)
                for (auto &h : t.heads)
                    if (h.i == v && h.enemy)
                        best = d;
        }
        if (best >= 0 && t.units >= 2 && !qnr)
            return std::string("MOVE ") + DIR_CH[best]; // take one with me
        int run = shed_last == t.rnd - 1 ? shed_run : 0;
        if (run >= cfg::BOXED_REV_AFTER && t.len >= cfg::BOXED_REV_LEN && t.units < limit && t.tn < t.len)
        {
            shed_run = 0;
            return "SPLIT " + std::to_string(t.len - 2);
        }
        if (t.len >= 4 && t.units < limit)
        {
            shed_run = run + 1;
            shed_last = t.rnd;
            return "SPLIT 2"; // a split is the only way to stand still
        }
        if (mydir < 0)
            return "";
        int order[4] = {mydir, (mydir + 1) % 4, (mydir + 3) % 4, (mydir + 2) % 4};
        for (int d : order)
        {
            int v = t.mv[d + 96];
            if (v >= 0 && t.occ[v] == 2)
                continue;
            return std::string("MOVE ") + DIR_CH[d];
        }
        return std::string("MOVE ") + DIR_CH[mydir];
    }

    // right of way (ROW): does this friend go before me? Our queen first; then a dragon taking itself for the longest
    // (longer than ROW_LONG_SMALL / ROW_LONG_BIG); then the longer, by its whole body in view or its sonar; then the
    // lower id (the older one, likely longer)
    inline bool Dragon::friend_outranks(const Turn &t, int fid) const
    {
        if (queen)
            return false;
        if (fid == 0 || fid == 1)
            return true;
        int thr = (double)W * H <= cfg::ROW_SMALL_AREA ? cfg::ROW_LONG_SMALL : cfg::ROW_LONG_BIG;
        int fl = -1;
        if (t.seg_n.has(fid) && !t.seg_ring.has(fid))
            fl = t.seg_n.get(fid, 0);
        for (auto &h : row_heard)
            if (h.first == fid && t.rnd - h.second.second <= 2 && h.second.first > fl)
                fl = h.second.first;
        bool mlong = t.len > thr;
        if (mlong && fl <= thr)
            return false; // I take myself for the longest
        if (!mlong && fl > thr)
            return true;
        if (fl >= 0 && fl != t.len)
            return fl > t.len;
        return fid < id;
    }

    // a step that kills only me: into kelp, else my own body, else a body that stays put (never a head)
    inline bool Dragon::row_die(const Turn &t, std::string &act) const
    {
        int best = -1, bk = 9;
        for (int d = 0; d < 4; d++)
        {
            int v = t.mv[d + 96];
            int k = 9;
            if (v == -1)
                k = 0;
            else if (v >= 0 && v != HEAD && t.occ[v] == 3 && t.freeat[v] > 1)
                k = 1;
            else if (v >= 0 && t.occ[v] == 1 && t.freeat[v] > 1)
                k = 2;
            if (k < bk)
            {
                bk = k;
                best = d;
            }
        }
        if (best < 0 || bk == 9)
            return false;
        act = std::string("MOVE ") + DIR_CH[best];
        return true;
    }

    // the direction from my head along which a sonar reaches this view tile first (next to me, or straight ahead)
    inline int Dragon::row_dir_to(int target) const
    {
        for (int d = 0; d < 4; d++)
        {
            int u = HEAD;
            for (int s = 0; s < 3; s++)
            {
                u = VT().step[u * 4 + d];
                if (u < 0)
                    break;
                if (u == target)
                    return d;
            }
        }
        return -1;
    }

    // hunted at the head: split off all but two if the tail end can get away
    inline bool Dragon::escape_split(const Turn &t, std::string &act)
    {
        if (t.len < 4)
            return false;
        if (t.tn >= t.len)
        {
            int need = cfg::ESC_ROOM;
            if (500 - t.rnd < need)
                need = 500 - t.rnd;
            if (need > 0 && tail_room(trail[(size_t)(t.len - 1)], need, t.occ) < need)
                return false;
            int tg = trail[(size_t)(t.len - 1)];
            int ti = vi(tg);
            if (ti >= 0)
            {
                bool any = false;
                for (int k = ti * 4; k < ti * 4 + 4; k++)
                {
                    int v = t.mv[k];
                    if (v == -2 || (v >= 0 && !t.occ[v]))
                    {
                        any = true;
                        break;
                    }
                }
                if (!any)
                    return false;
                for (auto &h : t.heads)
                    if (h.enemy && vman(h.i, ti) <= 2)
                        return false;
            }
            else
            {
                int free = 0;
                for (int v : mem.adj[(size_t)tg])
                    if (v >= 0 && std::find(trail.begin(), trail.end(), v) == trail.end())
                        free++;
                if (free < cfg::ESC_TAIL)
                    return false;
            }
        }
        act = "SPLIT " + std::to_string(t.len - 2);
        return true;
    }

    // a sprint along these tiles, step by step as the engine moves it: into my own body kills; a pearl holds the tail
    // for that step; a step past the free ones sheds a segment
    inline bool Dragon::sprint_ok(const std::vector<int> &steps, int length) const
    {
        int free = ceil_div(length, sdiv);
        std::deque<int> body;
        for (int k = 0; k < length && k < (int)trail.size(); k++)
            body.push_back(trail[(size_t)k]);
        std::vector<int> eaten;
        for (size_t j = 0; j < steps.size(); j++)
        {
            int g = steps[j];
            for (int b : body)
                if (b == g)
                    return false;
            body.push_front(g);
            if (pearls.has(g) && std::find(eaten.begin(), eaten.end(), g) == eaten.end())
                eaten.push_back(g);
            else
                body.pop_back();
            if ((int)j >= free)
            {
                if (body.size() <= 2)
                    return false;
                body.pop_back();
            }
        }
        return true;
    }

    // the big-map queen: her best step is within a rammer's reach (or every step looks fatal) - sprint out of it
    inline bool Dragon::escape_sprint(const Turn &t, const Option &best, double threat_w, int need_depth, int qfatal,
                                      std::string &act)
    {
        int free = ceil_div(t.len, sdiv);
        int kmax = cfg::Q_ESC;
        if (kmax > free + t.len - 2)
            kmax = free + t.len - 2;
        double base = qfatal ? 1e9 : threat_w * t.tpen[best.v];
        if (kmax < 2)
            return false;
        int hdist[49], hpar[49];
        for (int i = 0; i < 49; i++)
        {
            hdist[i] = -1;
            hpar[i] = -1;
        }
        hdist[HEAD] = 0;
        std::vector<int> q{HEAD};
        for (size_t qi = 0; qi < q.size(); qi++)
        {
            int u = q[qi];
            int nd = hdist[u] + 1;
            if (nd > kmax)
                break;
            for (int k = u * 4; k < u * 4 + 4; k++)
            {
                int v = t.mv[k];
                if (v >= 0 && hdist[v] < 0 && t.freeat[v] <= nd)
                {
                    hdist[v] = nd;
                    hpar[v] = k;
                    q.push_back(v);
                }
            }
        }
        std::vector<std::tuple<double, int, int>> cands;
        for (int v = 0; v < 49; v++)
        {
            int s = hdist[v];
            if (s < 2 || t.treach[v])
                continue;
            int paid = s > free ? s - free : 0;
            double c = threat_w * t.tpen[v] + cfg::Q_ESC_COST * paid;
            if (c + 1.0 < base)
                cands.push_back({c, s, v});
        }
        if (cands.empty())
            return false;
        std::sort(cands.begin(), cands.end());
        size_t lim = cands.size() < 6 ? cands.size() : 6;
        for (size_t ci = 0; ci < lim; ci++)
        {
            int s = std::get<1>(cands[ci]), v = std::get<2>(cands[ci]);
            std::vector<int> path_t{v}; // newest first, without my head
            int u = v;
            while (u != HEAD)
            {
                u = hpar[u] / 4;
                if (u != HEAD)
                    path_t.push_back(u);
            }
            std::vector<int> steps;
            for (size_t i = path_t.size(); i-- > 0;)
                steps.push_back(t.wg[path_t[i]]);
            if (!sprint_ok(steps, t.len))
                continue;
            int nl = t.len - (s > free ? s - free : 0);
            std::vector<int> body;
            for (size_t i = 0; i < path_t.size() && (int)i < nl; i++)
                body.push_back(t.wg[path_t[i]]);
            long long take = (long long)nl - (long long)path_t.size();
            if (take < 0)
                take = 0;
            if (take > (long long)trail.size())
                take = (long long)trail.size();
            for (long long k = 0; k < take; k++)
                body.push_back(trail[(size_t)k]);
            ba_scratch.clear();
            for (size_t k = 1; k < body.size(); k++)
                ba_scratch.set(body[k], nl - (int)k + 2);
            int want = need_depth <= nl + 4 ? need_depth : nl + 4;
            if (survive(t.wg[v], want, ba_scratch, 0, nl, &others, &pearls) < want)
                continue;
            std::vector<int> path;
            u = v;
            while (u != HEAD)
            {
                int k = hpar[u];
                path.push_back(k % 4);
                u = k / 4;
            }
            std::reverse(path.begin(), path.end());
            act = emit_move(path, t.hg);
            return true;
        }
        return false;
    }

    // the first ceil(L / 4) steps of a move cost nothing: after the chosen step go on towards the pearl that drew it
    // (and the next ones in view) or along my route, over open water, and stop on the last tile that is no more
    // threatened than the first step's and from which I can keep moving
    inline bool Dragon::free_sprint(const Turn &t, const Option &best, double threat_w, int need_depth,
                                    std::vector<int> &path)
    {
        const int *mv = t.mv;
        int free = ceil_div(t.len, sdiv);
        if (free < 2)
            return false;
        int d1 = best.d, v1 = best.v;
        std::vector<int> ext;
        int goal = food_goal;
        int gw = goal >= 0 ? vi(goal) : -1;
        const bool on_route = goal < 0 && has_route && !route.empty() && route[0] == t.wg[v1];
        if (gw >= 0 && gw != v1)
        {
            int par[49];
            for (int i = 0; i < 49; i++)
                par[i] = -1;
            par[v1] = v1;
            std::vector<int> q{v1};
            bool found = false;
            for (size_t qi = 0; qi < q.size() && !found; qi++)
            {
                int u = q[qi];
                for (int k = u * 4; k < u * 4 + 4; k++)
                {
                    int x = mv[k];
                    if (x >= 0 && par[x] < 0 && !t.occ[x] && t.freeat[x] == 0)
                    {
                        par[x] = u;
                        if (x == gw)
                        {
                            found = true;
                            break;
                        }
                        q.push_back(x);
                    }
                }
            }
            if (found)
            {
                for (int u = gw; u != v1; u = par[u])
                    ext.push_back(u);
                std::reverse(ext.begin(), ext.end());
            }
        }
        else if (on_route)
        {
            int u = v1;
            for (size_t ri = 1; ri < route.size() && (int)ri < free; ri++)
            {
                int x = vi(route[ri]);
                if (x < 0 || t.occ[x] || t.freeat[x] != 0)
                    break;
                if (!(mv[u * 4] == x || mv[u * 4 + 1] == x || mv[u * 4 + 2] == x || mv[u * 4 + 3] == x))
                    break;
                ext.push_back(x);
                u = x;
            }
        }
        if (gw >= 0 && (gw == v1 || !ext.empty()))
        {
            // the pearl is reached with free steps to spare: on to the nearest pearl in view
            int left = free - 1 - (int)ext.size();
            bool used[49] = {false};
            used[v1] = true;
            for (int x : ext)
                used[x] = true;
            int cur_t = ext.empty() ? v1 : ext.back();
            while (left > 0)
            {
                int par[49], dep[49];
                for (int i = 0; i < 49; i++)
                {
                    par[i] = -2;
                    dep[i] = 0;
                }
                par[cur_t] = -1;
                std::vector<int> q{cur_t};
                int hit = -1;
                for (size_t qi = 0; qi < q.size() && hit < 0; qi++)
                {
                    int u = q[qi];
                    if (dep[u] >= left)
                        continue;
                    for (int k = u * 4; k < u * 4 + 4; k++)
                    {
                        int x = mv[k];
                        if (x >= 0 && par[x] == -2 && !used[x] && !t.occ[x] && t.freeat[x] == 0)
                        {
                            par[x] = u;
                            dep[x] = dep[u] + 1;
                            if (t.wp[x])
                            {
                                hit = x;
                                break;
                            }
                            q.push_back(x);
                        }
                    }
                }
                if (hit < 0)
                    break;
                std::vector<int> seg;
                for (int u = hit; u != cur_t; u = par[u])
                    seg.push_back(u);
                std::reverse(seg.begin(), seg.end());
                for (int x : seg)
                {
                    used[x] = true;
                    ext.push_back(x);
                }
                left -= (int)seg.size();
                cur_t = hit;
            }
        }
        if (ext.empty())
            return false;
        if ((int)ext.size() > free - 1)
            ext.resize((size_t)(free - 1));
        double base_t = threat_w * t.tpen[v1];
        int base_r = t.treach[v1];
        for (int n = (int)ext.size(); n >= 1; n--)
        {
            int fin = ext[(size_t)(n - 1)];
            if (threat_w * t.tpen[fin] > base_t + 1e-9 || t.treach[fin] > base_r)
                continue;
            std::vector<int> tiles{v1};
            for (int i = 0; i < n; i++)
                tiles.push_back(ext[(size_t)i]);
            int eaten = 0;
            for (int x : tiles)
                if (t.wp[x])
                    eaten++;
            int nl = t.len + eaten;
            int sn = (int)tiles.size();
            std::vector<int> body;
            for (int i = sn - 1; i >= 0 && (int)body.size() < nl; i--)
                body.push_back(t.wg[tiles[(size_t)i]]);
            int take = nl - sn;
            if (take < 0)
                take = 0;
            if (take > (int)trail.size())
                take = (int)trail.size();
            for (int k = 0; k < take; k++)
                body.push_back(trail[(size_t)k]);
            ba_scratch.clear();
            for (size_t k = 1; k < body.size(); k++)
                ba_scratch.set(body[k], nl - (int)k + 2);
            int need = need_depth <= nl + 4 ? need_depth : nl + 4;
            if (survive(t.wg[fin], need, ba_scratch, 0, nl, &others, &pearls) < need)
                continue;
            path.clear();
            path.push_back(d1);
            int u = v1;
            for (int i = 0; i < n; i++)
            {
                int x = ext[(size_t)i];
                for (int dd = 0; dd < 4; dd++)
                    if (mv[u * 4 + dd] == x)
                    {
                        path.push_back(dd);
                        break;
                    }
                u = x;
            }
            if (on_route)
            {
                size_t k = 0;
                while (k < route.size())
                {
                    bool walked = false;
                    for (int x : tiles)
                        if (t.wg[x] == route[k])
                        {
                            walked = true;
                            break;
                        }
                    if (!walked)
                        break;
                    k++;
                }
                route.erase(route.begin(), route.begin() + (long)k);
            }
            return true;
        }
        return false;
    }

    // lanes: a pearl on lane tile g for a head that gets there at step k (1 = the first step of this turn)?
    inline bool Dragon::lane_pearl(int g, int k)
    {
        const Turn &t = *cur;
        int i = vi(g);
        if (i >= 0)
        {
            if (t.wp[i])
                return true;
            int c = t.wcd[i];
            return c >= 0 && c + 1 <= k;
        }
        if (!mem.pearl_known(g))
            return false;
        int h = mem.due[(size_t)g];
        return h <= t.rnd + k - 1 && h >= t.rnd - cfg::POCKET_MEM_AGE;
    }

    // the most pearls a head on lane tile u (reached at step k) collects going deeper along one path; a dragon on the
    // way ends that path (*busy)
    inline int Dragon::lane_food(int u, int k, bool *busy, int lim)
    {
        if (lim <= 0)
            return 0;
        const Turn &t = *cur;
        int best = 0;
        int du = mem.lane_depth[(size_t)u];
        for (int v : mem.adj[(size_t)u])
        {
            if (v < 0 || mem.lane_depth[(size_t)v] <= du)
                continue;
            int i = vi(v);
            if ((i >= 0 && t.occ[i]) || (i < 0 && lane_occ_seen[(size_t)v] >= t.rnd - cfg::LANE_OCC_MEM))
            {
                if (busy)
                    *busy = true;
                continue;
            }
            int f = (lane_pearl(v, k + 1) ? 1 : 0) + lane_food(v, k + 1, busy, lim - 1);
            if (f > best)
                best = f;
        }
        return best;
    }

} // namespace v30