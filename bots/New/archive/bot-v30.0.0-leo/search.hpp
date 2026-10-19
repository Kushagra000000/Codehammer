// v30 searches over the remembered map: how long can I keep moving from a tile (depth-first, my body freeing up
// behind me), how much room is there, food in a dead end beyond my view.
#pragma once

#include "dragon.hpp"

namespace v30 {

// The longest run of legal moves found from `start` (= step 1), capped at `need`. My body frees up as I move on
// (one step later for every pearl eaten on the way); `oth` are walls when given. Out of budget: count the room.
inline int Dragon::survive(int start, int need, const TileInts& bat, int grow, int length, const Marks* oth,
                           const Marks* prl) {
    struct Frame {
        int tile, next, prev, grown;
    };
    static std::vector<Frame> stack;
    stack.clear();
    std::vector<int>& at = surv_used;     // tile -> step it was entered on along the current path (0: not on it)
    stack.push_back({start, 0, 0, grow});
    at[(size_t)start] = 1;
    int best = 1;
    long long nodes = 0;
    long long cap = work;
    if (cap > cfg::SURV_NODES) cap = cfg::SURV_NODES;
    auto unwind = [&]() {
        for (size_t k = stack.size(); k-- > 0;) at[(size_t)stack[k].tile] = stack[k].prev;
        stack.clear();
    };
    while (!stack.empty()) {
        int depth = (int)stack.size();
        if (depth >= need) {
            work -= nodes;
            unwind();
            return need;
        }
        if (++nodes > cap) {
            work -= nodes;
            unwind();
            return room(start, need, bat, grow, (oth && !oth->empty()) ? oth : nullptr, best);
        }
        int nd = depth + 1;
        Frame& top = stack.back();
        int gr = top.grown;
        int life = length + gr;
        const auto& nl = mem.adj[(size_t)top.tile];
        bool pushed = false;
        while (top.next < 4) {
            int v = nl[(size_t)top.next];
            top.next++;
            if (v < 0) continue;
            if (surv_seen && mem.seen[(size_t)v] == NEVER_SEEN) continue;
            int pu = at[(size_t)v];
            if (pu != 0 && nd - pu <= life) continue;          // my own body is still there
            int fr = bat.get(v);
            if (fr != NONE && fr + gr > nd) continue;
            if (oth && oth->has(v)) continue;
            int ng = (prl && prl->has(v)) ? gr + 1 : gr;
            stack.push_back({v, 0, pu, ng});
            at[(size_t)v] = nd;
            if (nd > best) best = nd;
            pushed = true;
            break;
        }
        if (!pushed) {
            Frame f = stack.back();
            stack.pop_back();
            at[(size_t)f.tile] = f.prev;
        }
    }
    work -= nodes;
    return best;
}

// breadth-first count of the tiles I could reach from `start` (capped at need; at least `best`)
inline int Dragon::room(int start, int need, const TileInts& bat, int grow, const Marks* oth, int best) {
    std::vector<int> q{start};
    room_dist[(size_t)start] = 1;
    int ret = -1;
    for (size_t qi = 0; qi < q.size() && ret < 0; qi++) {
        int u = q[qi];
        int nd = room_dist[(size_t)u] + 1;
        for (int v : mem.adj[(size_t)u]) {
            if (v < 0 || room_dist[(size_t)v]) continue;
            if (surv_seen && mem.seen[(size_t)v] == NEVER_SEEN) continue;
            int fr = bat.get(v);
            if (fr != NONE && fr + grow > nd) continue;
            if (oth && oth->has(v)) continue;
            room_dist[(size_t)v] = nd;
            q.push_back(v);
            if ((int)q.size() >= need) {
                ret = need;
                break;
            }
        }
    }
    for (int g : q) room_dist[(size_t)g] = 0;
    if (ret >= 0) return ret;
    int n = (int)q.size();
    return n > best ? n : best;
}

// pearls I remember (beyond my view) within `lim` steps of `start`, due by the time I get there
inline int Dragon::pocket_mem(int start, int lim, const TileInts& bat, int grow, const Marks* oth, int rnd) {
    int old = rnd - cfg::POCKET_MEM_AGE;
    std::vector<int> q{start};
    room_dist[(size_t)start] = 1;
    int n = 0;
    for (size_t qi = 0; qi < q.size(); qi++) {
        int u = q[qi];
        int du = room_dist[(size_t)u];
        if (du >= lim) continue;
        int nd = du + 1;
        for (int v : mem.adj[(size_t)u]) {
            if (v < 0 || room_dist[(size_t)v]) continue;
            int fr = bat.get(v);
            if (fr != NONE && fr + grow > nd) continue;
            if (oth && oth->has(v)) continue;
            room_dist[(size_t)v] = nd;
            q.push_back(v);
            if (vi(v) < 0 && mem.pearl_known(v)) {
                int h = mem.due[(size_t)v];
                if (h <= rnd + nd && h >= old) n++;
            }
        }
    }
    for (int g : q) room_dist[(size_t)g] = 0;
    return n;
}

// how many tiles a child born at my tail could reach (stops at need)
inline int Dragon::tail_room(int tg, int need, const int* occ) {
    mine_set.clear();
    for (int g : trail) mine_set.add(g);
    std::vector<int> q{tg};
    room_dist[(size_t)tg] = 1;
    int n = 0, ret = -1;
    for (size_t qi = 0; qi < q.size() && ret < 0; qi++) {
        int u = q[qi];
        for (int v : mem.adj[(size_t)u]) {
            if (v < 0 || room_dist[(size_t)v] || mine_set.has(v)) continue;
            int i = vi(v);
            if (i >= 0 && occ[i]) continue;
            room_dist[(size_t)v] = 1;
            q.push_back(v);
            n++;
            if (n >= need) {
                ret = n;
                break;
            }
        }
    }
    for (int g : q) room_dist[(size_t)g] = 0;
    mine_set.clear();
    return ret >= 0 ? ret : n;
}

}  // namespace v30
