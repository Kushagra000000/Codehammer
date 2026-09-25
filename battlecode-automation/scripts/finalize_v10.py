"""Package tested v10 slots and report evidence, without champion promotion."""
from collections import defaultdict
from pathlib import Path
import hashlib, json, zipfile
import v10_matches as evaluation
import autorun_matches as ladder
from research_report import wilson

ROOT=ladder.ROOT;OUT=evaluation.OUT

def main():
    report=evaluation.report()
    rows=ladder.read_jsonl(OUT/'matches.jsonl')
    current={name:ladder.fingerprint_dir(ROOT/'bots/candidate'/name) for name in ['bot-v10','bot-v10.1']}
    final=[r for r in rows if r['experiment']=='v10.1-r4-validation']
    resource=[r for r in rows if r['experiment']=='v10.1-r4-final-sandbox']
    if len(final)!=44 or len(resource)!=2 or any(r['status']!='ok' for r in final+resource):
        raise RuntimeError('Final validation is incomplete or unparsed; no package issued')
    if any((r['b_hash'] if r['reverse'] else r['a_hash'])!=current['bot-v10.1'] for r in final+resource):
        raise RuntimeError('Current source differs from final validation')
    diagnostics=ladder.load_json(OUT/'resource_diagnostics.json',[])
    final_diag=[d for d in diagnostics if d['id'] in {r['id'] for r in resource}]
    if len(final_diag)!=2 or any(d['teams'][d['candidate_side']]['cpu_limit_events'] or d['teams'][d['candidate_side']]['invalid_action_deaths'] for d in final_diag):
        raise RuntimeError('Final source failed resource checks')
    packages={}
    for name in current:
        source=ROOT/'bots/candidate'/name;dest=ROOT/'bots/candidate'/f'{name}.zip'
        with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
            for p in sorted(source.iterdir()):
                if p.suffix in {'.cpp','.hpp','.toml'}:z.write(p,p.name)
        packages[name]={'source':str(source.relative_to(ROOT)),'source_fingerprint':current[name],
            'zip':str(dest.relative_to(ROOT)),'zip_sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
    def wdl(xs):return '/'.join(str(sum(r['score']==s for r in xs)) for s in [1,.5,0])
    text='''# v10 feature implementation report — 2026-09-26

Best new prototype: **bot-v10.1**, parent **bot-v10** (preserved r3 source),
ultimately based on v9. The formal champion remains **bot-v9**. No official
submission was made. The broad candidate search has been replaced by bounded
comparisons of v10, v10.1, v8 and v9.

## Result

The final source won 30/44 fresh-seed all-map games: 17–5 versus v8 and 13–9
versus v9. This is a positive observed result, not evidence of exponential or
universal superiority. Its earlier direct screen against stable v10 was 4–4.
The v5 five-map acceptance requirement was **not tested** because the user
restricted this work to the four-bot pool. Champion metadata is not promoted.

## Implemented

- Growth specialisation from round 250, inclusive; grower food priority.
- Directed sonar role/position/length and bounded territory messages; larger
  growers have precedence, with deterministic ID tie-breaking and expiry.
- Mapped clear rectangles, smaller-worker exclusion preferences and a safe
  serpentine patrol cycle with a reserved return column. Safety overrides patrol.
- Late self-collision feeding beside a visible, reachable grower. Friendly
  heads are excluded; stale reports and nearby enemy heads abort a donation.
- Small late attackers pursue and relay large-enemy head positions. Tactical
  sprint head trades are restricted to small dragons.
- Undersized growers yield to larger growers; split children can recover clear
  terrain through claims. A trapped grower can reverse most of its length into
  a split child when the tail escape is locally checked.
- Remembered own-body positions and self-collision fallback protect allies.

The bot uses reverse feeding rather than oversprinting: sprinting consumes
segments before death, reducing resource yield. Sonar is local ray communication,
not guaranteed global consensus. The patrol is a safe preference, not an
unconditional motion command on obstructed or contested terrain.

## Effective parameters

| Parameter | Final value |
|---|---:|
| Growth start | 250 |
| Stable hash grower fraction | 1/8, plus founders, large dragons and survival backups |
| Feeding start | 380 |
| Maximum feeder length | 4 |
| Minimum recipient length | 10 |
| Feeding radius | 3 |
| Initial territory capacity | max(80, 4 × grower length) |
| Minimum patrol grower length | 16 |
| Minimum cycle capacity | max(80, length + 20) |
| Claim expiry / position expiry | 12 / 2 rounds |
| Enemy report expiry | 2 rounds; forwarding preserves timestamp |
| Direct head-trade / sprint length cap | 3 / 4 |
| Sonar frequency | Every turn, up to four directions |

## Final validation by map

Seed 7203, both sides on every discovered map. Entries are W/D/L.

| Map | vs v8 | vs v9 |
|---|---|---|
'''
    for m in sorted({r['map'] for r in final}):
        text+=f'| {Path(m).stem} | '+ ' | '.join(wdl([r for r in final if r['map']==m and Path(r['opponent']).name==o]) for o in ['bot-v8','bot-v9'])+' |\n'
    vs9=[r for r in final if Path(r['opponent']).name=='bot-v9']
    lengths=[r['metrics']['teams']['B' if r['reverse'] else 'A']['longestDragon'] for r in final]
    rating=next(r['internal_elo'] for r in report if r['experiment']=='v10.1-r4-validation')
    text+=f'\nFinal-validation win rate: {30/44:.1%}. v9 win rate: {13/22:.1%}; Wilson 95% lower bound {wilson([r["score"] for r in vs9]):.1%}, below 50%, so statistical superiority over v9 remains unestablished.\n'
    text+=f'\nInternal native-cohort Elo for the final source: {rating}; this is separate from official team ratings. Mean final longest dragon {sum(lengths)/len(lengths):.1f}, maximum {max(lengths)}. Early eliminations count as zero.\n'
    text+='\n## Experiment history\n\n| Experiment | Games | W/D/L | Disposition |\n|---|---:|---|---|\n'
    for r in report:
        disposition={'v10-r1-screen':'Rejected: loses to v9','v10.1-r1-screen':'Advanced to broader tests','v10.1-r2-sandbox':'Resource checks passed; open-map losses',
            'v10.1-r3-allmaps':'Rejected for promotion: 10–12 vs v9; preserved as v10',
            'v10.1-r4-concentration':'4–4 vs v10, 5–3 vs v9; advanced',
            'v10.1-r4-validation':'Best new prototype; no formal promotion',
            'v10.1-r4-final-sandbox':'Final-source resource validation'}[r['experiment']]
        text+=f'| {r["experiment"]} | {r["games"]} | {r["W"]}/{r["D"]}/{r["L"]} | {disposition} |\n'
    native=sum(not r['sandbox'] for r in rows);sandbox=sum(r['sandbox'] for r in rows)
    text+=f'\nTotal: {len(rows)} ledger games ({native} native, {sandbox} sandbox), plus one separately recorded manual smoke. Final source: 60 native games and 2 sandbox games. All ledger results parsed; raw stdout/stderr and replays preserved.\n'
    text+='\n## Resource and correctness checks\n\n'
    for d in final_diag:
        t=d['teams'][d['candidate_side']]
        text+=f'- Big Empty, side {d["candidate_side"]}: maximum {t["max_turn_points"]:,.0f} points, {t["cpu_limit_events"]} CPU-limit events, {t["invalid_action_deaths"]} invalid actions.\n'
    text+='''
Toolkit: isolated unswbc 1.1.0, Python 3.13, Protocol 3. C++ safety fixtures
cover phase boundaries, priority, safe feeding, stale reports, blocked mouths,
enemy aborts, cycle continuity, packet expiry/team separation, split escapes,
friendly-head fallback, claim precedence/size, and grower sprint protection.
Python parser tests pass. Header-sensitive wasm cache invalidation is repaired
in both local runners without modifying the installed engine.

## Replay evidence and remaining weaknesses

The provided M237041 is Stronghold: A wins 151–102 in final longest length,
with A already at length 29 by round 100. Our requested round-250 specialisation
does not reproduce that early resource concentration. Source code and deliberate
intent cannot be recovered from the screenshot/replay.

In the initial v10 smoke, nine confirmed self-feeds dropped sixteen pearls:
nine went to named recipients, seven to other allies, none to enemies. This
proves the mechanism works but also shows imperfect recipient priority.
Detailed current-source replay accounting is saved separately when available.

Two final-source Stronghold replays confirm four self-feeds dropping eight
pearls: five collected by named growers, one by another ally, two uncollected
at the end, and none by enemies. One checked grower split reversal occurred.
Their geometry, portals, pearl respawn rules and initial dragons match the
supplied replay's map exactly (see `reference-map-comparison.json`).

Weaknesses remain map- and seed-dependent. Big Empty varied from two losses in
earlier tests to two v9 wins in final native validation, then two losses on
the fresh final sandbox seed. Several early-elimination
maps still regress. Stronghold improved in most comparisons, but one final
v9 loss was 17–18 at round 500. Neither that gain nor the all-map aggregate
establishes universal superiority. Multiple changes were bundled; individual
causal effects are unknown.

Promising next work: stronger protection of the longest surviving grower,
more reliable donation collection, contested-food reachability rather than
distance alone, and independent paired seeds before champion promotion. No
further search or official submission is running automatically.

## Reproducibility and paths

All tested candidate states are preserved in SHA-256-named source archives.
Matches record the existing Git HEAD plus source and map fingerprints; these
changes are not represented as a new Git commit. `current_sources.json` and
`packages.json` identify the final source trees and downloadable ZIPs.

'''
    for name,p in packages.items():text+=f'- **{name}**: `{p["source"]}`; ZIP `{p["zip"]}`; source SHA-256 `{p["source_fingerprint"]}`.\n'
    (OUT/'REPORT.md').write_text(text)
    (OUT/'packages.json').write_text(json.dumps(packages,indent=2)+'\n')
    state=ladder.load_json(ROOT/'battlecode-automation/data/research_state.json',{})
    state['LATEST_CANDIDATE']='bots/candidate/bot-v10.1';state['LATEST_CANDIDATE_SHA256']=current['bot-v10.1']
    state['stopping_condition']='Requested v10 features and bounded validation completed; superiority over v9 remains statistically unestablished'
    state['v10_report']='battlecode-automation/experiments/v10/REPORT.md'
    (ROOT/'battlecode-automation/data/research_state.json').write_text(json.dumps(state,indent=2)+'\n')
    print(json.dumps(packages,indent=2))

if __name__=='__main__':main()
