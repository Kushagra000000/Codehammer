// v30 navigation: when nothing in view is worth eating, head for remembered pearls, unseen or stale water, the
// centre (opening rush), or the king (guards) - breadth-first over the remembered map, routes kept between turns.
#pragma once

#include "dragon.hpp"

namespace v30 {

inline int Dragon::dir_to(int a, int b) const {
    const auto& nl = mem.adj[(size_t)a];
    for (int d = 0; d < 4; d++)
        if (nl[(size_t)d] == b) return d;
    return -1;
}

// the next step of the route I am on, if it still holds; -1: plan again
inline int Dragon::follow(int hg, const int* legal, int rnd) {
    if (!has_route || route.empty()) return -1;
    size_t k = 0, n = route.size();
    while (k < n && route[k] == hg) k++;
    if (k) route.erase(route.begin(), route.begin() + (long)k);
    if (route.empty()) {
        has_route = false;
        route_rush = false;
        if (route_king) {
            if (!route_part) king_seen.reset();          // got there and it is not in view: stale
            route_king = false;
            route_part = false;
        }
        return -1;
    }
    if (rnd - route_round > cfg::ROUTE_AGE) return -1;
    int goal = route.back();
    if (route_kind == 1) {
        if (!mem.pearl_known(goal) || mem.due[(size_t)goal] > rnd + (double)route.size() + cfg::ROUTE_SLACK) return -1;
    } else if (route_kind == 0) {
        int sg = mem.seen[(size_t)goal];
        if (sg >= rnd - 1 || sg == DEAD_TILE) return -1;
    }
    // I may have stepped aside for a pearl: pick the route up where it passes next to me
    int d = -1;
    n = route.size();
    for (size_t k2 = 0; k2 < (n > 3 ? 3 : n); k2++) {
        int dd = dir_to(hg, route[k2]);
        if (dd >= 0) {
            if (k2) route.erase(route.begin(), route.begin() + (long)k2);
            d = dd;
            break;
        }
    }
    if (d < 0) return -1;
    if (legal[d] != route[0]) return -1;
    if (mem.edges_changed) {
        // an edge may have turned out to be kelp: walk the route once
        int a = hg;
        for (int b : route) {
            if (dir_to(a, b) < 0) return -1;
            a = b;
        }
        mem.edges_changed = false;
    }
    return d;
}

inline int Dragon::navigate(const Turn& t, std::vector<Option>& scored, bool blind_ok) {
    const int hg = t.hg, rnd = t.rnd, mydir = t.mydir;
    int legal[4] = {-1, -1, -1, -1};
    for (auto& sc : scored)
        if (sc.s >= -20 && sc.g >= 0) legal[sc.d] = sc.g;
    int kgoal = -1;
    if (hunt_now && prey && prey->tile != hg) kgoal = prey->tile;
    else if (guard_now && king_seen && king_seen->tile != hg &&
             rnd - king_seen->round <= (gathering ? std::max(cfg::GUARD_MEM, cfg::GATHER_MEM) : cfg::GUARD_MEM))
        kgoal = king_seen->tile;
    double credit = nav_credit + cfg::NAV_RATE;
    if (credit > cfg::NAV_MAX) credit = cfg::NAV_MAX;
    nav_credit = credit;
    if (route_king) {
        bool drop = kgoal < 0;
        int ref = route_part ? route_kpos : route_goal;
        if (!drop && ref != kgoal) drop = mem.wrap_dist(kgoal, ref) > (route_part ? 6 : 3);
        if (drop) {
            has_route = false;
            route.clear();
            route_king = false;
            route_part = false;
        }
    }
    int fd = follow(hg, legal, rnd);
    if (fd >= 0) {
        if (route_rush) nav_w = cfg::RUSH_W;
        return fd;
    }
    // explore on a private diagonal: two perpendicular headings taken in turn, redrawn now and then
    if (pref < 0 || rnd - pref_round > cfg::EXPLORE_REDRAW) {
        int d1 = pref < 0 ? mydir : (int)floormod((long long)(noise.next() * 4), 4);
        int d2 = (d1 + (noise.next() < 0.5 ? 1 : 3)) % 4;
        if (pref < 0 && rnd == 0) {
            // the first heading across the map points at the centre
            if (d1 == 0 || d1 == 2) {
                int off = (int)floormod((W / 2 - hg % W) + W / 2, W) - W / 2;
                if (off) d2 = off > 0 ? 1 : 3;
            } else {
                int off = (int)floormod((H / 2 - hg / W) + H / 2, H) - H / 2;
                if (off) d2 = off > 0 ? 2 : 0;
            }
        }
        pref = d1;
        pref2 = d2;
        pref_round = rnd;
    }
    int a = pref, b = pref2;
    if (last_nav == a) std::swap(a, b);
    int order[4] = {a, b, (b + 2) % 4, (a + 2) % 4};
    std::vector<int>& par = nav_par;
    auto& pkeys = nav_par_keys;
    auto par_set = [&](int g, int v) {
        if (par[(size_t)g] == NONE) pkeys.push_back(g);
        par[(size_t)g] = v;
    };
    par_set(hg, -1);
    std::vector<int> q;
    for (int d : order) {
        int g2 = legal[d];
        if (g2 >= 0 && par[(size_t)g2] == NONE) {
            par_set(g2, hg);
            q.push_back(g2);
        }
    }
    nav_blocked.clear();                     // my body and every dragon in view stand still
    for (int g : body_at.keys) nav_blocked.add(g);
    for (int i = 0; i < 49; i++) {
        int g = gi_wg[i];
        if (gi[(size_t)g] == i && t.occ[i]) nav_blocked.add(g);
    }
    for (int g : nav_blocked.list)
        if (par[(size_t)g] == NONE) par_set(g, -2);
    int stale = rnd - cfg::REVISIT;
    double best_cost = 1e9;
    int best_tile = -1, best_kind = 0;
    long long nodes = 0;
    double cap = nav_credit < cfg::NAV_MIN ? cfg::NAV_MIN : nav_credit;
    const bool starving = cfg::SE_ROUNDS > 0 && rnd - last_eat >= cfg::SE_ROUNDS;
    if (starving && cap < cfg::SE_CAP) cap = cfg::SE_CAP;
    if ((rnd >= guard_round_eff || quiet) && body_at.keys.size() >= 12 && cap < cfg::KING_NAV_CAP) cap = cfg::KING_NAV_CAP;
    int rush = -1;
    if (rnd < cfg::RUSH_ROUND && rush_ok > 0) {
        // no food seen yet: the middle of the map counts as a pearl until something better shows up
        rush = rush_target;
        if (mem.seen[(size_t)rush] != NEVER_SEEN) {
            rush = -1;
            if (rush_ok == 1) rush_ok = 2;
        } else if (cap < cfg::RUSH_CAP) {
            cap = cfg::RUSH_CAP;
        }
    } else if (rush_ok == 1 && rnd >= cfg::RUSH_ROUND) {
        rush_ok = 2;
    }
    if (rush_ok == 2) rush_ok = 3;
    int stick = route_goal;
    if (blind_ok)
        for (auto& sc : scored)
            if (sc.kind == 2 && sc.s >= -20 && 1 + cfg::BLIND_COST < best_cost) {
                best_cost = 1 + cfg::BLIND_COST;            // an unexplored portal next to me
                best_tile = -2 - sc.d;
                best_kind = 2;
            }
    int dep = 1;
    std::map<long long, std::vector<std::pair<int, int>>> later;   // through a portal: arrives later
    bool reach = kgoal >= 0;
    int near_t = -1, near_dep = 0, near_d = 0;
    if (reach) {
        if (cap > cfg::GUARD_NAV_CAP) cap = cfg::GUARD_NAV_CAP;
        near_d = mem.wrap_dist(hg, kgoal);
    }
    // the opening's pull towards the centre (small maps, until the centre is seen to be barren)
    double oe_w = 0.0, oe_rad = 1.0;
    const int oe_cx = W / 2, oe_cy = H / 2;
    if (rnd < cfg::OE_ROUND && !((double)W * H >= cfg::OE_AREA) && (id >> 1) % cfg::OE_MOD == 0 &&
        !(cfg::OE_STARVE > 0 && rnd - last_eat >= cfg::OE_STARVE) && !(queen && cfg::QOPEN_HOME)) {
        int oseen = 0, olive = 0;
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4 + iabs(dy); dx <= 4 - iabs(dy); dx++) {
                int tt = ((oe_cy + dy) % H + H) % H * W + ((oe_cx + dx) % W + W) % W;
                int sv = mem.seen[(size_t)tt];
                if (sv == NEVER_SEEN) continue;
                oseen++;
                if (sv != DEAD_TILE) olive++;
            }
        if (!(oseen >= cfg::OE_BMIN && olive < cfg::OE_BFRAC * oseen)) {
            oe_w = cfg::OE_W;
            oe_rad = cfg::OE_R * (W + H) / 2.0;
            if (oe_rad < 1.0) oe_rad = 1.0;
        }
    }
    auto oe_pull = [&](int u) -> double {
        int dx = iabs(u % W - oe_cx), dy = iabs(u / W - oe_cy);
        if (dx > W - dx) dx = W - dx;
        if (dy > H - dy) dy = H - dy;
        double c = 1.0 - (dx + dy) / oe_rad;
        return c > 0.0 ? oe_w * c : 0.0;
    };
    std::vector<int> nq;
    double fount_slack = 0.0;                   // (FOUNT_NAV) a fountain further away may still be the best goal
    if (cfg::FOUNT_NAV > 0 && rush < 0 && !reach)
        for (int g = 0; g < N && fount_slack == 0.0; g++)
            if (mem.fountain[(size_t)g]) fount_slack = cfg::FOUNT_NAV;
    while ((!q.empty() || !later.empty()) && nodes < cap && dep < best_cost + oe_w + fount_slack) {
        nq.clear();
        for (size_t qi = 0; qi < q.size(); qi++) {
            int u = q[qi];
            nodes++;
            int su = mem.seen[(size_t)u];
            int kind = 0;
            double cost;
            if (reach) {
                int dd = mem.wrap_dist(u, kgoal);
                if (dd < near_d) {
                    near_d = dd;
                    near_t = u;
                    near_dep = dep;
                }
            }
            bool stale_look = false;
            if (u != rush && su < stale)
                stale_look = !(mem.pearl_known(u) && mem.due[(size_t)u] > rnd);
            if (cfg::FOUNT_NAV > 0 && mem.fountain[(size_t)u] && u != rush && rush < 0 && !reach) {
                cost = dep - cfg::FOUNT_NAV;                // a known fountain: worth the trip
                kind = 1;
            } else if (u == rush) {
                cost = dep;
                kind = 2;
            } else if (stale_look) {
                if (rush >= 0) cost = dep + 30;            // unexplored water is not the goal while rushing
                else if (reach) cost = 1e9;                // a guard on its way does not stop to look
                else if (su == NEVER_SEEN) {
                    cost = dep + unseen_cost_now;
                    if (oe_w > 0.0) cost -= oe_pull(u);
                } else {
                    cost = starving ? 1e9 : dep + 6;       // water I have not looked at for a while
                    if (oe_w > 0.0) cost -= oe_pull(u);
                }
            } else if (!mem.pearl_known(u)) {
                cost = 1e9;
            } else {
                int h = mem.due[(size_t)u];
                int arr = h - rnd + 1;
                if (arr < dep) arr = dep;
                cost = arr + (h < rnd - 25 ? 4 : 0);
                kind = 1;
                if (rush >= 0 && arr > dep + cfg::RUSH_WAIT) cost = 1e9;
            }
            if (cost < best_cost || u == stick || u == kgoal) {
                if (u == stick) cost -= cfg::ROUTE_STICK;
                if (cost < best_cost && banned[(size_t)u] > rnd) cost = 1e9;   // the way in was fatal
                if (u == kgoal) {
                    double c2 = dep - (gathering ? cfg::GATHER_BONUS : (double)cfg::GUARD_MEM_BONUS);
                    if (c2 < cost) {
                        cost = c2;
                        kind = 2;
                    }
                }
                if (cost < best_cost) {
                    best_cost = cost;
                    best_tile = u;
                    best_kind = kind;
                }
            }
            const auto& nl = mem.adj[(size_t)u];
            bool pu = mem.has_portal_tile(u);
            for (int d = 0; d < 4; d++) {
                int v = nl[(size_t)d];
                if (v < 0) {
                    if (v == -3 && blind_ok && dep + cfg::BLIND_COST < best_cost) {
                        best_cost = dep + cfg::BLIND_COST;
                        best_tile = u;
                        best_kind = 2;
                    }
                    continue;
                }
                if (par[(size_t)v] != NONE) continue;
                if (pu && mem.portal_side(u, d)) {
                    later[(long long)(dep + cfg::PORTAL_STEP)].push_back({v, u});   // may land on somebody
                    continue;
                }
                par_set(v, u);
                nq.push_back(v);
            }
        }
        dep++;
        q.swap(nq);
        auto it = later.find(dep);
        if (it != later.end()) {
            std::vector<std::pair<int, int>> lt = std::move(it->second);
            later.erase(it);
            for (auto& e : lt)
                if (par[(size_t)e.first] == NONE && !nav_blocked.has(e.first)) {
                    par_set(e.first, e.second);
                    q.push_back(e.first);
                }
        }
    }
    nav_credit -= nodes;
    if (nav_credit < 0) nav_credit = 0;
    bool part = false;
    if (reach && near_t >= 0 && best_tile != kgoal) {
        double c2 = near_dep - (gathering ? cfg::GATHER_BONUS : (double)cfg::GUARD_MEM_BONUS);      // the king is beyond the search: go part of the way
        if (c2 < best_cost) {
            best_cost = c2;
            best_tile = near_t;
            best_kind = 2;
            part = true;
        }
    }
    int result;
    if (best_tile <= -2) {
        has_route = false;
        route.clear();
        result = -2 - best_tile;
    } else {
        last_goal = best_tile >= 0 ? best_tile : -1;
        if (best_tile >= 0) {
            std::vector<int> rt;
            int g = best_tile;
            bool broken = false;
            while (g != hg) {
                rt.push_back(g);
                g = par[(size_t)g];
                if (g == NONE || g < 0) {
                    broken = true;
                    break;
                }
            }
            if (broken) {
                result = -1;
            } else {
                std::reverse(rt.begin(), rt.end());
                result = dir_to(hg, rt[0]);
                has_route = true;
                route = rt;
                route_goal = best_tile;
                route_kind = best_kind;
                route_round = rnd;
                mem.edges_changed = false;
                route_king = best_tile == kgoal || part;
                route_part = part;
                route_kpos = kgoal;
                route_rush = best_tile == rush;
                if (best_tile == rush) nav_w = cfg::RUSH_W;
            }
        } else {
            result = -1;
            for (int d : order)               // nothing found in the budget: keep sweeping my private way
                if (legal[d] >= 0) {
                    result = d;
                    break;
                }
        }
    }
    for (int g : pkeys) par[(size_t)g] = NONE;
    pkeys.clear();
    return result;
}

}  // namespace v30
