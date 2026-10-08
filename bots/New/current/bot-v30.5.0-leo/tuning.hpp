// v30 tuning constants (one place for every number the strategy uses)
#pragma once

namespace v30::cfg {

// ---- splitting and the game clock
constexpr int SPLIT_MIN = 4;              // shortest dragon that splits
constexpr int BUD = 2;                    // child length once the team is past half the unit limit
constexpr int LATE = 380;                 // from here a dragon at least half the king's length stops splitting
constexpr int STOP_SPLIT = 440;
constexpr int BANK = 460;                 // last rounds: long dragons play safe
constexpr int BANK_LEN = 12;
constexpr double BANK_SAFETY = 3.0;
constexpr int GUARD = 300;                // short dragons stay near the king from here
constexpr int FEED = 330;                 // short dragons feed themselves to the king from here
constexpr int BEACON = 300;               // the king announces itself from here
constexpr int KING_SPLIT = 350;           // the king stops splitting from here
constexpr int CAP_MARGIN = 12;            // near the unit cap the longest stop splitting
constexpr int CAP_ROUND = 100;
// early economy: a big team that has not met the enemy starts the late game sooner
constexpr int EARLY = 260;
constexpr int EARLY_UNITS = 30;
constexpr int EARLY_FEED = 20;
constexpr int EARLY_LATE = 20;
constexpr int SEP_ROUND = 200;
constexpr double SEP_T = 0.003;
// nobody around for long: the game goes the distance
constexpr int QUIET_ROUND = 150;
constexpr int QUIET_GAP = 60;
constexpr int QUIET_FEED_EARLY = 50;
constexpr int QUIET_UNITS = 12;
// the contact clock (the team's rate of meeting the enemy)
constexpr int REG_R = 3;
constexpr int REG_WIN = 60;
constexpr double REG_A = 0.2;
constexpr double REG_T = 0.01;
constexpr double REG_OPEN = 0.01;
constexpr int REG_FROM = 260;
constexpr int REG_GUARD = 440;
constexpr int REG_BEACON = 330;
constexpr int REG_FEED = 450;
constexpr int REG_KSPLIT = 370;
constexpr int REG_LATE = 499;
constexpr int REG_STOP = 499;
constexpr int REG_KLEN = 4;

// ---- king, guards, feeding
constexpr int BEACON_LEN = 5;
constexpr int BEACON_RELAY = 3;
constexpr int KING_FRESH = 20;
constexpr int FEED_FRESH = 4;
constexpr int HUNGRY = 30;
constexpr int KING_HUNGRY = 12;
constexpr int GUARD_KING = 10;            // a friend this long (in view) is worth guarding
constexpr int GUARD_MAXLEN = 6;
constexpr double GUARD_W = 0.6;
constexpr double GUARD_FOOD = 0.1;        // a pearl next to the king is the king's
constexpr int GUARD_MEM = 40;
constexpr int GUARD_MEM_BONUS = 8;
constexpr int GUARD_NAV_CAP = 400;
constexpr double KING_STAY_W = 0.3;
constexpr double KING_SPACE = 0.3;
constexpr int FEED_LATE = 440;
constexpr int FEED_DIST = 4;
constexpr double FEED_RATIO = 2.0;
constexpr int FEED_KING_MIN = 4;
constexpr int FEED_KING_MARGIN = 3;
constexpr int FEED_KING_MARGIN_LEN = 6;
constexpr int HUNT_ROUND = 350;
constexpr int HUNT_SEG = 8;
constexpr int HUNT_MAXLEN = 3;
constexpr int HUNT_MEM = 12;

// ---- fighting
constexpr int FIGHT_MAX_LEN = 8;            // trap kills: lengths 3..this
constexpr int RAM_DOWN_MAXLEN = 4;          // a dragon this short rams an enemy one shorter (with friends)
constexpr int EVEN_MARGIN = 0;              // even / down trades need friends in view > enemies - 1 + this
constexpr int REACH_CAP = 6;
constexpr double KEEP1 = 0.35;
constexpr double KEEP2 = 0.2;
constexpr double THREAT_W = 1.5;
constexpr double HUNT_W = 0.5;
constexpr int THREAT_OUTNUM_MAXLEN = 8;
constexpr int LATE_KILL = 440;
constexpr int ELEN_MEMORY = 10;
constexpr int ESC_LEN = 8;
constexpr double ESC_PEN = 12.0;
constexpr int ESC_TAIL = 2;
constexpr int ESC_ROOM = 24;
constexpr int BOXED_REV_AFTER = 2;
constexpr int BOXED_REV_LEN = 10;

// ---- the queen on big maps
constexpr double Q_AREA = 1100;
constexpr double Q_SAFE = 200;
constexpr int Q_SPLIT = 20;
constexpr int Q_KEEP = 5;
constexpr int Q_ESC = 8;
constexpr double Q_ESC_COST = 2.0;
constexpr int Q_JAM = 5;
constexpr double Q_EXIT = 3;
constexpr int Q_HOME = 8;
constexpr double Q_HOME_W = 0.7;

// ---- moving
constexpr double SOFT_W = 4.0;
constexpr double SAFE0 = 1.5;
constexpr double POCKET_L2 = 0.02;
constexpr int SURV_CAP = 80;
constexpr int SURV_NODES = 220;
constexpr int WORK = 550;
constexpr int POCKET_MIN = 2;
constexpr double POCKET_RUN_COST = 2.5;
constexpr int POCKET_MEM_AGE = 30;
constexpr int FOUNT_CD = 2;
constexpr double FOUNT_W = 1.0;
constexpr int FOUNT_PKMIN = 3;
constexpr int FOUNT_MAXLEN = 6;
constexpr double TERR_FOOD = 0.5;
constexpr double TERR_W = 2.0;
constexpr double SPACE_W = 3.0;
constexpr double PORTAL_CLEAR_LONG = 0.4;
constexpr int PORTAL_CLEAR_LEN = 4;
constexpr double NAV_GATE = 0.06;
constexpr int GOAL_BAN = 30;
constexpr int LANE_RELAY_F = 2;
constexpr int LANE_OCC_MEM = 2;
constexpr double LANE_AREA = 1100;

// ---- navigation
constexpr int RUSH_ROUND = 40;
constexpr int RUSH_POOR = 2;
constexpr int RUSH_DUE = 30;
constexpr double RUSH_SHARE = 0.5;
constexpr int RUSH_MAXD = 20;
constexpr double RUSH_W = 3.0;
constexpr int RUSH_CAP = 600;
constexpr int RUSH_WAIT = 2;
constexpr int REVISIT = 70;
constexpr int UNSEEN_COST = 3;
constexpr int EXPLORE_REDRAW = 60;
constexpr int NAV_MAX = 1000;
constexpr int NAV_RATE = 120;
constexpr int NAV_MIN = 200;
constexpr int KING_NAV_CAP = 600;
constexpr int ROUTE_AGE = 15;
constexpr int ROUTE_SLACK = 6;
constexpr double ROUTE_STICK = 2.0;
constexpr int BLIND_COST = 5;
constexpr int PORTAL_STEP = 2;
constexpr double PORTAL_HOLD_W = 0.3;
constexpr int OE_ROUND = 50;              // the opening's pull to the centre (small maps)
constexpr double OE_W = 18;
constexpr double OE_R = 0.5;
constexpr int OE_BMIN = 12;
constexpr double OE_BFRAC = 0.3;
constexpr double OE_AREA = 1100;          // ... on maps smaller than this
constexpr int SE_ROUNDS = 0;              // starving (no pearl for this many rounds): stale tiles are no goal, search wider (0: off)
constexpr int SE_CAP = 1500;              // ... navigation node budget while starving
constexpr bool QOPEN_HOME = false;       // the queen neither rushes the centre nor feels the opening's pull
constexpr int OE_STARVE = 0;              // a dragon that has not eaten for this many rounds stops feeling the pull (0: off)
constexpr int OE_MOD = 1;                 // only dragons with (id >> 1) % OE_MOD == 0 feel it (1: all)

// ---- experiments (0 / false: off - the base plays as described above)
constexpr bool DEBUG_WHY = false;         // INDICATOR line with the reason for each action (analysis builds)
constexpr bool KEEP_POCKET_MEM = false;   // dead-end pearls stay in memory (still not in-view food)
constexpr int QKING_FROM = 150;             // from this round our queen is the king, whatever her length (0: off)
constexpr int QKING_SILENT = 6;
constexpr double QKING_AREA = 1100;          // the crown only on maps of at least this many tiles (0: every map)
constexpr bool QKING_FEED_LONGER = true;  // under the crown, dragons longer than the queen feed her too (false: only shorter ones)          // ... unless nobody has seen or heard her for this long
constexpr double FOUNT_NAV = 0;           // navigation bonus for a known fountain (0: off)
// food scarcity: the share of tiles first seen that hold a pearl or are due within SCARCE_DUE rounds. Below
// SCARCE_T (once SCARCE_MIN tiles are seen) dragons chase farther pearls and explore less
constexpr double SCARCE_T = 0;            // (0: off)
constexpr int SCARCE_DUE = 100;
constexpr int SCARCE_MIN = 100;
constexpr double SCARCE_GATE = 0.015;     // NAV_GATE while scarce
constexpr int SCARCE_UNSEEN = 8;          // UNSEEN_COST while scarce
constexpr bool QNOPOCKET_RUN = true;    // the queen never takes a harvest run into a dead end (her split-escape leaves her inside)
constexpr double OWP = 0;                 // one-way passages: penalty for entering a 1-wide passage against its direction
                                          // (vertical: even column north, odd south; horizontal: even row east, odd west)
constexpr bool OWP_QUEEN_ONLY = false;
constexpr double CY = 0;                  // corridor yield: penalty for stepping into a 1-wide passage a friend's head is coming down
constexpr bool CY_QUEEN_ONLY = false;     // ... for the queen only
constexpr int BIGCLK = 0;                 // big maps: the contact clock's guard / feed / beacon / king-split rounds this much sooner
constexpr double BIGCLK_AREA = 1100;
constexpr double QSAFE_SMALL = 0;         // small maps: the queen prices every enemy head's reach at this stake
constexpr bool QNR_SMALL = false;         // small maps: the queen never rams or traps
constexpr bool QESC_SMALL = false;        // small maps: the queen's escape sprint
constexpr double QGUARD = 0;              // small maps: the queen's penalty for ending inside an enemy head's ram reach
constexpr int QGUARD_CAP = 8;             // ... reach counted up to this many steps
// small maps, graded guard for the queen: for each enemy head that would see her there, a tile inside the reach of
// its seen length costs QG2_W * (1 + steps to spare), inside only its estimated reach or one step past it QG2_W / 2;
// summed over enemies, so with every step threatened she takes the least threatened one (0: off)
constexpr double QG2_W = 0;
constexpr double QG2_HALO = 0.5;          // ... share of QG2_W inside only the estimated reach or one step past it
// small maps: our queen rams the enemy queen at any price, as the workers do (a queen trade), even when she otherwise
// never rams (QNR_SMALL)
constexpr bool QQRAM = false;
// small maps: with an enemy head in view, the queen's step is worth QFLEE_W per tile it adds to her distance from the
// nearest one (and costs as much per tile it takes away): the top teams' queens step away ~75% of the time, ours ~50%
constexpr double QFLEE_W = 0;
constexpr double QFLEE_CROWD = 0;         // ... and this much per enemy head within 3 steps of where she ends (minus where she is)
constexpr double QPORTAL_PEN = 0;         // small maps: the queen's cost of a portal step (twice that through an unseen one)
constexpr bool QSPLIT_SAFE = false;
// right of way among our own dragons (ROW). Priority: our queen; then a dragon longer than ROW_LONG_SMALL (maps of
// at most ROW_SMALL_AREA tiles) / ROW_LONG_BIG (larger) - it assumes it is the longest; then the longer by what is seen
// in view or heard by sonar (a right-of-way packet a dragon in a jam sends straight at the friend); then the lower id.
// A dragon does not step onto a 1-wide passage ahead of a friend with priority (or next to its head there). When
// boxed in, the queen stalls (SPLIT 2) and calls "stuck" on all four sonars; a friend whose segment blocks her and
// would not free up on its next move dies against kelp (never into a head); a boxed worker in the way of a friend
// with priority dies instead of stalling.
constexpr bool ROW = true;
constexpr double ROW_AREA_MAX = 1100;        // ... only on maps smaller than this many tiles (0: every map)
// small maps: the queen never walks deeper into a lane (a dead-end farming corridor) and never makes the lane-out
// split (it leaves her, the head end, inside)
constexpr bool QNOLANE = true;
constexpr int QNEED = 0;
// one-way portals for our team: a step through a portal from the higher-numbered tile to the lower costs this much,
// so our dragons do not cross the same portal from both ends at once and meet head-on on the unseen side (0: off)
constexpr double OWPORT_W = 0;                  // small maps: the queen's steps must leave her at least this many moves (0: as v30)
constexpr double ROW_SMALL_AREA = 256;
constexpr int ROW_LONG_SMALL = 8;
constexpr int ROW_LONG_BIG = 15;
constexpr double ROW_W = 5.0;             // cost of stepping onto the passage ahead of a friend with priority
constexpr int ROW_AHEAD = 5;              // ... tiles of 1-wide passage followed ahead of its head       // small maps: the queen does not split (stand still) while an enemy head can reach her
// from this round (whatever the contact clock says) a dragon at least half the king's length stops splitting: under
// the clock's "everything late" setting (REG_LATE 499) the second-longest dragon - or the king's own tail after an
// escape split, still hearing the old king's beacon - otherwise sheds a bud every turn down to 3 (0: off)
constexpr int LATE_KEEP = 420;
// under the crown, the dragon that would be king without it (longest of ours that it knows, longer than the queen)
// keeps the king's no-split rule: our longest stays whole in case the queen dies
constexpr bool QK_KEEP_LONG = false;
// big-map gather: from GATHER_BIG every dragon that knows the king (and is not it) heads for it, food or not
constexpr int GATHER_BIG = 0;             // round (0: off)
constexpr double GATHER_AREA = 1100;
constexpr double GATHER_W = 2.0;          // navigation weight towards the king
constexpr int GATHER_FEED = 0;            // big maps: feeding may start from this round (0: the clock's)
constexpr int GATHER_MEM = 40;            // how old a king sighting a gatherer still follows
constexpr double GATHER_BONUS = 8;        // a gatherer's route-cost bonus for the king's tile (guards: GUARD_MEM_BONUS)

}  // namespace v30::cfg
