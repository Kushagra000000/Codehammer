## Strategic Objective

The primary objective is not to maximise the number of living dragons or total team length by itself.

After 500 rounds, the first tiebreak is the length of the longest surviving dragon. Only after comparing the longest dragon does total team length matter.

Therefore the bot should evolve from:

**swarm creation → map/resource acquisition → role specialisation → resource concentration → protected longest dragon**

The strategy should explicitly account for this changing objective.

---

## 1. Early Game: Swarm Creation

### Objective

Create enough dragons to cover the map and acquire pearls from many different regions.

### Current hypothesis

Rapid splitting is valuable because a single long dragon can recursively produce several smaller dragons.

A newly created split child receives its own program instance and takes a later turn in the same round. The child has no memory of the parent.

### Important consequence

Splitting increases physical coverage but destroys local memory.

Therefore the swarm should compensate through:

* deterministic exploration targets,
* sonar communication,
* portal information sharing,
* pearl hotspot sharing,
* role assignment from dragon ID.

### Variables to expose

* `SPLIT_UNTIL_ROUND`
* `SPLIT_MIN_LENGTH`
* `MAX_PRE_GROWTH_LENGTH`
* `TARGET_SWARM_SIZE`
* `MAX_SWARM_SIZE`
* `ALLOW_RECURSIVE_SAME_ROUND_SPLITS`

These values must be configurable near the top of `main.cpp`.

---

## 2. Exploration / Distribution

### Problem

A pearl-oriented local score can cause several dragons to converge onto the same profitable region.

The objective is not simply "find pearls".

The swarm should approximately cover the entire board so that:

1. pearls in different regions are discovered,
2. future pearl spawns are encountered,
3. portals are discovered,
4. enemy locations are detected,
5. the team is harder to eliminate with one attack.

### Candidate approaches

#### A. Sector Assignment

Assign every dragon a deterministic exploration sector using its ID and the map dimensions.

Example concept:

`sector = hash(id) % number_of_sectors`

A dragon prefers unexplored positions within its sector unless combat or a high-value pearl justifies leaving.

#### B. Frontier Exploration

Prefer cells with:

* high time since last visit,
* low teammate density,
* unknown edges,
* unknown portal information,
* low known pearl density.

#### C. Hybrid

Normal movement:

`pearl value + exploration value + safety + spacing`

rather than purely following the nearest pearl.

### Metrics

Measure:

* unique tiles visited,
* map percentage explored,
* number of regions containing team dragons,
* average pairwise distance between team dragons,
* number of dragons within 3/5/10 tiles of one another.

---

## 3. Portal Strategy

Portals are valuable because they can provide very rapid map traversal.

However, portal destinations are initially unknown and dragons cannot see through them.

### Hypothesis

Use portals more aggressively during exploration but protect established growers.

A target experiment is approximately:

`PORTAL_EXPLORATION_PROBABILITY = 15%`

but this should mean:

> When a portal is a strategically acceptable move, use it approximately 15% of the time.

It should NOT mean blindly enter dangerous or completely unknown portals 15% of the time.

### Portal roles

#### Scout

A small dragon is allowed to explore an unknown portal.

#### Route

Once both portal endpoints are known, treat the portal as a real transport edge.

#### Escape

A portal can be preferred when escaping enemy pressure.

#### Feeding

A portal can potentially be used to move feeders or the king rapidly between regions.

### Variables

* `PORTAL_PROBABILITY_PERCENT`
* `PORTAL_UNKNOWN_PENALTY`
* `PORTAL_KNOWN_BONUS`
* `PORTAL_SCOUT_MAX_LENGTH`
* `ALLOW_PORTAL_FOR_KING`
* `PORTAL_ESCAPE_BONUS`

---

## 4. King / Capital Dragon Strategy

### Hypothesis

A single large dragon should become the team's main resource sink during the late game.

The team should not necessarily treat all dragons equally.

The king should:

* avoid unnecessary combat,
* avoid head trades,
* collect valuable pearls,
* receive resources produced by sacrificial dragons,
* remain mobile,
* avoid dead ends,
* receive team status through sonar.

