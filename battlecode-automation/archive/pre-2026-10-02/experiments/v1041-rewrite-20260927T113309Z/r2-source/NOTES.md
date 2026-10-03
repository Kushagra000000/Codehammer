# v10.4 — untested derivative of v10.3.1

Only main.cpp and coordination.hpp change strategy. Helper and project settings
are copied unchanged. No build, test, match, or submission was run.

Editable settings in Config near the top of main.cpp:

- WORKER_SPLIT_STOP_ROUND = 350 (previously tied to feeding at 380).
- FEED_START_ROUND = 380; FEEDER_MAX_LENGTH = 4.
- ALPHA_FEED_START_ROUND = 475.
- FEED_MIN_GROWER_LENGTH = 10; FEED_REQUEST_MAX_AGE = 1.
- FEEDING_RADIUS = 3; FEED_APPROACH_WEIGHT = 2400.

Round-250 grower selection and emergency grower escape splits are preserved.
Ordinary splitting stops at 350. Existing workers and small growers may answer
feeding requests from 380. From 475 all lengths may answer a strictly larger
grower's request. Largest fresh requester wins, with lower ID breaking recipient
ties; equal-length snakes do not donate to one another. This is a local alpha,
not a guaranteed global election.

A checked 0xE sonar packet requests feeding using the existing status layout,
length capped at 511, position and checksum. Donors stop requesting food while
approaching a recipient. Claims retain their rotating ray. Requests are not
relayed; visual sightings cannot refresh them. Donors keep valuable-snake safety
logic, but stop receiving the patrol bonus while approaching the recipient.

Suicide uses verified own-body collision, never the recipient's head or an
oversprint. It requires a fresh request, visible recipient head, a clear local
collection path within three steps and no visible enemy head within four tiles
of either head. Without a safe delivery, ordinary movement continues. Local
checks cannot guarantee safety from unseen enemies or guarantee pearl collection.

HYPOTHESIS: earlier split cessation and late concentration improve longest length.
UNKNOWN: win rate, runtime validity and net strategic benefit. Donation returns
ceil(length/2) pearls spread along the donor's body, not all beside alpha. It
loses team length and a backup grower. The 25 remaining rounds after 475 may be
insufficient for travel and collection. Keep v10.3.1 as the user-designated best
until manual comparison supplies evidence.
