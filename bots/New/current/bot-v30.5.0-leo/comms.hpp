// v30 team signals, one 64-bit sonar value a turn (the same value sent in all four directions).
//   beacon:   x(6) y(6) len(8) hungry(1) age(3) kind(1) starving(1) id(16) check(16) team(1) danger(1)
//             kind 0: our king (the longest), kind 1: a long enemy worth hunting
//   contact:  5 << 60 | team << 59 | rate(12) << 47 | check(16) << 31  - my estimate of how often the team meets
//             the enemy (it moves the late-game clock)
#pragma once

#include "core.hpp"

namespace v30 {

struct Beacon {
    int x = 0, y = 0, len = 0, hungry = 0, age = 0, kind = 0, id = 0;   // hungry: bit 0 hungry, 1 starving, 2 danger
};

inline uint64_t beacon_check(uint64_t x, uint64_t y, uint64_t L, uint64_t h, uint64_t A, uint64_t K, uint64_t S,
                             uint64_t D, uint64_t tm, uint64_t Dg) {
    return ((x * 73 + y * 151 + L * 31 + h * 5 + A * 17 + K * 91 + S * 3 + D * 7 + tm * 11 + Dg * 13 + 0x5A) *
            0x9E3779B1ULL) & 0xFFFF;
}

inline uint64_t encode_beacon(int W, int g, long long len, long long age, int kind, long long hungry, long long id,
                              int tbit) {
    uint64_t x = (uint64_t)floormod(g, W), y = (uint64_t)floordiv(g, W);
    if (age > 7) age = 7;
    if (len > 255) len = 255;
    id &= 0xFFFF;
    uint64_t S = (uint64_t)((hungry >> 1) & 1), Dg = (uint64_t)((hungry >> 2) & 1), Hh = (uint64_t)(hungry & 1);
    uint64_t L = (uint64_t)len, A = (uint64_t)age, K = (uint64_t)kind, D = (uint64_t)id, tm = (uint64_t)tbit;
    uint64_t chk = beacon_check(x, y, L, Hh, A, K, S, D, tm, Dg);
    return x | (y << 6) | (L << 12) | (Hh << 20) | (A << 21) | (K << 24) | (S << 25) | (D << 26) | (chk << 42) |
           (tm << 58) | (Dg << 59);
}

inline bool decode_beacon(uint64_t v, int tbit, Beacon& b) {
    if (v >= (1ULL << 60)) return false;
    uint64_t x = v & 63, y = (v >> 6) & 63, L = (v >> 12) & 255, h = (v >> 20) & 1, A = (v >> 21) & 7, K = (v >> 24) & 1,
             S = (v >> 25) & 1, D = (v >> 26) & 0xFFFF, tm = (v >> 58) & 1, Dg = (v >> 59) & 1;
    if ((int)tm != tbit) return false;
    if (beacon_check(x, y, L, h, A, K, S, D, tm, Dg) != ((v >> 42) & 0xFFFF)) return false;
    b.x = (int)x;
    b.y = (int)y;
    b.len = (int)L;
    b.hungry = (int)(h | (S << 1) | (Dg << 2));
    b.age = (int)A;
    b.kind = (int)K;
    b.id = (int)D;
    return true;
}

// right of way: 6 << 60 | team << 59 | flags(2) << 57 | len(8) << 49 | id(16) << 33 | check(16) << 17
//   flags bit 0: I am our queen, bit 1: I am boxed in
inline uint64_t row_check(uint64_t id, uint64_t len, uint64_t fl, uint64_t tm) {
    return ((id * 131 + len * 31 + fl * 7 + tm * 13 + 0x3C1) * 0x9E3779B1ULL) & 0xFFFF;
}

inline uint64_t encode_row(int tbit, long long id, long long len, int flags) {
    if (len > 255) len = 255;
    uint64_t I = (uint64_t)(id & 0xFFFF), L = (uint64_t)len, F = (uint64_t)(flags & 3), T = (uint64_t)tbit;
    return (6ULL << 60) | (T << 59) | (F << 57) | (L << 49) | (I << 33) | (row_check(I, L, F, T) << 17);
}

inline bool decode_row(uint64_t v, int tbit, int& id, int& len, int& flags) {
    if ((v >> 60) != 6ULL || (int)((v >> 59) & 1ULL) != tbit) return false;
    uint64_t F = (v >> 57) & 3, L = (v >> 49) & 255, I = (v >> 33) & 0xFFFF;
    if (((v >> 17) & 0xFFFF) != row_check(I, L, F, (uint64_t)tbit)) return false;
    id = (int)I;
    len = (int)L;
    flags = (int)F;
    return true;
}

inline long long contact_check(long long q, int tbit) { return (q * 40503LL + (long long)tbit * 7919LL + 0x2D1FLL) & 0xFFFFLL; }

inline uint64_t encode_contact(double rate, int tbit) {
    long long q = (long long)(rate * 8192.0 + 0.5);
    if (q > 4095) q = 4095;
    return (5ULL << 60) | ((uint64_t)tbit << 59) | ((uint64_t)q << 47) | ((uint64_t)contact_check(q, tbit) << 31);
}

inline bool decode_contact(uint64_t v, int tbit, double& rate) {
    if ((v >> 60) != 5ULL || (int)((v >> 59) & 1ULL) != tbit) return false;
    long long q = (long long)((v >> 47) & 4095ULL);
    if ((long long)((v >> 31) & 0xFFFFULL) != contact_check(q, tbit)) return false;
    rate = (double)q / 8192.0;
    return true;
}

}  // namespace v30