### King selection

Possible strategies:

1. Original dragon ID.
2. Original team root.
3. Lowest living team ID.
4. Largest dragon.
5. Largest dragon after a threshold round.
6. Hybrid: original root unless it dies, then largest safe dragon.

The "ID 1 is king" observation should be tested rather than assumed universally.

If team B's original dragon really is consistently ID 1 on the competition maps, this can be a dedicated experiment.

A more robust version is:

`KING_ID = original/root dragon`

rather than simply hard-coding `1`.

### Backup king

Maintain at least one candidate backup grower.

If the king dies, the backup immediately becomes the new king.

---

## 5. Late-Game Sacrifice / Feeding

### Hypothesis

Near the end of the game, small dragons should stop trying to survive independently and instead convert themselves into pearls for the king.

This is directly motivated by:

* longest surviving dragon being the primary tiebreak,
* dragon death producing pearls,
* small dragons having low strategic value near the end.

### Proposed phases

#### Preparation

Around round 420–449:

* establish king,
* keep king alive,
* small dragons begin moving toward the king,
* do not sacrifice yet unless advantageous.

#### Feeding

Around round 450 onward:

* short dragons approach the king,
* avoid the king's head,
* target the king's body / nearby feeding position,
* deliberately die,
* king collects the resulting pearls.

### Feeder candidates

Prioritise:

`length <= 3`

because they have relatively little remaining growth potential and can still generate useful pearls when they die.

### Candidate death methods

1. Collision into king's body.
2. Controlled self-collision.
3. Controlled sprint death.
4. Enemy-body collision only if it does not feed the enemy.
5. Head collision should generally be avoided because it can kill the king.

### Variables

* `FEED_START_ROUND`
* `FEED_ONLY_SMALL_DRAGONS`
* `SACRIFICE_MAX_LENGTH`
* `KING_FEED_RADIUS`
* `KING_PROTECTION_RADIUS`
* `SELF_COLLISION_FEED`
* `BODY_COLLISION_FEED`
* `MIN_TIME_FOR_KING_TO_COLLECT`
* `BACKUP_KING_ENABLED`

---

## 6. Adaptive Phases

Fixed round numbers such as 300 and 425 are useful baselines but should not necessarily remain fixed.

A better system should consider:

* current round,
* rounds remaining,
* current king length,
* total team length,
* unit count,
* number of small dragons,
* enemy longest known length,
* known pearl density,
* map size,
* whether the team has reached 64 dragons.

Example:

Early phase ends when:

`unit_count >= target_swarm_size`

OR

`round >= split_cutoff`

Growth phase begins when:

`king exists && enough feeders exist`

Feeding phase begins when:

`rounds_remaining <= expected_feeding_time`

This makes the strategy less dependent on one arbitrary round value.

---

## 7. Pearl Strategy

Pearls are the only growth resource.

The bot should therefore distinguish:

### Immediate pearl

Pearl currently visible.

### Predicted pearl

Pearl expected to spawn soon.

### High-value region

A region with repeated pearl activity.

### Dangerous pearl

Pearl that requires passing through:

* enemy threat,
* kelp,
* unknown portal,
* narrow area,
* teammate congestion.

The best path to a pearl is not necessarily the Manhattan shortest path.

Future experiments should compare:

1. toroidal Manhattan distance,
2. known-map BFS,
3. portal-aware BFS,
4. risk-adjusted BFS,
5. expected-value path planning.

---

## 8. Combat

### Small dragons

Useful for:

* hunting enemy heads,
* forcing enemy trades,
* scouting,
* sacrificial feeding,
* distracting enemy swarms.

### King / large dragons

Useful for:

* survival,
* pearl accumulation,
* denying space,
* forcing enemy movement,
* collecting sacrifice pearls.

The king should not automatically inherit the same combat policy as small dragons.

### Important experiment

The current tactical sprint code can create head-trade attacks for non-growers of length >=3.

Test whether:

`large dragon head-trade = false`

while:

`small dragon head-trade = true`

improves results.

---

## 9. Team Communication

Sonar is the team's only true shared communication mechanism.

Useful messages:

