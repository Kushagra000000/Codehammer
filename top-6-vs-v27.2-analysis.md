# Top_6_bot compared with bot-v27.2.0-leo

**7 October 2026 · Read-only analysis of the supplied defaults.** Top_6’s header identifies it as **v3.9-qual**. Its many disabled experiments are excluded. Your reported performance advantage is the context; the strengths below are supported by code, with no new head-to-head matches in this review.

**Source location:** the reference folder was renamed from `Top_6_bot` to `Top_20_bot`; its source hash is unchanged. “Top_6” below retains the original label.

## 1. How the other team plays

**Top_6 grows a swarm, exploits profitable food and attacks, then turns some of that population into length for the longest known friendly dragon.** It adjusts this transition using enemy contact, population and prolonged peace. Its “king” is an elected growth target, not a separate game unit and not necessarily the queen.

### Game phases

These branches overlap: the contact regime takes precedence over quiet mode, which takes precedence over population-triggered early conversion. Individual dragons may switch at different times because their observations differ. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2867)

| Phase / trigger | Top_6 behavior |
|---|---|
| **Opening, roughly 0–50** | Eat and split from length four. Selected early dragons starting in poor water rush toward the center until round 40 or until that target has been seen. Below 1,100 tiles, a separate exploration bias lasts until round 50 and turns off when enough of the center is known barren. |
| **Ordinary expansion** | Continue food gathering, splitting, profitable rams, exploration and lane harvesting. Around the unit cap, long dragons can stop splitting early: from round 100, at `limit − 12` units. |
| **Normal consolidation** | Guard from 300, feed from 330, elected king stops splitting from 350, other sufficiently long dragons retain from 380; ordinary splitting ends at 440. |
| **Peaceful / separated games** | From 150, ≥12 units and 60 rounds without seeing an enemy segment activate guarding/late behavior immediately; feeding threshold becomes 280. Alternatively, ≥30 units can trigger conversion at 260, or 200 with very low contact; feeding follows 20 rounds later. |
| **Contact-heavy games** | From 260, sufficient reachable enemy contact and observed kelp delay guards to 440 and feeding to 450. King retention starts at 370; other eligible workers can keep splitting until 499. |
| **Finish** | From 460, dragons of length ≥12 increase safety weighting. At 499, threat handling accounts for enemies that already took their final turn. |

Opening: [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2659) and [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:6140). Split/finish rules: [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5111) and [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4108).

### Dragon roles

- **Workers/explorers:** the default role; collect pearls, spread, reproduce and take worthwhile fights. Routine splits roughly halve a dragon while population is at most half the cap, then usually detach two segments.
- **Lane harvesters and replacement children:** on maps below 1,100 tiles, travel inward without ordinary splitting; when food ahead runs out, reverse-split so most body mass leaves from the old tail. An outward-moving dragon can leave a two-segment child to revisit an unoccupied, productive lane. This is a temporary behavior, not a permanently assigned caste. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3470)
- **King / length retainers:** the longest known ally wins, with lower ID breaking ties. The king eventually stops splitting; later, dragons at least half the known maximum can also retain length. There is no guaranteed separate “second king.” [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2148) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5111)
- **Guards/feeders:** eligible dragons approach the king, reduce competition for its nearby pearls, and may die on their own neck to leave food. Feeding requires ≥4 friendly units, no visible enemy head, and an estimate that the recipient can collect useful drops. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3394) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5286)
- **Hunters:** short dragons follow recent reports of large enemies from round 350; nearby enemy queens can be attacked earlier. The separate `ASSN` scouting and queen-call systems are disabled. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3969)
- **Queen:** below 1,100 tiles she largely follows ordinary logic, with lane-entry and teammate-exit protections. At ≥1,100 tiles, dedicated safety activates: retain five segments on routine splits, stop ordinary queen splitting at 20 units, heavily price enemy reach, avoid pockets, use emergency sprints, and prefer staying within eight toroidal steps of her birthplace. **She is not automatically elected king.** [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2471) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3544) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4281)

## 2. Individual and collective behavior: differences from ours

