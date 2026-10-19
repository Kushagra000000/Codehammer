// v30 map memory: what one dragon knows of the torus - when each tile was last seen, when a pearl is due where,
// kelp and portal edges (with the portals' far sides once both ends are known), and the dead-end lanes of the
// known map.
#pragma once

#include "core.hpp"

namespace v30
{

    struct MapMemory
    {
        int W = 0, H = 0, N = 0;
        std::vector<int> seen; // round last seen; NEVER_SEEN; DEAD_TILE
        std::vector<int> due;  // round a pearl is / will be there (valid where has_due)
        std::vector<char> has_due;
        std::vector<int> fount_seen;                                 // first round of the current run of "pearl, countdown <= 2" looks (-1: none)
        std::vector<char> fountain;                                  // seen holding a pearl that is due again at once, twice, 3+ rounds apart
        std::vector<char> fastbed;                                   // v40.1.0 FARM: a bed seen to refill within FARM_GAP rounds
        std::vector<int> last_cd;                                    // v40.1.0 FARM: the countdown last seen on the tile (-1: none)
        std::vector<std::array<int, 4>> adj;                         // neighbour tile, -1 kelp, -3 portal with an unseen far side
        std::vector<char> edge_seen;                                 // [kind * N + tile], kind 0 north edge, 1 west edge
        std::map<int, std::vector<std::pair<int, int>>> portal_ends; // portal id -> [(kind, tile)]
        std::vector<std::array<int, 4>> portal_at;                   // tile -> portal id across each side (-1: none)
        std::vector<char> any_portal_at;
        int portal_tiles = 0;
        std::map<int, int> portal_busy; // portal id -> round until which a dragon is threading through
        bool edges_changed = false;     // a kelp or portal edge was learnt (routes may be stale)
        // dead-end lanes of the known map: depth = steps in from the network (0: not in a lane)
        std::vector<int> lane_depth, lane_deg, lane_q;
        std::vector<char> lane_in;

        void init(int w, int h)
        {
            W = w;
            H = h;
            N = w * h;
            seen.assign((size_t)N, NEVER_SEEN);
            due.assign((size_t)N, 0);
            has_due.assign((size_t)N, 0);
            fount_seen.assign((size_t)N, -1);
            fountain.assign((size_t)N, 0);
            fastbed.assign((size_t)N, 0);
            last_cd.assign((size_t)N, -1);
            adj.resize((size_t)N);
            for (int g = 0; g < N; g++)
                for (int d = 0; d < 4; d++)
                    adj[(size_t)g][(size_t)d] = plain(g, d);
            edge_seen.assign((size_t)(2 * N), 0);
            portal_at.assign((size_t)N, {-1, -1, -1, -1});
            any_portal_at.assign((size_t)N, 0);
        }

        int plain(int g, int d) const
        {
            switch (d)
            {
            case 0:
                return g >= W ? g - W : g - W + N;
            case 1:
                return (g % W) + 1 < W ? g + 1 : g + 1 - W;
            case 2:
                return g + W < N ? g + W : g + W - N;
            default:
                return g % W ? g - 1 : g - 1 + W;
            }
        }
        int wrap_dist(int a, int b) const
        {
            int dx = iabs(a % W - b % W), dy = iabs(a / W - b / W);
            if (dx > W - dx)
                dx = W - dx;
            if (dy > H - dy)
                dy = H - dy;
            return dx + dy;
        }
        bool pearl_known(int g) const { return has_due[(size_t)g] != 0; }
        void set_due(int g, int r)
        {
            due[(size_t)g] = r;
            has_due[(size_t)g] = 1;
        }
        void drop_due(int g) { has_due[(size_t)g] = 0; }
        int portal_id(int g, int d) const { return (g >= 0 && g < N) ? portal_at[(size_t)g][(size_t)d] : -1; }
        bool portal_side(int g, int d) const { return portal_id(g, d) >= 0; }
        bool has_portal_tile(int g) const { return g >= 0 && g < N && any_portal_at[(size_t)g]; }