* king position,
* king length,
* king direction,
* feeder target,
* danger location,
* portal location,
* portal ID,
* exploration target,
* emergency retreat,
* backup king.

### Communication hierarchy

King:

* broadcasts current state.

Small dragons:

* report location and role.

Explorers:

* report discovered portals / useful regions.

Feeders:

* report that they have entered feeding range.

Because sonars can be intercepted by enemy dragons, messages should contain only information whose tactical leak is acceptable.

---

## 10. ID / Turn Order Strategy

Dragon IDs determine turn order.

Lower IDs act before higher IDs.

A newly split child gets a new ID and acts later in the round.

This creates a possible hierarchy:

**king → older dragons → newer split children**

A low-ID king can broadcast instructions that higher-ID teammates may receive within the same round, while messages travelling to a lower-ID dragon may arrive on its next turn.

This turn-order asymmetry should be explicitly tested.

---

## 11. New Strategy Ideas

Other strategies worth investigating:
a) Having smaller snakes swarm near enemy heads and send location to other smaller snakes to do the same to restrict enemy motion and having it kill itself. This will be late game strategy and the exact round when this strategy is implemented needs to be tested. Try testing from round:
    - starting round 400
    - starting round 425
    - starting round 450

### Map coverage

Use deterministic sector targets to prevent the swarm collapsing into one area.

### Portal scouts

Sacrifice low-value dragons to map unknown portal connections early.

### Portal escape

Use known portals as emergency escape routes from enemy pressure.

### Body barriers

Use large dragons and positioning to make regions harder for enemy heads to enter.

### Enemy head hunting

Use short dragons to intercept small enemy dragons while protecting the king.

### King escort

Keep a small number of safe dragons around the king to detect threats and collect nearby pearls.

### Delayed growth

Do not grow too many dragons if their resources can produce a much longer king instead.

### Backup king

Maintain a second large dragon so the entire late-game plan does not collapse after one death.

### Opponent-specific policies

Learn whether particular opponents favour:

* swarm,
* king,
* portal-heavy exploration,
* aggressive head trading,
* central map control,
* late sacrifice.

Do not hard-code an opponent-specific strategy until replay evidence confirms that the behaviour is reliable.

---

## 12. Evaluation Principle

A strategy should not be accepted because it wins one spectacular game.

A candidate should be tested:

* across all available maps,
* against the current champion,
* against the frozen v5 baseline,
* against multiple older versions,
* against strong external opponents when replays/data are available.

The important statistics are:

1. overall win rate,
2. map-by-map win rate,
3. longest surviving dragon,
4. total team length,
5. elimination round,
6. final unit count,
7. portal success/death rate,
8. exploration coverage,
9. number of sacrificial conversions,
10. king survival rate.

---

## 13. Research Principle

Never assume a strategy is correct simply because a top team appears to use it.

Each observed behaviour should become:

**Observation → Hypothesis → Implementation → Controlled Experiment → Result → Conclusion**

Replay observations are evidence for hypotheses, not proof that the implementation is optimal.


## 2026-09-26 research findings (bounded screens)

OBSERVED in v8 code: candidate tiles are usually refreshed by the current vision
before age scoring. Changing their age coefficient alone cannot reliably drive
map-wide exploration. EXP004 measures the *future vision window* instead.

TESTED on eight development maps, seed 101, both sides against v5/v8: EXP004
frontier scored 25.5/32 versus equivalent v8's 22/32. This supports advancing the
component to independent validation, not claiming it is universally better.
In particular it lost both Default games against v8; map regressions need followup.

TESTED local king alone scored 21/32, below baseline. King plus opportunistic
self-feeding scored 24/32; both Stronghold games against v8 were wins with logged
feeding actions. Only six scheduled matches reached round 300, so general
late-game value remains UNKNOWN. Local leader election can have multiple kings
under partial visibility; it does not implement global consensus.

OBSERVED protocol constraint: four sonar echoes are aggregate counts. They cannot
be converted into four directional enemy positions. Any directional probe needs
an explicit emission schedule and delayed-response bookkeeping.


## Accepted v9 finding — 2026-09-26

