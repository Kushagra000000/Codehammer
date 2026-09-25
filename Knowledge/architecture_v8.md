# v8 architecture audit — 2026-09-26

OBSERVED from every line of `bots/current/bot-v8/main.cpp` and its helper.

Each dragon owns an independent map-sized memory of edges, visit counts, pearl
observations and countdowns. Vision updates this memory before action selection.
Canonical portal edges are paired by ID; unknown exits remain uncertain. Children
start fresh. No shared memory or inherited parent state is assumed.

Visible heads generate one-step enemy and ally threat arrays. Candidate generation
rejects kelp and occupied destinations except small non-grower enemy-head trades.
A capped flood fill estimates space, treating unknown ordinary edges as open.
Pearl objectives use toroidal Manhattan distance with staleness penalties.
Candidates combine pearl progress, immediate growth, visits, exploration age,
spacing, portals, exits, space, threats and deterministic ID/round jitter.
Growers and length >=4 dragons choose a safe tier when available.

Splitting takes precedence, leaves a length-2 parent, and gives the remaining body
to a child. All dragons split before round 301; thereafter a stable hash selects
one quarter as growers; after round 425 every survivor retains length.
Two-step sprints target visible enemy heads only. Unlike direct trades, sprint
trades lack an upper length bound, although the safe tier often suppresses them.

Four directed 64-bit sonar messages carry team, ID, predicted position/length,
round and grower status. Received signals expire after approximately two rounds.
The helper negotiates Protocol 3 and parses echoes, but the strategy ignores them.
Messages have a marker, not authentication. Echo counts are aggregated across rays.

OBSERVED limitation: ordinary one-step destinations have just been observed, so
their age reward is normally zero. Raising that coefficient alone does not create
map-wide exploration. Archived v8.1 adds frontier potential and portal bonuses;
v8.2 additionally changes threat/trade/bootstrap logic, confounding attribution.

No explicit king, feeder, sacrifice, or portal probability exists in v8. These
must remain disabled in an equivalent parameterised baseline.

Historical champion: v8, 1220.10 internal Elo / 110 games. This is prior evidence,
not current-toolkit revalidation. Highest archived version: v8.2; next family: v9.