        void mark_portal(int g, int d, int pid)
        {
            if (!any_portal_at[(size_t)g])
            {
                any_portal_at[(size_t)g] = 1;
                portal_tiles++;
            }
            portal_at[(size_t)g][(size_t)d] = pid;
        }

        // one non-open edge of the view: kind 0 = the north edge of g, kind 1 = the west edge of g; tok "w" is kelp,
        // a number is a portal
        void learn_edge(int kind, int g, const std::string &tok)
        {
            int key = kind * N + g;
            if (edge_seen[(size_t)key])
                return;
            edge_seen[(size_t)key] = 1;
            edges_changed = true;
            int d_out = kind == 0 ? 0 : 3; // leaving g across the edge
            int d_in = BACK[d_out];
            int other = plain(g, d_out);
            if (tok == "w")
            {
                adj[(size_t)g][(size_t)d_out] = -1;
                adj[(size_t)other][(size_t)d_in] = -1;
                return;
            }
            int pid = (int)to_int(tok);
            auto &ends = portal_ends[pid];
            bool dup = false;
            for (auto &e : ends)
                if (e.first == kind && e.second == g)
                    dup = true;
            if (!dup)
                ends.push_back({kind, g});
            mark_portal(g, d_out, pid);
            mark_portal(other, d_in, pid);
            if (ends.size() < 2)
            {
                adj[(size_t)g][(size_t)d_out] = -3;
                adj[(size_t)other][(size_t)d_in] = -3;
                return;
            }
            // both ends known: crossing the edge at one end comes out across the edge at the other
            int a = ends[0].second, b = ends[1].second;
            adj[(size_t)a][(size_t)d_out] = plain(b, d_out);
            adj[(size_t)plain(a, d_out)][(size_t)d_in] = b;
            adj[(size_t)b][(size_t)d_out] = plain(a, d_out);
            adj[(size_t)plain(b, d_out)][(size_t)d_in] = a;
        }

        // Peel the known map: a seen tile with at most one way on is a dead end; take it away and repeat. What is
        // taken hangs off the rest as trees; depth counts steps in from the tile a tree hangs off.
        void build_lanes()
        {
            lane_deg.assign((size_t)N, 0);
            lane_depth.assign((size_t)N, 0);
            lane_in.assign((size_t)N, 0);
            lane_q.clear();
            for (int g = 0; g < N; g++)
            {
                int k = 0;
                for (int d = 0; d < 4; d++)
                    if (adj[(size_t)g][(size_t)d] != -1)
                        k++;
                lane_deg[(size_t)g] = k;
                if (k <= 1 && seen[(size_t)g] != NEVER_SEEN)
                {
                    lane_in[(size_t)g] = 1;
                    lane_q.push_back(g);
                }
            }
            for (size_t qi = 0; qi < lane_q.size(); qi++)
                for (int v : adj[(size_t)lane_q[qi]])
                {
                    if (v < 0 || lane_in[(size_t)v])
                        continue;
                    if (--lane_deg[(size_t)v] <= 1 && seen[(size_t)v] != NEVER_SEEN)
                    {
                        lane_in[(size_t)v] = 1;
                        lane_q.push_back(v);
                    }
                }
            if ((int)lane_q.size() >= N)
            {
                lane_in.assign((size_t)N, 0);
                return;
            }
            std::vector<int> q;
            for (int g : lane_q)
                for (int v : adj[(size_t)g])
                    if (v >= 0 && !lane_in[(size_t)v])
                    {
                        lane_depth[(size_t)g] = 1;
                        q.push_back(g);
                        break;
                    }
            for (size_t qi = 0; qi < q.size(); qi++)
            {
                int u = q[qi];
                for (int v : adj[(size_t)u])
                    if (v >= 0 && lane_in[(size_t)v] && lane_depth[(size_t)v] == 0)
                    {
                        lane_depth[(size_t)v] = lane_depth[(size_t)u] + 1;
                        q.push_back(v);
                    }
            }
        }
    };

} // namespace v30