SUPPORTED within this batch: pearl objective distance should account for known
kelp and paired portals. The BFS-only candidate passed 10/11 v5 map gates and
beat v8 11–3 across development and holdout matches. The frontier+BFS combination
lost 2–5–1 to BFS alone; more exploration is not automatically additive.

OBSERVED limitation: Trauma rewards preserving late longest length. High coverage
and many dragons can still lose to a small conservative opponent by one segment.
Do not optimize coverage as a proxy for the primary tiebreak. King/feeding and
risk-adjusted routing remain hypotheses for a new independent experiment batch.

## v10 coordination findings — 2026-09-26

OBSERVED: feeding can transfer resources to designated growers, but proximity
alone does not ensure the named recipient collects them. In the initial
Stronghold smoke, seven of sixteen donated pearls went to other allies.

TESTED: broad early food denial hurt v10's opening against v9 (2W/1D/5L in the
four-map screen). Narrower early right-of-way, larger patrol areas, and terrain
sharing together improved v10.1 to 4W/2D/2L against v9 on the same screening
schedule. These changes were tested as a bundle; individual effects are UNKNOWN.

OBSERVED: the supplied Stronghold winner grew well before round 250. Late
feeding alone cannot explain its final length-151 result. A snake's death
returns only ceil(length/2) pearls, so feeding concentrates resources with loss.

HYPOTHESIS: active territory cooperation is most useful in rich, bounded
chambers. Two Big Empty sandbox losses show that the current implementation
has not established a large-open-map advantage. Do not extrapolate the
Stronghold wins into a universal improvement claim.

## 2026-09-26 v11 navigation research

OBSERVED: a rectangular zigzag is not by itself a collision-safe Hamiltonian
policy. The entire body's cyclic order matters; rebuilding or shortcutting
without preserving that order invalidates the argument. The tail's currently
occupied square is forbidden in this engine, even if it would vacate.
TESTED: ordered patrol construction, 500 consecutive invariant-preserving moves,
split-child visible body reconstruction, intermediate sprint history and
bounded virtual-body dodges pass deterministic fixtures. Enemy interference
and unseen occupancy remain UNKNOWN; no global survival guarantee is claimed.

OBSERVED: the v10.2 4-bit claim tag overlaps legacy C8/CA/CB packets. TeamB can
accept some old messages as false claims. Disjoint message tags plus a regression
fixture fix this compatibility error. Sonar still has no authenticated sender.

TESTED/REJECTED as defaults in the v11 screens: depth-eight lookahead, restricting
claims to productive rectangles, universal early-grower specialisation, and a
stronger short opening center pull. The early-grower prototype reached73 on
Big Empty but regressed on other maps. None is a general strength result.

OBSERVED: caching same-turn food distances and own-body occupancy reduced the
observed sandbox peak from99.8M in the initial prototype to55.0M in subsequent
Big Empty checks (different seeds/trajectories). Do not spend the entire judge
budget on lookahead; keep bounded search and test exact source fingerprints.

HYPOTHESIS: opening population and small head trades can dominate outcomes on
compact arenas before long-snake navigation ever activates. Colosseum failures
were mostly early head-to-head deaths. Final R8 tests relax only small-dragon
trade restrictions while retaining large-grower and last-ally protection.
The final report records whether this survives fresh-seed validation.

## 2026-09-27 — tested tail-rescue constraint

OBSERVED in engine replays: a trapped long snake can have a clear tail exit far
outside its head's7x7 vision. Requiring that exit to be currently visible blocked
legal rescue splits and caused avoidable forced self-deaths at lengths47/28.
TESTED in v10.4.1 rewrite: legal L-2 tail splits with fresh child vision work for
both workers and growers, including incomplete parent body memory. Known sealed
ends and unit caps remain real limitations; an unseen enemy can kill the child.

SUPPORTED as implemented behavior, not strength: local pearl BFS can temporarily
redirect an established patrol while preserving normal threat checks. Final
all-map screen6W/5L and all final-code games8W/8L do not establish an overall
upgrade. Stronghold longest-length concentration remains UNKNOWN/unresolved.
