// v30 perception: reading a turn into the Turn picture and the map memory; the clocks; the king; my own body;
// the threat map.
#pragma once

#include "dragon.hpp"

namespace v30 {

inline Dragon::Dragon(int did, const std::string& tm, int w, int h, int lim)
    : id(did), team(tm), tbit(tm == "B" ? 1 : 0), W(w), H(h), N(w * h), limit(lim) {
    noise.seed(did);
    queen = did == 0 || did == 1;
    mem.init(w, h);
    rush_target = (h / 2) * w + w / 2;
    banned.assign((size_t)N, -1);
    gi.assign((size_t)N, -1);
    others.reset(N);
    pearls.reset(N);
    nav_blocked.reset(N);
    mine_set.reset(N);
    body_at.reset(N);
    ba_scratch.reset(N);
    surv_used.assign((size_t)N, 0);
    room_dist.assign((size_t)N, 0);
    nav_par.assign((size_t)N, NONE);
}

// the line layout of one turn; false when it does not parse
inline bool Dragon::read_turn(const std::vector<std::string>& L, Turn& t, long long& tiles0, long long& bodies0,
                              long long& nbod, long long& edges0) {
    if (L.size() < 5 || L[1].size() < 5) return false;
    t.rnd = (int)tail_int(L[0]);
    t.mydir = dir_index(L[1][4]);
    if (t.mydir < 0) return false;
    t.len = (int)tail_int(L[2]);
    t.units = (int)tail_int(L[3]);
    long long nm = tail_int(L[4]);
    long long p = 5;
    for (long long k = 0; k < nm && p < (long long)L.size(); k++, p++) t.msgs.push_back(L[(size_t)p]);
    p = 5 + nm;
    if (p < (long long)L.size() && L[(size_t)p].rfind("ECHOES", 0) == 0) p++;
    tiles0 = p;
    p += 49;
    if (p >= (long long)L.size()) return false;
    nbod = tail_int(L[(size_t)p]);
    bodies0 = p + 1;
    edges0 = bodies0 + nbod;
    if (edges0 + 15 > (long long)L.size()) return false;
    auto hw = words(L[(size_t)(tiles0 + 24)]);
    if (hw.size() < 2) return false;
    t.hx = (int)to_int(hw[0]);
    t.hy = (int)to_int(hw[1]);
    for (int r = 0; r < 7; r++)
        for (int c = 0; c < 7; c++)
            t.wg[r * 7 + c] = (int)floormod(t.hy - 3 + r, H) * W + (int)floormod(t.hx - 3 + c, W);
    t.hg = t.wg[HEAD];
    return true;
}

// pearls and countdowns into memory
inline void Dragon::read_tiles(const std::vector<std::string>& L, long long tiles0, Turn& t) {
    int horizon = 500 - t.rnd;
    for (int i = 0; i < 49; i++) {
        auto w = words(L[(size_t)(tiles0 + i)]);
        bool pearl = w.size() >= 2 && w[w.size() - 2] == "1";
        int cd = w.empty() ? -1 : (int)to_int(w.back());
        int g = t.wg[i];
        t.wp[i] = 0;
        t.wcd[i] = 0;
        if (mem.seen[(size_t)g] == NEVER_SEEN) {
            tiles_first++;
            if (pearl || (0 <= cd && cd <= cfg::SCARCE_DUE)) tiles_productive++;
        }
        if (pearl) {
            t.wp[i] = 1;
            t.wcd[i] = cd;
            if (0 < cd && cd <= 2) {
                int& fs = mem.fount_seen[(size_t)g];
                if (fs < 0) fs = t.rnd;
                else if (t.rnd - fs >= 3) mem.fountain[(size_t)g] = 1;
            } else {
                mem.fount_seen[(size_t)g] = -1;
            }
            mem.seen[(size_t)g] = t.rnd;
            mem.set_due(g, t.rnd);
            seen_pearl = true;
        } else if (cd < 0) {
            t.wcd[i] = -1;
            mem.seen[(size_t)g] = DEAD_TILE;
            mem.drop_due(g);
        } else {
            t.wcd[i] = cd;
            if (cd <= horizon) {
                mem.seen[(size_t)g] = t.rnd;
                mem.set_due(g, t.rnd + cd);
            } else {
                mem.seen[(size_t)g] = DEAD_TILE;     // due after the game ends
                mem.drop_due(g);
            }
        }
    }
}

// my first look: do I rush the centre? (poor water around me, early, one of the first children, not too far)
inline void Dragon::first_look(Turn& t) {
    int ok = (queen && cfg::QOPEN_HOME) ? 0 : 1;
    int n = 0;
    for (int i = 0; i < 49; i++)
        if (0 <= t.wcd[i] && t.wcd[i] <= cfg::RUSH_DUE) n++;
    if (n > cfg::RUSH_POOR) ok = 0;
    if (ok) {
        int idx = id >> 1;
        int n0;
        if (t.len == 2) {
            n0 = t.units / 2;
            if (n0 < 1) n0 = 1;
            idx -= n0;
        } else {
            n0 = t.units - (t.len >= 4 ? idx : 0);
            if (n0 < 1) n0 = 1;
        }
        int n_rush = (int)(n0 * cfg::RUSH_SHARE + 0.5);
        if (n_rush < 1) n_rush = 1;
        if (t.rnd > 1 || idx < 0 || idx >= n_rush) ok = 0;
    }
    // the centre: of the two middle columns / rows (even sizes) the nearer one
    int tx = W / 2, ty = H / 2;
    {
        int a = iabs(t.hx - (W - 1) / 2), b = iabs(t.hx - W / 2);
        if (a > W - a) a = W - a;
        if (b > W - b) b = W - b;
        if (a < b) tx = (W - 1) / 2;
        a = iabs(t.hy - (H - 1) / 2);
        b = iabs(t.hy - H / 2);
        if (a > H - a) a = H - a;
        if (b > H - b) b = H - b;
        if (a < b) ty = (H - 1) / 2;
    }
    rush_target = ty * W + tx;
    if (ok) {
        int dx = iabs(t.hx - tx), dy = iabs(t.hy - ty);
        if (dx > W - dx) dx = W - dx;
        if (dy > H - dy) dy = H - dy;
        if (dx + dy > cfg::RUSH_MAXD) ok = 0;
    }
    rush_ok = ok;
    rush_home = t.hg;
}

// kelp and portal edges in view, then the view's move table
inline void Dragon::read_edges(const std::vector<std::string>& L, long long e0, Turn& t) {
    static const std::string OPEN_H = ". . . . . . .", OPEN_V = ". . . . . . . .";
    t.plain = true;
    for (int r = 0; r < 8; r++) {
        const std::string& ln = L[(size_t)(e0 + r)];
        if (ln == OPEN_H) continue;
        t.plain = false;
        auto w = words(ln);
        for (int c = 0; c < 7 && c < (int)w.size(); c++) {
            if (w[(size_t)c] == ".") continue;
            if (r < 7) mem.learn_edge(0, t.wg[r * 7 + c], w[(size_t)c]);
            else mem.learn_edge(0, mem.plain(t.wg[42 + c], 2), w[(size_t)c]);
        }
    }
    for (int r = 0; r < 7; r++) {
        const std::string& ln = L[(size_t)(e0 + 8 + r)];
        if (ln == OPEN_V) continue;
        t.plain = false;
        auto w = words(ln);
        for (int c = 0; c < 8 && c < (int)w.size(); c++) {
            if (w[(size_t)c] == ".") continue;
            if (c < 7) mem.learn_edge(1, t.wg[r * 7 + c], w[(size_t)c]);
            else mem.learn_edge(1, mem.plain(t.wg[r * 7 + 6], 1), w[(size_t)c]);
        }
    }
    if (t.plain) {
        t.mv = VT().step;
        t.ext = VT().border;
        return;
    }
    for (int i = 0; i < 49; i++) {
        t.ext_buf[i] = 0;
        const auto& nl = mem.adj[(size_t)t.wg[i]];
        for (int d = 0; d < 4; d++) {
            int v = nl[(size_t)d];
            if (v >= 0) {
                int j = vi(v);
                v = j >= 0 ? j : -2;
                if (v == -2) t.ext_buf[i] = 1;
            }
            t.mv_buf[i * 4 + d] = v;
        }
    }
    t.mv = t.mv_buf;
    t.ext = t.ext_buf;
}

// the dragons in view
inline void Dragon::read_bodies(const std::vector<std::string>& L, long long b0, long long nbod, Turn& t) {
    for (int i = 0; i < 49; i++) {
        t.occ[i] = 0;
        t.own_face[i] = -1;
    }
    others.clear();
    for (long long k = 0; k < nbod; k++) {
        auto q = words(L[(size_t)(b0 + k)]);
        if (q.size() < 6) continue;
        int did = (int)to_int(q[1]);
        long long c = floormod(to_int(q[2]) - t.hx + 3, W);
        long long r = floormod(to_int(q[3]) - t.hy + 3, H);
        long long ii = r * 7 + c;
        if (ii < 0 || ii >= 49) continue;
        int i = (int)ii;
        int face = q[4].size() == 1 ? dir_index(q[4][0]) : -1;
        if (face < 0) face = 0;
        if (did == id) {
            t.occ[i] = 3;
            t.own_face[i] = face;
            continue;
        }
        int g = t.wg[i];
        others.add(g);
        bool enemy = q[0] != team;
        if (enemy) last_enemy = t.rnd;
        bool head = q[5] == "1";
        if (mem.portal_tiles > 0 && !head && mem.portal_side(g, face)) t.threading.push_back({mem.portal_id(g, face), did});
        t.seg_n.set(did, t.seg_n.get(did, 0) + 1);
        if (VT().border[i]) t.seg_ring.set(did, 1);
        if (head) {
            t.occ[i] = 2;
            t.heads.push_back({i, did, enemy});
            t.head_face.set(did, face);
        } else {
            t.occ[i] = 1;
        }
    }
}

// how often the team meets the enemy (an enemy head within REG_R steps of me or of a friend's head in my view),
// averaged with what the others report; it switches the late clock on
inline void Dragon::contact_update(Turn& t) {
    std::vector<double> heard;
    for (auto& m : t.msgs) {
        uint64_t v;
        double r;
        if (parse_u64(m, v) && decode_contact(v, tbit, r)) heard.push_back(r);
    }
    bool hit = false;
    bool es[49] = {false};
    bool anye = false;
    for (auto& h : t.heads)
        if (h.enemy) {
            es[h.i] = true;
            anye = true;
        }
    if (anye) {
        std::vector<int> starts{HEAD};
        for (auto& h : t.heads)
            if (!h.enemy) starts.push_back(h.i);
        for (int st : starts) {
            int dd[49], q[49], qn = 0;
            for (int i = 0; i < 49; i++) dd[i] = -1;
            dd[st] = 0;
            q[qn++] = st;
            for (int qi = 0; qi < qn; qi++) {
                int u = q[qi];
                if (es[u]) {
                    hit = true;
                    break;
                }
                if (dd[u] >= cfg::REG_R) continue;
                for (int k = u * 4; k < u * 4 + 4; k++) {
                    int x = t.mv[k];
                    if (x >= 0 && dd[x] < 0) {
                        dd[x] = dd[u] + 1;
                        q[qn++] = x;
                    }
                }
            }
            if (hit) break;
        }
    }
    if (reg_born < 0) reg_born = t.rnd;
    long long kc = 0;
    for (int k = 0; k < 196; k++)
        if (t.mv[k] == -1) kc++;
    reg_kelp += kc;
    reg_edges += 196;
    if (hit) reg_hist.push_back(t.rnd);
    double win = cfg::REG_WIN;
    while (!reg_hist.empty() && (double)reg_hist[0] <= t.rnd - win) reg_hist.erase(reg_hist.begin());
    double span = t.rnd - reg_born + 1;
    if (span > win) span = win;
    double hs = 0.0;
    for (double h : heard) hs += h;
    double est = reg_est;
    double prior = est >= 0.0 ? est : (heard.empty() ? 0.0 : hs / (double)heard.size());
    double own = ((double)reg_hist.size() + (win - span) * prior) / win;
    double mean = (own + hs) / (double)(1 + heard.size());
    est = est < 0.0 ? mean : est + cfg::REG_A * (mean - est);
    reg_est = est;
    if (reg_on) {
        if (est < cfg::REG_T / 2) reg_on = false;
    } else if (est >= cfg::REG_T) {
        reg_on = true;
    }
    reg_out = encode_contact(est, tbit);
}

inline double Dragon::early_round() const {
    if (early_on) return early_e;
    if (reg_est >= 0.0 && reg_est < cfg::SEP_T) return cfg::SEP_ROUND;
    return cfg::EARLY;
}

// the round numbers of the late game, for me, this turn
inline void Dragon::set_clock(Turn& t) {
    bool q = t.rnd >= cfg::QUIET_ROUND && t.rnd - last_enemy >= cfg::QUIET_GAP && t.units >= cfg::QUIET_UNITS;
    quiet = q;
    t.late_round = cfg::LATE;
    t.guard_round = cfg::GUARD;
    t.feed_round = cfg::FEED;
    t.beacon_round = cfg::BEACON;
    t.ksplit_round = cfg::KING_SPLIT;
    reg_late = false;
    if (reg_on && t.rnd >= cfg::REG_FROM && (double)reg_kelp >= cfg::REG_OPEN * (double)reg_edges) {
        // the swarms mix here: everything late
        t.late_round = cfg::REG_LATE;
        t.guard_round = cfg::REG_GUARD;
        t.feed_round = cfg::REG_FEED;
        t.beacon_round = cfg::REG_BEACON;
        t.ksplit_round = cfg::REG_KSPLIT;
        if (cfg::BIGCLK && (double)W * H >= cfg::BIGCLK_AREA) {
            t.guard_round -= cfg::BIGCLK;
            t.feed_round -= cfg::BIGCLK;
            t.beacon_round -= cfg::BIGCLK;
            t.ksplit_round -= cfg::BIGCLK;
        }
        reg_late = true;
    } else if (q) {
        if (t.late_round > t.rnd) t.late_round = t.rnd;
        if (t.guard_round > t.rnd) t.guard_round = t.rnd;
        t.feed_round -= cfg::QUIET_FEED_EARLY;
    } else if (t.rnd >= early_round() && (t.units >= cfg::EARLY_UNITS || early_on)) {
        if (!early_on) early_e = early_round();
        early_on = true;
        double e = early_e;
        if (t.guard_round > e) t.guard_round = e;
        if (t.beacon_round > e) t.beacon_round = e;
        if (t.ksplit_round > e) t.ksplit_round = e;
        if (t.feed_round > e + cfg::EARLY_FEED) t.feed_round = e + cfg::EARLY_FEED;
        if (t.late_round > e + cfg::EARLY_LATE) t.late_round = e + cfg::EARLY_LATE;
    }
    beacon_round = t.beacon_round;
    ksplit_round = t.ksplit_round;
    guard_round_eff = t.guard_round;
}

// the beacons heard: the best king and the best enemy king they tell of; returns a value to pass on (0: none)
inline uint64_t Dragon::hear_beacons(const Turn& t) {
    int best_ln[2] = {-1, -1}, best_h[2] = {0, 0}, best_id[2] = {-1, -1}, best_age[2] = {0, 0}, best_g[2] = {-1, -1};
    for (auto& m : t.msgs) {
        uint64_t v;
        Beacon b;
        if (!parse_u64(m, v) || !decode_beacon(v, tbit, b)) continue;
        bool from_queen = b.kind == 0 && (b.id == 0 || b.id == 1);
        if (from_queen && t.rnd - b.age > q_last) q_last = t.rnd - b.age;
        if (qk && b.kind == 0) {
            if (!from_queen || b.x >= W || b.y >= H || b.len < 2) continue;   // the crowned queen only
        } else if (b.x >= W || b.y >= H || b.len < (b.kind == 0 ? cfg::BEACON_LEN : cfg::HUNT_SEG)) continue;
        int k = b.kind;
        if (b.len > best_ln[k] || (b.len == best_ln[k] && (best_id[k] < 0 || b.id < best_id[k]))) {
            best_ln[k] = b.len;
            best_h[k] = b.hungry;
            best_id[k] = b.id;
            best_age[k] = b.age;
            best_g[k] = b.y * W + b.x;
        }
    }
    uint64_t relay = 0;
    int me16 = id & 0xFFFF;
    for (int kind = 0; kind < 2; kind++) {
        if (best_ln[kind] < 0) continue;
        int ln = best_ln[kind], age = best_age[kind], g = best_g[kind];
        if (kind == 0) {
            int did = best_id[0];
            if (king_pos && t.rnd - king_pos->round <= cfg::FEED_FRESH && did != king_id &&
                (king_pos->len > ln || (king_pos->len == ln && king_id < did))) {
                // a better king I know of: pass that one on instead
                int a2 = t.rnd - king_pos->round + 1;
                if (a2 < cfg::BEACON_RELAY && !relay)
                    relay = encode_beacon(W, king_pos->tile, king_pos->len, a2, 0, king_hungry > 0 ? king_hungry : 0,
                                          king_id, tbit);
                continue;
            }
            king_id = did;
            if (!king_seen || ln >= king_seen->len || t.rnd - king_seen->round > 3)
                king_seen = Sighting{g, t.rnd - age, ln};
            king_pos = Sighting{g, t.rnd - age, ln};
            if (ln >= king_len || t.rnd - king_len_round > 20) {
                king_len = ln;
                king_len_round = t.rnd;
                king_hungry = best_h[0];
            }
            bool mine = did == me16;
            bool beats_me = qk || ln > my_len || (ln == my_len && did < me16);
            if (age < cfg::BEACON_RELAY && beats_me && !mine && !relay)
                relay = encode_beacon(W, g, ln, age + 1, 0, best_h[0], did, tbit);
        } else {
            if (!prey || ln >= prey->len || t.rnd - prey->round > 3) prey = Sighting{g, t.rnd - age, ln};
            if (age < cfg::BEACON_RELAY && !relay) relay = encode_beacon(W, g, ln, age + 1, 1, 0, 0, tbit);
        }
    }
    return relay;
}

// who is the team's king (the longest, by beacon or in view), and am I it?
inline void Dragon::find_king(Turn& t) {
    int kid_ = -1;
    if (king_pos && t.rnd - king_pos->round <= cfg::KING_FRESH) kid_ = king_id;
    int me16 = id & 0xFFFF;
    int fm = friend_max;
    if (t.rnd - friend_max_round > 40) fm = 0;
    int kb = cfg::GUARD_KING - 1;
    king_i = -1;
    const std::optional<Sighting> before = king_pos;
    for (auto& h : t.heads) {
        if (h.enemy) continue;
        int n = t.seg_n.get(h.id, 0);
        if (n >= fm) {
            fm = n;
            friend_max_round = t.rnd;
        }
        if (h.id == 0 || h.id == 1) q_last = t.rnd;
        if (qk) {
            // the crowned queen in view: that is where the king is
            if (h.id == 0 || h.id == 1) {
                int kl = n;
                if (t.seg_ring.has(h.id) && before && t.rnd - before->round <= cfg::KING_FRESH && before->len > n)
                    kl = before->len;
                king_i = h.i;
                king_id = h.id;
                kid_ = h.id;
                king_seen = Sighting{t.wg[h.i], t.rnd, kl};
                king_pos = Sighting{t.wg[h.i], t.rnd, kl};
            }
            continue;
        }
        if (kid_ >= 0) {
            if ((h.id & 0xFFFF) == kid_) {
                // the king itself is in view: that is where it is
                king_i = h.i;
                king_seen = Sighting{t.wg[h.i], t.rnd, n};
                if (!t.seg_ring.has(h.id)) king_pos = Sighting{t.wg[h.i], before->round, n};
                else king_pos = Sighting{t.wg[h.i], before->round, before->len > n ? before->len : n};
            }
        } else if (n > kb) {
            kb = n;
            king_seen = Sighting{t.wg[h.i], t.rnd, n};
        }
    }
    friend_max = fm;
    const bool lead_plain = t.len >= fm &&
                            (kid_ < 0 || kid_ == me16 || king_pos->len < t.len || (king_pos->len == t.len && me16 < kid_));
    bool lead_ = qk ? queen : lead_plain;
    lead_long = qk && !queen && lead_plain;
    lead = lead_;
    kid = kid_;
    t.kid = kid_;
    t.lead = lead_;
}

// my body: exact from my own moves; a new process follows the facings it can see
inline void Dragon::track_body(Turn& t) {
    auto follow_facings = [&](int cur) {
        bool used[49] = {false};
        for (int g : trail) {
            int i = vi(g);
            if (i >= 0) used[i] = true;
        }
        while ((int)trail.size() < t.len) {
            int nxt = -1;
            for (int d = 0; d < 4; d++) {
                int n = t.mv[cur * 4 + d];
                if (n >= 0 && !used[n] && t.own_face[n] >= 0 && t.mv[n * 4 + t.own_face[n]] == cur) {
                    nxt = n;
                    break;
                }
            }
            if (nxt < 0) break;
            used[nxt] = true;
            trail.push_back(t.wg[nxt]);
            cur = nxt;
        }
    };
    if (trail.empty()) {
        trail.push_back(t.hg);
        // (a fresh process: follow every facing in view, however long)
        int cur = HEAD;
        bool used[49] = {false};
        used[HEAD] = true;
        while (true) {
            int nxt = -1;
            for (int d = 0; d < 4; d++) {
                int n = t.mv[cur * 4 + d];
                if (n >= 0 && !used[n] && t.own_face[n] >= 0 && t.mv[n * 4 + t.own_face[n]] == cur) {
                    nxt = n;
                    break;
                }
            }
            if (nxt < 0) break;
            used[nxt] = true;
            trail.push_back(t.wg[nxt]);
            cur = nxt;
        }
    } else if (trail[0] != t.hg) {
        if (planned && !plan.empty() && plan.back() == t.hg) {
            for (int g : plan) trail.insert(trail.begin(), g);
        } else {
            trail.insert(trail.begin(), t.hg);
        }
    }
    if ((int)trail.size() < t.len) {
        int cur = vi(trail.back());
        if (cur >= 0) follow_facings(cur);
    }
    if ((int)trail.size() > t.len) trail.resize((size_t)(t.len > 0 ? t.len : 0));
    planned = false;
    plan.clear();
    t.tn = (int)trail.size();
    for (int i = 0; i < 49; i++) t.freeat[i] = t.occ[i] ? BLOCKED : 0;
    body_at.clear();
    for (int k = 0; k < t.tn; k++) {
        int g = trail[(size_t)k];
        int when = t.len - k + 1;
        body_at.set(g, when);
        int i = vi(g);
        if (i >= 0) t.freeat[i] = when;
    }
    if (t.tn < t.len) {
        // my segments in view that are not on the trail I know
        int tu = t.len - t.tn + 1;
        for (int i = 0; i < 49; i++)
            if (t.occ[i] == 3 && !body_at.has(t.wg[i])) body_at.set(t.wg[i], tu);
    }
}

// enemy heads in view: what they can reach before my next turn, priced by what I would lose there
inline void Dragon::assess_threats(Turn& t) {
    for (int i = 0; i < 49; i++) {
        t.tpen[i] = 0.0;
        t.treach[i] = 0;
    }
    t.n_en = 0;
    for (auto& h : t.heads)
        if (h.enemy) t.n_en++;
    t.n_fr = (int)t.heads.size() - t.n_en;
    vis_n = t.seg_n;
    t.qsafe = qs ? cfg::Q_SAFE : (queen && cfg::QSAFE_SMALL > 0 ? cfg::QSAFE_SMALL : 0.0);
    int qpass = t.qsafe > 0 ? cfg::Q_ESC : 0;      // the queen counts her own tail tiles as passable
    bool last_round = t.rnd >= 499;
    for (auto& h : t.heads) {
        if (!h.enemy) {
            t.friends.push_back(h.i);
            continue;
        }
        int ln = t.seg_n.get(h.id, 0);
        bool ring = t.seg_ring.has(h.id);
        int est = ln + (ring ? 2 : 0);
        int rl = ln;
        if (t.rnd >= cfg::LATE_KILL) {
            auto it = elen.find(h.id);
            if (!ring) elen[h.id] = {ln, t.rnd};
            else if (it == elen.end() || ln >= it->second.first) elen[h.id] = {ln, t.rnd};
            else if (t.rnd - it->second.second <= cfg::ELEN_MEMORY && it->second.first > rl) rl = it->second.first;
            if (ring) {
                int late = ln * 2;
                if (late > rl) rl = late;
            }
        }
        if (rl > est) est = rl;
        t.enemies.push_back({h.i, h.id, rl, est});
        if (last_round && h.id < id) continue;          // it has moved for the last time
        int reach = ceil_div(est, sdiv) + est - 2;
        if (reach < 1) reach = 1;
        else if (reach > cfg::REACH_CAP) reach = cfg::REACH_CAP;
        double stake = t.len - est;
        if (t.qsafe > 0) stake = t.qsafe;
        else if (stake > 0 && t.n_fr >= t.n_en && t.len <= cfg::THREAT_OUTNUM_MAXLEN) continue;   // our drops
        if (stake <= 0) continue;                        // it loses as much as I do
        int dist[49];
        for (int i = 0; i < 49; i++) dist[i] = -1;
        dist[h.i] = 0;
        int q[49], qn = 0;
        q[qn++] = h.i;
        int er = VT().row[h.i], ec = VT().col[h.i];
        for (int qi = 0; qi < qn; qi++) {
            int u = q[qi];
            int nd = dist[u] + 1;
            for (int k = u * 4; k < u * 4 + 4; k++) {
                int v = t.mv[k];
                if (v < 0 || dist[v] >= 0) continue;
                if (t.occ[v] && !(qpass > 0 && t.occ[v] == 3 && t.freeat[v] <= qpass)) continue;
                dist[v] = nd;
                double f;
                if (nd <= reach) {
                    int dr = VT().row[v] - er, dc = VT().col[v] - ec;
                    if (dr >= -3 && dr <= 3 && dc >= -3 && dc <= 3) {   // it must see my head to aim at it
                        f = nd == 1 ? 1.0 : (nd == 2 ? 0.9 : (nd == 3 ? 0.8 : 0.7));
                        t.treach[v] = 1;
                    } else {
                        f = cfg::KEEP1;
                    }
                } else if (nd == reach + 1) {
                    f = cfg::KEEP1;
                } else {
                    f = cfg::KEEP2;
                }
                double w = stake * f;
                if (w > t.tpen[v]) t.tpen[v] = w;
                if (nd < reach + 2) q[qn++] = v;
            }
        }
    }
    if (cfg::QG2_W > 0 && queen && !qs) {
        double add[49];
        for (int i = 0; i < 49; i++) add[i] = 0.0;
        for (auto& en : t.enemies) {
            int ln = t.seg_n.get(en.id, 0);
            if (ln < 2) ln = 2;
            int rk = ceil_div(ln, sdiv) + ln - 2;          // reach of the length I see
            int re = ceil_div(en.est, sdiv) + en.est - 2;  // reach of the length I estimate
            if (rk < 1) rk = 1;
            if (re < rk) re = rk;
            int lim = (re > rk + 1 ? re : rk + 1);
            if (lim > cfg::QGUARD_CAP) lim = cfg::QGUARD_CAP;
            int dist[49], q[49], qn = 0;
            for (int i = 0; i < 49; i++) dist[i] = -1;
            dist[en.i] = 0;
            q[qn++] = en.i;
            int er = VT().row[en.i], ec = VT().col[en.i];
            for (int qi = 0; qi < qn; qi++) {
                int u = q[qi];
                if (dist[u] >= lim) continue;
                for (int k = u * 4; k < u * 4 + 4; k++) {
                    int v = t.mv[k];
                    if (v < 0 || dist[v] >= 0 || t.occ[v]) continue;
                    dist[v] = dist[u] + 1;
                    q[qn++] = v;
                    int dr = VT().row[v] - er, dc = VT().col[v] - ec;
                    if (dr < -3 || dr > 3 || dc < -3 || dc > 3) continue;   // it has to see my head
                    if (dist[v] <= rk) add[v] += cfg::QG2_W * (1 + rk - dist[v]);
                    else add[v] += cfg::QG2_W * cfg::QG2_HALO;
                }
            }
        }
        for (int i = 0; i < 49; i++)
            if (add[i] > 0.0) {
                if (add[i] > t.tpen[i]) t.tpen[i] = add[i];
                t.treach[i] = 1;
            }
    }
    if (cfg::QGUARD > 0 && queen && !qs) {
        // small maps: every enemy (but their queen) rams our queen at any price if it can reach her: keep her out of
        // each one's ram reach this turn (she moves first in the round)
        for (auto& en : t.enemies) {
            if (en.id == 0 || en.id == 1) continue;
            int budget = ceil_div(en.est, sdiv) + en.est - 2;
            if (budget < 1) budget = 1;
            if (budget > cfg::QGUARD_CAP) budget = cfg::QGUARD_CAP;
            int dist[49], q[49], qn = 0;
            for (int i = 0; i < 49; i++) dist[i] = -1;
            dist[en.i] = 0;
            q[qn++] = en.i;
            int er = VT().row[en.i], ec = VT().col[en.i];
            for (int qi = 0; qi < qn; qi++) {
                int u = q[qi];
                if (dist[u] >= budget) continue;
                for (int k = u * 4; k < u * 4 + 4; k++) {
                    int v = t.mv[k];
                    if (v < 0 || dist[v] >= 0 || t.occ[v]) continue;
                    dist[v] = dist[u] + 1;
                    q[qn++] = v;
                    int dr = VT().row[v] - er, dc = VT().col[v] - ec;
                    if (dr < -3 || dr > 3 || dc < -3 || dc > 3) continue;   // it has to see my head
                    if (cfg::QGUARD > t.tpen[v]) t.tpen[v] = cfg::QGUARD;
                    t.treach[v] = 1;
                }
            }
        }
    }
}

}  // namespace v30
