"""Write an evidence report; never silently promote a bot."""
import json
from pathlib import Path
from collections import defaultdict
from research_report import build,wilson
from resource_diagnostics import collect
import autorun_matches as ladder

ROOT=ladder.ROOT;OUT=ROOT/'battlecode-automation/experiments/research-20260926'
def finalize():
    report=build();rows=ladder.read_jsonl(OUT/'matches.jsonl');candidate='bots/candidate/bot-v9-exp012'
    own=[r for r in rows if r['candidate']==candidate]
    valid=[r for r in own if r['experiment'].endswith('validation-v5')]
    robustness=[r for r in own if r['experiment']=='EXP012-validation-robustness']
    resource=[r for r in own if r['experiment']=='EXP012-resource']
    maps=defaultdict(list)
    for r in valid:maps[r['map']].append(r)
    map_rows=[];flags=[]
    for m,rs in maps.items():
        scores=[r['score'] for r in rs if r['status']=='ok'];bound=wilson(scores)
        passed=len(scores)>=8 and bound>.5 and all(any(r.get('score')==1 and r['reverse']==s for r in rs) for s in [False,True])
        map_rows.append({'map':Path(m).name,'games':len(scores),'W':scores.count(1),'D':scores.count(.5),'L':scores.count(0),'wilson_lower':bound,'passed':passed})
        if len(scores)>=8 and scores.count(1)==0:flags.append('No v5 wins on '+Path(m).name)
    for m in {r['map'] for r in robustness}:
        rs=[r for r in robustness if r['map']==m and r['opponent'].endswith('/bot-v8')]
        if len(rs)>=2 and not any(r.get('score')==1 for r in rs):flags.append('No incumbent wins in holdout side pair: '+Path(m).name)
    runtime=[]
    for r in own:
        if r['status']!='ok':runtime.append(r['id']);continue
        if r['metrics']['teams']['B' if r['reverse'] else 'A'].get('runtime_failures',0):runtime.append(r['id'])
    diagnostics=collect()
    for d in diagnostics:
        mine=d['teams'][d['candidate_side']]
        if mine['cpu_limit_events'] or mine['invalid_action_deaths']:
            runtime.append(d['id'])
    vs8=[r['score'] for r in own if r['status']=='ok' and r['opponent'].endswith('/bot-v8')]
    complete=len(valid)==88 and len(robustness)==12 and len(resource)==4
    passed=sum(m['passed'] for m in map_rows)>=5
    eligible=complete and passed and not flags and not runtime and bool(vs8) and sum(vs8)/len(vs8)>.5
    summary=next(r for r in report['bots'] if r['candidate']==candidate)
    decision={'candidate':candidate,'parent':'bots/candidate/bot-v9-exp001 (v8 equivalent)','complete':complete,'five_map_gate':passed,'beaten_maps':[m['map'] for m in map_rows if m['passed']],'regression_flags':flags,'runtime_failure_matches':runtime,'eligible_for_review':eligible,'CURRENT_CHAMPION':'bots/current/bot-v8','BEST_DISCOVERED_BOT':'bots/current/bot-v8','LATEST_CANDIDATE':'bots/candidate/bot-v9-exp015','best_new_candidate':candidate,'promotion':'pending review; incumbent unchanged','batch_games':len(rows),'candidate_summary':summary}
    state=ladder.load_json(OUT/'state.json',{})
    for key in ['CURRENT_CHAMPION','BEST_DISCOVERED_BOT','promotion']:
        if key in state:decision[key]=state[key]
    (OUT/'decision.json').write_text(json.dumps(decision,indent=2)+'\n')
    text='# Research report — 2026-09-26\n\n'
    text+=f'Current champion: **{Path(decision["CURRENT_CHAMPION"]).name}**. Promotion status: {decision["promotion"]}. Best new screening candidate: **v9-exp012**, parent v9-exp001 / v8. {len(rows)} research matches recorded; final validation complete: {complete}.\n\n'
    text+='The change replaces toroidal Manhattan pearl-objective distance with BFS over remembered kelp and paired portals. Unknown ordinary edges remain optimistic. Existing memory, safety tiers, splitting, sonar, spacing and combat architecture are preserved. King, feeding and frontier additions are disabled in this candidate.\n\n'
    text+='Parameters: split specialization 300, final growth cutoff 425, stable hash grower fraction 1/4, swarm target/max 64, pre-growth length 3, portal override probability 0%, portal rewards 250/35 (swarm/grower), unknown penalties 90/1200, exploration weights 6/2, visits 28/8, space penalties 1100/4800, enemy threats 260/1700, ally congestion 1600, direct trade max length 3, legacy sprint-trade ceiling 1000000, four sonar directions every turn. Reserved king/feeder controls are inactive, not implemented promises.\n\n'
    text+=f"Candidate participated in {summary['games']} native games plus {len(resource)} sandbox checks; current-batch native Elo {summary['elo']} (separate from historical Elo). Against v5 in native games: {summary['v5']}; against v8: {summary['incumbent']}. Mean final longest: {summary['mean_final_longest']:.2f}.\n\n"
    text+='## Independent v5 validation\n\nBoth sides for seeds 303, 404, 505, 606. Threshold fixed before testing: at least eight games, Wilson lower bound >0.5, and a win on each side.\n\n|Map|W/D/L|95% lower bound|Beaten|\n|---|---|---:|---|\n'
    for r in map_rows:text+=f"|{r['map']}|{r['W']}/{r['D']}/{r['L']}|{r['wilson_lower']:.3f}|{r['passed']}|\n"
    text+=f'\nFive-map requirement: **{passed}**. Regression flags: {flags or "none"}. Runtime/parse failures for candidate: {len(runtime)}.\n\n'
    text+='## Resource validation\n\nSandbox games are diagnostic and excluded from strategy Elo/win-rate tables.\n\n|Map|Candidate side|Maximum reported points|CPU limit events|Invalid actions|\n|---|---|---:|---:|---:|\n'
    for d in diagnostics:
        m=d['teams'][d['candidate_side']];text+=f"|{Path(d['map']).name}|{d['candidate_side']}|{m['max_turn_points']}|{m['cpu_limit_events']}|{m['invalid_action_deaths']}|\n"
    text+='\nThe frozen v5 opponent exceeded CPU limits on Big Empty; these resource wins are not independent evidence of strategic superiority. Earlier replay metric schemas omitted invalid-action death events from their runtime counter. The supplemental CLI diagnostics preserve and correct that limitation without rewriting old match rows.\n\n'
    text+='## Experiments\n\n|Experiment|Games|W/D/L|\n|---|---:|---|\n'
    for exp in dict.fromkeys(r['experiment'] for r in rows):
        xs=[r.get('score') for r in rows if r['experiment']==exp and r['status']=='ok'];text+=f'|{exp}|{len(xs)}|{xs.count(1)}/{xs.count(.5)}/{xs.count(0)}|\n'
    text+='\nEXP002 king alone regressed. EXP003 feeding and EXP004 frontier were promising but limited. Portal/adaptive/trade/echo sweeps did not surpass the navigation screen. EXP015 frontier+navigation lost 2–5–1 to navigation alone and was rejected. EXP014 king+feeding+frontier remains untested and preserved.\n\n'
    text+=f"Strongest maps (all candidate games): {', '.join(summary['strongest'])}. Weakest: {', '.join(summary['weakest'])}. These rankings include unequal screening exposure; use the balanced validation table for v5 conclusions.\n\n"
    text+='## Limits and next research\n\nThis is budget-limited research, not a plateau or optimality claim. External executable bots were unavailable; the replay endpoint required authentication and no API-key environment variable was present. Historical replays referenced in notes were absent. Root/global king consensus, alternative sacrifice methods/timings/sizes, sector assignment, risk-adjusted routing, structured sonar knowledge sharing and independent replication remain promising work.\n\n'
    text+='All original bots and historical result files remain preserved. Exact candidates have source manifests and tar snapshots; working changes are not represented as a new Git commit. Replays and full raw output remain local. Nothing was submitted.\n\n'
    text+='Candidate path: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/candidate/bot-v9-exp012`. Previous champion path: `/Users/kushagra/Desktop/UNSW_battlecode/Codehammer/bots/current/bot-v8`.\n'
    text+='\nBEST_DISCOVERED_BOT: `'+str(ROOT/decision['BEST_DISCOVERED_BOT'])+'`.\n'
    (OUT/'REPORT.md').write_text(text)
    print(json.dumps(decision,indent=2))
if __name__=='__main__':finalize()
