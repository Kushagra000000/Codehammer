// bot-v30.0.0-leo - Codehammer 40K.
//
// A swarm economy: dragons split early and often, spread out over the food, trade heads only where it pays, keep
// clear of longer enemies, and late in the game the team builds one long king - the short ones guard it and feed
// it their bodies. On big maps the queen plays safe near home. One process per dragon; the team talks through a
// 64-bit sonar beacon (the king's whereabouts, long enemies, the team's contact rate).
//
// Files: core.hpp (tables, containers), mapmem.hpp (map memory, portals, lanes), comms.hpp (sonar values),
// tuning.hpp (every constant), dragon.hpp (state), perceive.hpp (reading a turn, clocks, king, body, threats),
// search.hpp (survival and room searches), tactics.hpp (rams, feeding, splits, sprints), navigate.hpp (routes),
// decide.hpp (the turn).

#include "decide.hpp"

namespace {

std::string read_line() {
    static char buf[4096];
    std::string out;
    while (fgets(buf, sizeof buf, stdin) != nullptr) {
        out += buf;
        if (!out.empty() && out.back() == '\n') break;
    }
    return out;
}

std::string chomp(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

}  // namespace

int main() {
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    int id = 0, w = 0, h = 0, limit = 64;
    std::string team = "A";
    std::string line = read_line();
    while (!line.empty() && line.rfind("ROUND", 0) != 0) {
        auto p = v30::words(line);
        if (p.size() >= 2) {
            if (p[0] == "ID") id = (int)v30::to_int(p[1]);
            else if (p[0] == "TEAM") team = p[1];
            else if (p[0] == "MAP" && p.size() >= 3) {
                w = (int)v30::to_int(p[1]);
                h = (int)v30::to_int(p[2]);
            } else if (p[0] == "UNIT_LIMIT") limit = (int)v30::to_int(p[1]);
        }
        line = read_line();
    }
    if (w <= 0 || h <= 0) return 1;
    v30::Dragon me(id, team, w, h, limit);
    while (true) {
        while (line == "\n" || line == "\r\n") line = read_line();
        if (line.empty() || line[0] == 'E') return 0;       // end of input / ENDGAME
        std::vector<std::string> L{line, read_line(), read_line(), read_line(), read_line()};
        long long nm = v30::tail_int(chomp(L[4]));
        for (long long k = 0; k < nm; k++) L.push_back(read_line());
        std::string nxt = read_line();                      // "ECHOES ..." or the first tile
        L.push_back(nxt);
        int more = nxt.rfind("ECHOES", 0) == 0 ? 49 : 48;
        for (int k = 0; k < more; k++) L.push_back(read_line());
        std::string hdr = read_line();
        L.push_back(hdr);
        long long nb = v30::tail_int(chomp(hdr));
        for (long long k = 0; k < nb + 15; k++) L.push_back(read_line());
        for (auto& s : L) s = chomp(s);
        // the protocol line the official template ends every turn with: without it the engine drops our sonar
        std::string reply = me.turn(L) + "\nPROTOCOL 3\nENDTURN\n";
        fwrite(reply.data(), 1, reply.size(), stdout);
        fflush(stdout);
        line = read_line();
    }
}
