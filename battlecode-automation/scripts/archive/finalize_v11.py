"""Freeze the completed v11 research report. Never submits or promotes."""
from pathlib import Path
from collections import Counter, defaultdict
import hashlib,json,sys,zipfile
import research
import v11_report
OUT=research.ROOT/'battlecode-automation/experiments/v11'

def tally(rows):
 c=Counter({1:'W',.5:'D',0:'L'}[r['score']] for r in rows)
 return f"{c['W']} / {c['D']} / {c['L']}"
def main():
 rows=research.ladder.read_jsonl(OUT/'matches.jsonl');summary=v11_report.build()
 assert all(r['status']=='ok' for r in rows),'Resolve every match error first'
 assert len({r['id'] for r in rows})==len(rows),'Duplicate match identity'
 final=[r for r in rows if r['experiment']=='v11.1-r8-final-validation']
 mutual=[r for r in rows if r['experiment']=='v11.1-r8-mutual']
 sandbox=[r for r in rows if r['experiment']=='v11.1-r8-resource']
 assert len(final)==110 and len(mutual)==22 and len(sandbox)==4,'Final schedule still running'
 current={}
 for name in ['bot-v11','bot-v11.1']:
  project=research.ROOT/'bots/candidate'/name;sha=research.ladder.fingerprint_dir(project)
  current[name]={'path':str(project.relative_to(research.ROOT)),'sha256':sha,'status':'experimental; not promoted'}
  dest=OUT/f'{name}.zip'
  with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED) as z:
   for p in sorted(project.iterdir()):
    if p.suffix in {'.cpp','.hpp','.toml'}:z.write(p,arcname=f'{name}/{p.name}')
  current[name]['package']=str(dest.relative_to(research.ROOT));current[name]['package_sha256']=hashlib.sha256(dest.read_bytes()).hexdigest()
 assert all((r['b_hash'] if r['reverse'] else r['a_hash'])==current['bot-v11.1']['sha256'] for r in final+mutual+sandbox)
 by_opponent=defaultdict(list);by_map=defaultdict(list)
 for r in final:by_opponent[Path(r['opponent']).name].append(r);by_map[Path(r['map']).stem].append(r)
 passed=all(sum(r['score'] for r in rs)>len(rs)/2 for rs in by_opponent.values())
 assert not passed,'Revisit decision language: all-opponent outcome changed'
 result={'best_new_candidate':'bot-v11.1','current_sources':current,'promotion':'rejected','reason':'Did not achieve positive fresh validation against every requested baseline; historical v5 gate untested.','v5_requirement':'not tested; not satisfied by this evidence','official_submission':False,'all_games':len(rows),'native_games':sum(not r.get('sandbox') for r in rows),'sandbox_games':sum(bool(r.get('sandbox')) for r in rows),'final_validation_games':len(final),'mutual_games':len(mutual),'opponents':{o:tally(rs) for o,rs in by_opponent.items()},'maps':{m:tally(rs) for m,rs in by_map.items()}}
 (OUT/'decision.json').write_text(json.dumps(result,indent=2)+'\n');(OUT/'current_sources.json').write_text(json.dumps(current,indent=2)+'\n')
 data=next(x for x in summary if x['experiment']=='v11.1-r8-final-validation')
 resource=json.loads((OUT/'resource_diagnostics.json').read_text());ids={r['id'] for r in sandbox}
 resource=[r for r in resource if r['id'] in ids]
 max_points=max(r['teams'][r['candidate_side']]['max_turn_points'] or 0 for r in resource)
 cpu=sum(r['teams'][r['candidate_side']]['cpu_limit_events'] for r in resource)
 invalid=sum(r['teams'][r['candidate_side']]['invalid_action_deaths'] for r in resource)
 final_long=[r['metrics']['teams']['B' if r['reverse'] else 'A']['longestDragon'] for r in final]
 map_order=sorted(by_map,key=lambda m:sum(r['score'] for r in by_map[m])/len(by_map[m]),reverse=True)
 lines=['# v11 research report — 2026-09-26','',
 '**Decision: no champion promotion. Keep the established v10.1/v10.2 options for competition.**',
 'The final new candidate is bot-v11.1 (R8). It implements the requested navigation features, but did not beat every requested baseline in fresh validation. No official submission was made.','',
 '## Exact versions and lineage','',
 f"- v11.1: `{current['bot-v11.1']['path']}`; SHA-256 `{current['bot-v11.1']['sha256']}`.",
 f"- v11: `{current['bot-v11']['path']}`; SHA-256 `{current['bot-v11']['sha256']}`.",
 '- Parent: frozen v10.2; v11.1 restores v10.1 food/exploration coefficients. R8 adds the small opening-trade correction. The direct v10.1 rebase (R7) was rejected and archived.',
 '- Every tested revision has a hash-named source tarball. The two ZIP files contain the exact final working sources. Existing bots, user scripts, replays and old experiment records were preserved.','',
 '## Implementation','',
 '- Center attraction stops by round 25, or earlier after observing a barren/occupied center. Resource-aware deterministic sector targets spread workers.',
 '- Body-aware portal food routes, visible body reconstruction for new split children, and exact intermediate sprint history.',
 '- Rectangular Hamiltonian patrols require whole-body cyclic ordering. Expansion is accepted only when ordering survives. The currently occupied tail is forbidden.',
 '- Four-move virtual-body continuation and bounded two-to-five-step emergency sprints (one-to-four segments spent), using visible sprint destinations. Other dragons and unseen cells prevent a global safety guarantee.',
 '- Protocol 3 terrain/pearl/portal messages preserve sample time; role and territory messages remain. New 5/6/7 tags avoid legacy packet collisions. Aggregate echoes do not certify a specific portal.',
 '- Existing growth, feeding, large-grower protection and reversal escape subsystems remain. R8 permits length<=3 direct trades with at least 2 living allies.','',
 '## Active parameters','',
 '|Parameter|v11|v11.1|','|---|---:|---:|',
 '|Center cutoff / weight|25 /45|25 /45|','|Growth specialisation|250|250|',
 '|Worker exploration weight / cap|10 /800|6 /360|','|Immediate food score worker / grower|2500 /3800|850 /2800|',
 '|Sector gradient weight|75|75|','|Lookahead / maximum sprint steps|4 /5|4 /5|',
 '|Patrol shortcuts|disabled|disabled|','|Feeding start / max feeder length / radius|380 /4 /3|380 /4 /3|',
 '|Direct head-trade maximum length|3|3|','|Direct trade opening restriction|round>=50 and allies>=6|allies>=2|',
 '|Sonar|4 directed rays/turn;1 terrain ray|same|','',
 '## Evidence','',
 f"- Entire session: **{len(rows)} parsed games**, {result['native_games']} native and {result['sandbox_games']} sandbox; zero unparsed outcomes. Maps discovered automatically: 11.",
 f"- Exact final v11.1: 110 fresh validation games, 22 mutual games, 8 diagnostic games and 4 sandbox games. Diagnostic maps/seeds are not silently pooled into all-map validation.",
 f"- Final validation W / D / L: **{tally(final)}**. Mutual vs final v11: **{tally(mutual)}**.",
 f"- Final-source internal Elo: **{data['internal_elo']}** (starts1000, local/order-dependent; unrelated to official ratings).",
 f"- Mean final longest: **{sum(final_long)/len(final_long):.2f}**; maximum final longest: **{max(final_long)}**. Elimination counts as zero.",
 f"- Sandbox maximum observed turn points: **{max_points:,.0f}**; CPU-limit events:{cpu}; invalid-action deaths:{invalid}.",
 '- Native fixtures: tail-before-movement collision, 500 ordered cycle steps, split-child body reconstruction, sprint intermediate history, multi-step escape paths, terrain/portal packet round trips, expiry, legacy packet rejection and small-trade protection.','',
 '|Opponent|Wins / draws / losses|Score rate|','|---|---:|---:|']
 for o,rs in by_opponent.items():lines.append(f"|{o}|{tally(rs)}|{sum(r['score'] for r in rs)/len(rs):.1%}|")
 lines+=['','These are one fresh seed per map/side for the exact final source. The earlier R6 source had another 110-game cohort but is a different code identity; those results are not pooled to inflate R8. Small margins are uncertain. **v5 was excluded by the requested opponent pool; the historical five-map-v5 gate is untested and not satisfied by these results.**','',
 '## Maps','',f"Strongest observed: {', '.join(map_order[:3])}. Weakest observed: {', '.join(map_order[-3:])}.",'',
 '|Map|Wins / draws / losses|','|---|---:|']
 for m,rs in sorted(by_map.items()):lines.append(f'|{m}|{tally(rs)}|')
 lines+=['','## Rejected experiments and limitations','',
 '- R1 navigation: mixed/negative results and peaks near 99.8M instructions. Caches improved observed resource margin in later runs.',
 '- Deeper eight-step planning and cycle shortcuts: did not beat the strict four-step screen.',
 '- Requiring productive patrol rectangles: 4–4 vs v11 and 3–5 vs v10.2 on sparse maps; no gain.',
 '- Early growers: reached 73 on Big Empty but finished5–11 in screening; rejected for cross-map regressions.',
 '- Strong center opening:7–9; rejected. Direct v10.1 rebase did not meet its selection score.',
 '- R8 opening trades: 6–1–1 on its diagnostic, but that improvement was not a guarantee on fresh seeds. The full fixed schedule was completed without selective extensions.',
 '- Large snakes can still die. Partial vision, unknown portal exits, enemy moves, blocked feeding routes and incomplete new-child body knowledge remain limits. A successful Hamiltonian fixture is not proof of invulnerability in Battlecode.',
 '- King length is not falsely inferred: several growers can coexist, so per-match king_length remains null. Feeding counters are logged attempts; confirmed transfer analysis requires replay-specific inspection.','',
 '## Pipeline and next work','',
 '`hypothesis → source snapshot → native fixtures → smoke → both-side screen → fresh all-map validation → sandbox → documented decision`',
 '',
 '- The evaluator discovers new maps and bots, hashes source and maps, preserves raw outputs/replays, checks CLI/replay agreement, and never submits or promotes automatically.',
 '- Strongest remaining ideas: test the untried interaction of v11’s higher food weights with v11.1’s small opening trades; time-expanded tail-reachability beyond local lookahead; resource-aware cycle acquisition without sacrificing early swarm; corridor reservations with turn-order-aware ally prediction; topology-limited early growers. Each needs an isolated, preregistered test.',
 '- Before any formal promotion, run repeated balanced v5 comparisons with a threshold specified before results and satisfy at least five maps, plus broader robustness.',
 '- Stopping condition: the fixed final validation is complete after multiple strategy families failed to produce a robust all-opponent winner. Keep these candidates as research artifacts; do not label them ultimate or replace the established bots automatically.','',
 'Research references: [John Tapsell’s cycle-ordering algorithm](https://johnflux.com/2015/05/02/nokia-6110-part-3-algorithms/) and [chynl’s actual Snake AI](https://github.com/chynl/snake).','',
 'Detailed ledger: `matches.jsonl`; cohorts: `summary.json`; dashboard: `../../reports/v11.html`; resource records: `resource_diagnostics.json`.']
 (OUT/'REPORT.md').write_text('\n'.join(lines)+'\n')
 print(json.dumps(result,indent=2))
if __name__=='__main__':main()