| Mechanism | Top_6 | Our v27.2.0-leo |
|---|---|---|
| **Map memory** | Private flat `W × H` arrays: `seen`, pearl due-times in `hot`, four neighbors per tile, special edges/portal pairs, own trail and limited occupancy memory. Unknown terrain initially has ordinary wraparound neighbors. | Private `CellMemory` grid, body-release estimates, learned pearl-bed quality, portal graph and shared exploration coverage. Neither bot receives an omniscient map. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:1357) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:3069) |
| **Sonar / collective knowledge** | Sends one prioritized 64-bit packet in all four directions: king or large-enemy beacon first, otherwise contact-rate estimate. King packets carry location, length, ID, age and hunger/starvation. Pearl gossip is **off**; no active portal-pair or body-handover sharing. | Multiplexes status, grower/enemy reports, king beacons, portal pairs and coverage across directions; split children can receive body handover. Our active protocol shares more map/body information, but lacks their hunger/contact-rate economy signals. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2412) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:1937) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:4205) |
| **Food and multiplication** | Strong immediate-food preference; food value falls with squared arrival time. Falls back to remembered targets/frontiers, with route persistence and target bans. Small-map newborns skip first-turn splitting after round 0. | More elaborate pearl-sequence planning and wall-aware arrival estimates; worker splits normally retain two, cap children below heir length, and reserve emergency slots. We already have exploration, predicted spawns, farming and pocket raids. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4368) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3467) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/route.hpp:241) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1605) |
| **Ordinary death avoidance** | Body-release timing, territory, exits, enemy reach and bounded survival search. Ordinary threat reach caps at six steps; some small dragons accept risk when allies locally match enemies. | Longer body-aware planning, hard/soft reach maps, candidate safety filtering and escape-split selection. These are richer checks, although our earlier audit found specific scoring/survival defects. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3145) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/survival.hpp:165) |
| **Queen safety and longest queen** | Heavy protection and birthplace preference are large-map features. Small-map defaults do not categorically prevent the queen trading or feeding. General boxed-in fallback can spend a turn on `SPLIT 2`. | Queen is valuable on every map from round zero; retains after 75, follows a checked cycle until 400, receives king priority from 380, and has dedicated two-segment waiting. Our design more directly concentrates late growth into the first tiebreak. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5229) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5287) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5717) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1561) |
| **Protecting teammates / queen** | Prices taking friendly exits and adds a queen-exit penalty. Guards primarily support the elected king; this is not a guaranteed physical shield for the queen. | Explicit queen-side/exit protection plus v27.1 teammate-boxing costs; our audit found missed sprint penalties and false positives. Neither scoring system is a universal veto. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4544) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/queen.hpp:401) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/scoring.hpp:242) |
| **Killing enemy queen / trading** | Searches visible sprint paths to the queen; attack entry requires ≥2 friendly dragons. Generic trades are available early, with local-support tests for equal/slightly smaller targets. | Immediate queen-kill shortcut lacks that last-survivor check. Before 50, ordinary workers need ≥3 friendly dragons and an equal-or-longer estimated target; dedicated tactical sprints still start at 50. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3388) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5157) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/queen.hpp:380) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:3683) |
| **Movement speed** | Extends a selected move into free pearl/route steps, including multiple pearls. Paid sprints serve attacks; protected queens get escape sprints. Their routine free-sprint extension is disabled for the protected queen. | Free, route, contested-pearl, attack and valuable-dragon escape sprints compete as candidates; queen cycles can use multiple steps. Both use the initial-length `ceil(L/4)` free allowance. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4904) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4962) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1094) |
| **Portals** | Short dragons tolerate unknown exits cheaply; longer dragons pay more. Remembers a portal being threaded by another body and temporarily penalizes crossing. Known exits receive survival checks. | Shares portal pairs, compares destination resources, prices risk by round/role, discourages loops and recognizes portal rooms. Queen avoids portals after 100 when enough portal-free room exists. There is no universal “always prioritize portals” rule in either. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2854) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4649) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/scoring.hpp:510) |
| **Longest dragon apart from queen** | Normal king election can build a nonqueen directly; other near-maximum dragons may retain. This provides a natural secondary-tiebreak candidate, but can divert food from the queen. | Hash-selected growers and long workers provide candidates before consolidation; queen priority later dominates. Dedicated vice retention is effectively disabled (`VICE_PERCENT=101`). Neither guarantees two independently maintained champions. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5119) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1506) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:291) |

### Short code evidence

**Their adaptable clock, our fixed baseline clock:**

[Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2881)

```cpp
late_round = Pp.REG_LATE;
guard_round = Pp.REG_GUARD;
feed_round = Pp.REG_FEED;
```

[Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:249)

```cpp
constexpr int GATHER_ROUND = 380;     // every non-retaining dragon heads for the king...
constexpr int SACRIFICE_ROUND = 400;  // ...and from here turns itself into the king's pearls
```

Our active `KEEP_ARMY` exception matters: roughly half of eligible nonretaining workers continue harvesting, hunting and splitting during rounds **380–439**, then join feeding at 440. The comments above describe the older baseline. Their contact regime adjusts the broader economic schedule instead of selecting this fixed cohort. [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1619)

**Our late election prioritizes the queen; their default does not force this:**

[Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:599)

```cpp
double Q_KING = 0;
```

[Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/endgame.hpp:80)

```cpp
if (queen_crowned() && king_fresh() && queen != king_.queen)
    better = queen;
```

**Their attack gate avoids our previously reproduced last-worker suicide:**

[Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3388)

```cpp
if (T(Pp.RAM) && !enemies.empty() && ucount >= 2) {
    std::string act;
    if (try_kill(enemies, mv, freeat, length, rnd, hg, occ, (int)friends.size(), act)) return act;
}
```

[Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/queen.hpp:380)

```cpp
if (steps->size() == 1)
    controller_.make_move(steps->front());
else
    controller_.make_moves(*steps);
```

Our excerpt is the execution path after a queen target/path is found; the complete function has no friendly-unit-count guard.

## 3. Where Top_6 has a stronger mechanism

1. **Timing population conversion:** adjusts to fighting versus isolation instead of applying one economic timetable. This can preserve attackers when needed or start length growth earlier.
2. **Repeated lane exploitation:** tracks a lane’s direction, remaining resources and occupancy, and creates a replacement worker deliberately. Our general pocket raids do not implement this complete lifecycle.
3. **Avoiding wasteful sacrifices:** protects the last attacker and requires four units for feeding; evaluates actual alternating body-drop positions, collection distance and remaining time. Our feeding test centers on whether the king can reach the donor’s head area. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5380) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/endgame.hpp:270)
4. **More selective trade evidence:** ordinarily separates observed enemy length used to justify a ram from the larger estimate used for danger. Our early trading uses estimated length. Their late-game rules relax this caution. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3120)
5. **Purpose-built tactical options:** explicit sprint-across-an-enemy’s-only-exit attacks and general boxed-dragon waiting; broader early paid attacks can exploit opportunities our round-50 sprint gate misses. More aggression also spends population and length.

## 4. Where our bot has a stronger mechanism

1. **Queen investment across every map:** permanent valuable status, protected cycles, late crowning and feeding priority fit the queen-first tiebreak more directly.
2. **Team information sharing:** portal topology, exploration coverage, detailed status and split-body handover reduce repeated discovery and newborn uncertainty.
3. **Split safety/capacity:** active newborn-aware parent-exit checks and reserved emergency slots. Their equivalent `SPLIT_PEXIT` experiment and fixed `CAP_FREE` reserve are both off. [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1689) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1729)
4. **Food-route accuracy:** active wall/portal-aware arrival estimates and pearl-sequence search. Their ordinary local rival-food estimate is Manhattan because `FOOD_ETA=0`. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:4352)
5. **More extensive safety planning:** a richer body/route search and role-sensitive threat model; their six-step threat cap and small search budgets trade caution for speed. Greater search complexity also costs CPU and creates more places for implementation bugs.

These advantages do not establish which bot wins a particular map. Our previously confirmed last-worker attack, teammate-boxing, consumed-pearl search and army/feeder issues remain relevant; no fixes were made here.

## 5. Active mechanisms we do not currently implement

- **Shared contact-rate estimates driving the economic timetable.**
- **A complete lane harvesting/replacement-child lifecycle**, beyond our existing pocket raids.
- **A first-turn split hold for small-map newborns after round 0.**
- **King hunger/starvation beacons plus donor-drop collection valuation.**
- **A dedicated sprint generator that crosses an enemy’s sole exit and continues beyond it.** We already score trapping in combat; the dedicated move generator is the difference. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5187) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/scoring.hpp:829)
- **General two-segment waiting for boxed workers**, independent of proving the child escapes. Our dedicated waiting behavior is queen-only; some other splits can incidentally detach two. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5717) [Our code](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/endgame.hpp:619)
- **Explicit queen birthplace-radius preference** on large maps; our cycles provide a different form of locality.

