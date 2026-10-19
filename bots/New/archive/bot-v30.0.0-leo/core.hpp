// v30 core: tables for the 7x7 view, small containers, integer helpers, the tie-break RNG.
#pragma once

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace v30 {

// ---------------------------------------------------------------- integers
inline long long floordiv(long long a, long long b) {
    long long q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) --q;
    return q;
}
inline long long floormod(long long a, long long b) {
    long long r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
inline int iabs(int a) { return a < 0 ? -a : a; }
inline int ceil_div(int a, int b) { return (int)floordiv((long long)a + b - 1, b); }

// ---------------------------------------------------------------- directions and the view
// directions: 0 N, 1 E, 2 S, 3 W.  View index i = row * 7 + col, my head at 24.
constexpr const char* DIR_CH = "NESW";
constexpr int BACK[4] = {2, 3, 0, 1};
constexpr int STEP_X[4] = {0, 1, 0, -1};
constexpr int STEP_Y[4] = {-1, 0, 1, 0};
constexpr int HEAD = 24;
constexpr int BLOCKED = 9999;            // a tile that does not free up this turn
constexpr int NEVER_SEEN = -100000;
constexpr int DEAD_TILE = 1000000000;    // seen, and no pearl will ever come there in this game
constexpr int NONE = INT_MIN;

// out-of-window step: -2.  kelp: -1.  portal with an unseen far side: -3
struct ViewTables {
    int step[196];       // step[i*4+d]: neighbour view index, -2 = leaves the view
    int border[49];      // 1 on the outer ring
    int row[49], col[49];
    ViewTables() {
        const int dy[4] = {-1, 0, 1, 0};
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 7; c++)
                for (int d = 0; d < 4; d++) {
                    int rr = r + dy[d], cc = c + STEP_X[d];
                    step[(r * 7 + c) * 4 + d] = (rr >= 0 && rr < 7 && cc >= 0 && cc < 7) ? rr * 7 + cc : -2;
                }
        for (int i = 0; i < 49; i++) {
            row[i] = i / 7;
            col[i] = i % 7;
            border[i] = (row[i] == 0 || row[i] == 6 || col[i] == 0 || col[i] == 6) ? 1 : 0;
        }
    }
};
inline const ViewTables& VT() {
    static ViewTables t;
    return t;
}
inline int dy_of(int d) { return d == 0 ? -1 : (d == 2 ? 1 : 0); }
inline int vman(int a, int b) { return iabs(VT().row[a] - VT().row[b]) + iabs(VT().col[a] - VT().col[b]); }

// ---------------------------------------------------------------- tie-break noise (64-bit LCG, seeded per dragon)
struct Noise {
    uint64_t s = 0;
    void seed(int id) { s = (uint64_t)(int64_t)id * 2654435761ULL + 12345ULL; }
    double next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)(s >> 11) * (1.0 / 9007199254740992.0);
    }
};

// ---------------------------------------------------------------- containers over global tiles
struct Marks {                          // a set of tiles, cleared in O(size)
    std::vector<char> on;
    std::vector<int> list;
    void reset(int n) { on.assign((size_t)n, 0); list.clear(); }
    bool has(int g) const { return g >= 0 && g < (int)on.size() && on[(size_t)g]; }
    void add(int g) { if (!on[(size_t)g]) { on[(size_t)g] = 1; list.push_back(g); } }
    void clear() { for (int g : list) on[(size_t)g] = 0; list.clear(); }
    bool empty() const { return list.empty(); }
};

struct TileInts {                       // tile -> int, NONE when absent, cleared in O(size)
    std::vector<int> v;
    std::vector<int> keys;
    void reset(int n) { v.assign((size_t)n, NONE); keys.clear(); }
    int get(int g) const { return (g >= 0 && g < (int)v.size()) ? v[(size_t)g] : NONE; }
    bool has(int g) const { return get(g) != NONE; }
    void set(int g, int x) { if (v[(size_t)g] == NONE) keys.push_back(g); v[(size_t)g] = x; }
    void clear() { for (int g : keys) v[(size_t)g] = NONE; keys.clear(); }
};

struct IdCount {                        // dragon id -> int, for the few dragons in view
    std::vector<std::pair<int, int>> kv;
    void clear() { kv.clear(); }
    const int* find(int k) const { for (auto& e : kv) if (e.first == k) return &e.second; return nullptr; }
    bool has(int k) const { return find(k) != nullptr; }
    int get(int k, int dflt) const { const int* p = find(k); return p ? *p : dflt; }
    void set(int k, int x) { for (auto& e : kv) if (e.first == k) { e.second = x; return; } kv.push_back({k, x}); }
};

struct Sighting {                       // (tile, round, length)
    int tile, round, len;
};

struct HeadSeen {
    int i, id;
    bool enemy;
};

struct Rival {                          // an enemy head in view
    int i, id, len, est;                // len: what I can vouch for; est: what it may be
};

struct Option {                         // a first step and its score
    double s;
    int d, v, g, kind;                  // kind 0: open tile in view, 1: portal to a remembered tile, 2: blind portal
};

// ---------------------------------------------------------------- text
inline bool ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
inline std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0, n = s.size();
    while (i < n) {
        while (i < n && ws(s[i])) i++;
        if (i >= n) break;
        size_t j = i;
        while (j < n && !ws(s[j])) j++;
        out.emplace_back(s, i, j - i);
        i = j;
    }
    return out;
}
inline long long to_int(const std::string& s) {
    long long v = 0;
    bool neg = false;
    size_t i = 0;
    while (i < s.size() && ws(s[i])) i++;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) neg = s[i++] == '-';
    for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; i++) v = v * 10 + (s[i] - '0');
    return neg ? -v : v;
}
inline long long tail_int(const std::string& s) {       // the number after the first word ("LENGTH 7" -> 7)
    size_t p = s.find(' ');
    return p == std::string::npos ? 0 : to_int(s.substr(p + 1));
}
inline int dir_index(char c) { return c == 'N' ? 0 : c == 'E' ? 1 : c == 'S' ? 2 : c == 'W' ? 3 : -1; }

// a sonar value: ok only for a plain unsigned decimal below 2^64
inline bool parse_u64(const std::string& s, uint64_t& out) {
    size_t i = 0, n = s.size();
    while (i < n && ws(s[i])) i++;
    while (n > i && ws(s[n - 1])) n--;
    if (i >= n) return false;
    if (s[i] == '+') i++;
    else if (s[i] == '-') {
        // "-0" is zero; anything else negative is not ours
        bool nz = false;
        for (size_t k = i + 1; k < n; k++) {
            if (s[k] < '0' || s[k] > '9') return false;
            if (s[k] != '0') nz = true;
        }
        if (nz || i + 1 >= n) return false;
        out = 0;
        return true;
    }
    if (i >= n) return false;
    uint64_t v = 0;
    for (; i < n; i++) {
        char c = s[i];
        if (c < '0' || c > '9') return false;
        unsigned d = (unsigned)(c - '0');
        if (v > (UINT64_MAX - d) / 10) return false;
        v = v * 10 + d;
    }
    out = v;
    return true;
}

}  // namespace v30
