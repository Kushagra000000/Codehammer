# Research protocol, declared before screening

Environment: Python 3.13.2, unswbc 1.1.0, bundled engine. Two match workers.
Frozen v5 and incumbent v8 retain their initial manifest hashes.
No automatic official submissions or challenges.

Initial scope: parameter equivalence, then bounded single-variable screens for
king, feeding, distribution, portal probability, adaptive growth, navigation and
echo information. At most 400 new games in this initial research batch. This is
a practical match budget, not evidence of a strategic plateau. Stop earlier for
an infrastructure failure; do not promote if required evidence is unavailable.

Equivalence: same map, opponent, side and pearl seed; compare outcomes and replay
event streams. Native seed does not seed arbitrary bot RNG, but v8 uses its own
deterministic hash. Native timings/metadata are not part of equivalence.

Screening: seeds 101 and 202 as budget permits, both sides, v5 and v8, plus older
diverse opponents for finalists. Holdouts: schooltime, trauma and big_empty.
Do not tune parameters using holdout results. New maps enter discovery and are
validation maps until a future experiment plan assigns them.

Promotion: at least 8 v5 games per map (4 seeds × both sides). A beaten map needs
a 95% Wilson lower confidence bound for score >0.5 (draw=0.5), plus at least one
win on each side. At least five maps must pass. This is a conservative gate;
small screens cannot satisfy it. Finalists need a separate seed set and broad
opponents, aggregate score >0.5 against incumbent, no unresolved parse/crash,
no unexplained map-family collapse, successful sandbox checks, full fingerprints
and logged analysis. Existing Elo alone never promotes a candidate.

Unavailable strategic metrics are null/UNKNOWN, never invented. Inferred
self-collision is not called intentional sacrifice without action evidence.