Do not count the other bot’s **queen vault, chase/cooldown, free-step-threshold pearl claims, symmetry inference, pearl gossip, special assassin scouting or queen “never-trap” system** as advantages: those switches are disabled in this supplied version. [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:692) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:267) [Top code](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:304)

### Official format reference

The official repository confirms `.map` is a text configuration containing dimensions, pearl-spawn intervals, edges/portal IDs and starting bodies. `.replay` uses the Cap’n Proto schema: original map text, bot names, seed, event stream, result and format version. Replay events include actions, movement/splits/deaths, sonar and instruction usage where recorded. These files support later behavioral validation; bots themselves operate on partial observations. Sources: [map parser](https://github.com/unswcpmsoc/unswbc/blob/79e22e7950e334711574bcc1166b3973153ccc4f/engine/src/config.cc), [replay schema](https://github.com/unswcpmsoc/unswbc/blob/79e22e7950e334711574bcc1166b3973153ccc4f/engine/replay.capnp), [replay loader](https://github.com/unswcpmsoc/unswbc/blob/79e22e7950e334711574bcc1166b3973153ccc4f/packages/visualiser/src/replay/loader.ts). Winning-condition reference: [official rules](https://game.battlecode.au/docs/structure).

## Reference code appendix

These excerpts are copied from the analyzed sources. Expand a topic to read the implementation alongside the comparison; surrounding control flow is omitted where indicated by the cited line range.

<details>
<summary>Queen protection: map-dependent in Top_6, always valuable in ours</summary>

**Top_6: queen safety gate** — [main.cpp, lines 2468–2476](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2468)

```cpp
const Params& Pp = P;
{
    int rnd0 = (int)pyint(tail_from(at(L, 0), 6));
    qs = is_queen && ((double)W * H >= Pp.Q_AREA || (T(Pp.Q_LATE) && rnd0 >= Pp.Q_LATE));
}
surv_seen = qs && T(Pp.Q_SURV);
if (T(Pp.SURV_SEEN_LEN) && (double)pyint(tail_from(at(L, 2), 7)) <= Pp.SURV_SEEN_LEN)
    surv_seen = true;                   // (3 Oct, SURV_SEEN_LEN) short dragons too
qnr = is_queen && ((qs && Pp.Q_SAFE > 0) || T(Pp.Q_NORAM_ALL));
```

**Ours: role classification** — [main.cpp, lines 1590–1597](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1590)

```cpp
void compute_roles()
{
    retaining_now_ = compute_retaining();
    // v23: the queen always, from round 0: her length is the first tiebreak, so she gets
    // the king's caution (reach threats, crowds, unseen heads, no blind portals, no hunts
    // or trades) even while she seeds the swarm.
    valuable_now_ = retaining_now_ || controller_.get_length() >= Config::VALUABLE_LENGTH || is_queen();
}
```

</details>

<details>
<summary>Small-map newborn hold and lane harvesting</summary>

`LANE_EARLY=0` and `LANE_RELAY_F=2` in the supplied defaults. These blocks supplement our existing pocket-raiding comparison.

**Top_6: newborn hold** — [main.cpp, lines 3467–3469](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3467)

```cpp
// (4 Oct, NB_NOSPLIT) a newborn's tail touches its parent: a child of mine would face straight into it
const bool lane_map = !(T(Pp.LANE_AREA) && (double)W * H >= Pp.LANE_AREA);   // (4 Oct, LANE_AREA)
const bool nb_hold = T(Pp.NB_NOSPLIT) && lane_map && turns_played == 0 && rnd > 0;
```

**Top_6: stop splitting inward and reverse after depletion** — [main.cpp, lines 3489–3497](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3489)

```cpp
if (lane_dir == 1) {
    bool busy = false;
    lane_ahead = lane_food(hg, 0, &busy, 60);
    lane_nosplit = true;    // walking in: the head part would go on deeper, shorter
    if (length >= 4 && ucount < limit && !nb_hold && (lane_ahead == 0 || (T(Pp.LANE_EARLY) && lane_ahead >= 2))) {
        // nothing (or, LANE_EARLY, enough for the front two) left ahead: the rest walks out
        std::string alt;
        if (reverse_split(length, ucount, tn, mv, occ, alt)) return lane_note(alt, "lane-in");
    }
```

**Top_6: replacement-child gate and parent-exit check** — [main.cpp, lines 3505–3518](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:3505)

```cpp
const int fb = lane_food(tl, 0, &busy, 60);
if (T(Pp.LANE_RELAY) && !busy && fb >= Pp.LANE_RELAY_F && ucount < limit && !nb_hold) {
    // I stand still this turn: my head needs a way on that nobody can take first (as SPLIT_SAFE)
    int safe = 0, free = 0;
    for (int d = 0; d < 4; d++) {
        const int v = mv[d + 96];
        if (v == -2) safe++;
        else if (v >= 0 && !occ[v]) {
            free++;
            if (headadj[v] == 0) safe++;
        }
    }
    if (safe > 0 || free >= 2) return lane_note("SPLIT 2", "lane-relay");
}
```

</details>

<details>
<summary>Sonar: prioritized shared packet versus multiple message types</summary>

Top_6 transmits the selected packet in all four directions. Its pearl-gossip feature is disabled; contact-rate reports fill the otherwise unused slot. Our enemy-queen-sharing branch shown below is also disabled by default.

**Top_6: packet priority** — [main.cpp, lines 2414–2418](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:2414)

```cpp
uint64_t b = beacon_out;
if (!b && gossip_out) b = gossip_out;     // (v2.11) no king to tell of: tell what I have seen
gossip_out = 0;
if (!b && reg_out) b = reg_out;           // (v3, REG_R) nothing else to tell: my estimate of the contact rate
reg_out = 0;
```

**Ours: direction-specific packet selection** — [main.cpp, lines 4251–4261](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:4251)

```cpp
if (beacon && (di == beacon_dir ||
               (round >= Config::BEACON_DOUBLE_ROUND && di != (beacon_dir + 3) % 4)))
    controller_.send_sonar(direction, beacon_message());
else if (portal_msg && di == portal_dir)
    controller_.send_sonar(direction, portal_msg);
else if (explore_share && di == explore_dir)
    controller_.send_sonar(direction, explore_message());
else if (enemy_queen_share() && di == (beacon_dir + 3) % 4 && round % 2 == 1)
    controller_.send_sonar(direction, enemy_queen_message()); // v27 idea 3
else
    controller_.send_sonar(direction, m);
```

</details>

<details>
<summary>Sacrifice value and two-segment waiting</summary>

Each excerpt runs only after its enclosing function’s eligibility checks. Top_6’s gain calculation follows drop-position and recipient-reachability checks.

**Top_6: estimated collectible drop gain** — [main.cpp, lines 5432–5436](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5432)

```cpp
    int gain = 1 + (int)fdiv(left - dmin, 2);
    if (gain > ndrops) gain = ndrops;
    if (gain > best_gain) best_gain = gain;
}
if (best_gain < P.FEED_MIN_GAIN) return false;
```

**Top_6: general boxed-dragon wait** — [main.cpp, lines 5717–5721](/Users/kushagra/Desktop/UNSW_battlecode/Top_20_bot/main.cpp:5717)

```cpp
if (length >= 4 && ucount < limit) {
    shed_run = run + 1;
    shed_last = rnd;
    return "SPLIT 2";           // a split is the only way to stand still
}
```

**Ours: dedicated queen wait** — [endgame.hpp, lines 619–629](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/endgame.hpp:619)

```cpp
if (is_queen())
{
    // Splitting consumes the move without moving our head. Do not require a
    // safe child exit or a complete body trace: this buys the queen a turn even
    // when the tail is boxed. She is not an escape remnant handing off her role.
    controller_.output_log("QUEEN_WAIT_SPLIT", child, "keep", L - child);
    controller_.do_split(child);
    send_status(controller_.get_position(), L - child);
    send_body_handover(child);
    return true;
}
```

</details>

<details>
<summary>Our newborn-aware parent escape check</summary>

This is the final part of `split_parent_ok()`: a parent needs one exit away from the child’s reach, or at least two exits within it.

**Ours: account for where the new child could move** — [main.cpp, lines 1704–1713](/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/New/current/bot-v27.2.0-leo/main.cpp:1704)

```cpp
    bool next_to_child = false;
    for (Direction e : Direction::get_direction_list())
    {
        Step const t = step_from(child_head, e);
        if (t.passable && t.destination_known && t.destination == s.destination)
            next_to_child = true;
    }
    ++(next_to_child ? near : far);
}
return far >= 1 || near >= 2;
```

</details>
