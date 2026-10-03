# alan-v1.1

Parent: `bots/alan/alan-v1`

Baseline evidence: the immutable alan-v1 snapshot
`071dc2555e53f440680820095e57fa1ab1705ac867dc53bad3d51b0718f5699f`
scored 9W/0D/21L against `bot-v10.3.3` on all 15 available maps, both sides,
seed 10333. Fifteen of its 21 losses were eliminations.

## Hypothesis

Preventing voluntary head collisions whenever a non-colliding move exists, and
preserving late feeders by split-escaping under visible enemy pressure, will
reduce avoidable eliminations without disabling forced trades when the head is
physically boxed in.

## Changes

- Candidate selection is survival-first: safe route, viable route, emergency
  unknown portal, threatened route, sealed route, and only then head trade.
- A head trade can no longer override any available non-colliding movement.
- If no non-colliding candidate exists, adjacent enemy heads are admitted as a
  forced trade before the mandatory self-collision fallback.
- The final self-collision is retained only for the rules case where every
  passable destination is occupied by a non-head body and no legal split exists.
- Late feeders may split-escape certain death when a visible enemy is within
  three cells; they still retain the normal feeding policy when no enemy is near.

Evaluation results are stored outside this directory so they do not change the
candidate source fingerprint.
