"""Current-batch dashboard and fixed promotion gate, separate from old Elo."""
from pathlib import Path
from collections import defaultdict
import html,json,math
import autorun_matches as ladder

ROOT=ladder.ROOT;OUT=ROOT/'battlecode-automation/experiments/research-20260926'
def wilson(scores):
    n=len(scores)
    if not n:return 0
    p=sum(scores)/n;z=1.96
    return (p+z*z/(2*n)-z*math.sqrt(p*(1-p)/n+z*z/(4*n*n)))/(1+z*z/n)

def build():
    rows=ladder.read_jsonl(OUT/'matches.jsonl');groups=defaultdict(list);rating=defaultdict(lambda:1000.)
    state=ladder.load_json(OUT/'state.json',{'CURRENT_CHAMPION':'bots/current/bot-v8'})
    for r in sorted(rows,key=lambda r:(r['experiment'],r['map'],r['opponent'],r['seed'],r['reverse'])):
        if r['status']!='ok':continue
        if r.get('sandbox'):continue # Diagnostic resource games do not rate strategy.
        a,b=r['candidate'],r['opponent'];groups[a].append(r)
        groups[b].append({**r,'candidate':b,'opponent':a,'score':1-r['score'],'reverse':not r['reverse']})
        if a==b:continue
        delta=24*(r['score']-1/(1+10**((rating[b]-rating[a])/400)))
        rating[a]+=delta;rating[b]-=delta
    report=[]
    for candidate,rs in groups.items():
        validation=[r for r in rs if 'validation' in r['experiment']]
        maps=defaultdict(list)
        for r in validation:
            if Path(r['opponent']).name=='bot-v5':maps[r['map']].append(r)
        beaten=[m for m,x in maps.items() if len(x)>=8 and wilson([r['score'] for r in x])>.5 and all(any(r['reverse']==s and r['score']==1 for r in x) for s in [False,True])]
        permap=defaultdict(list)
        for r in rs:permap[Path(r['map']).name].append(r['score'])
        ranked=sorted(permap,key=lambda m:sum(permap[m])/len(permap[m]),reverse=True)
        vs={}
        for opponent in ['bot-v5','bot-v8']:
            x=[r['score'] for r in rs if Path(r['opponent']).name==opponent]
            vs[opponent]={'games':len(x),'W':x.count(1),'D':x.count(.5),'L':x.count(0)}
        lengths=[r['metrics']['teams']['B' if r['reverse'] else 'A']['longestDragon'] for r in rs]
        report.append({'candidate':candidate,'games':len(rs),'score_rate':sum(r['score'] for r in rs)/len(rs),'elo':round(rating[candidate],1),'v5':vs['bot-v5'],'incumbent':vs['bot-v8'],'v5_maps_beaten':beaten,'passes_five_maps':len(beaten)>=5,'strongest':ranked[:3],'weakest':ranked[-3:],'mean_final_longest':sum(lengths)/len(lengths),'king_length':None,'sacrifices':None,'status':'incumbent' if candidate=='bots/current/bot-v8' else 'unpromoted'})
    for item in report:
        item['status']='champion source' if item['candidate']==state.get('champion_source',state['CURRENT_CHAMPION']) else 'evaluated'
    payload={'champion':state['CURRENT_CHAMPION'],'latest_candidate':state.get('LATEST_CANDIDATE',rows[-1]['candidate'] if rows else None),'total_records':len(rows),'errors':sum(r['status']!='ok' for r in rows),'bots':report}
    (OUT/'report.json').write_text(json.dumps(payload,indent=2)+'\n')
    esc=html.escape
    body='''<!doctype html><meta charset="utf-8"><title>Battlecode research</title><style>body{font:15px system-ui;background:#101923;color:#e6eef5;margin:40px}table{border-collapse:collapse;width:100%;background:#192633}th,td{padding:12px;border-bottom:1px solid #354353;text-align:left}h1{color:#6de0bb}.note{color:#b9c8d6}a{color:#6de0bb}</style><h1>Did the latest candidate improve?</h1>'''
    body+=f'<p>Champion: <b>{esc(Path(state["CURRENT_CHAMPION"]).name)}</b>. {len(rows)} recorded games; {payload["errors"]} errors. Promotion requires a recorded acceptance decision.</p><p class="note">Current-batch native Elo starts at 1000 and is separate from historical and official team ratings. Sandbox diagnostics are excluded from the table. Unequal schedules and update order affect Elo; direct comparisons and validation gates govern promotion. Unknown king/sacrifice metrics are not zero.</p>'
    body+='<table><tr><th>Candidate</th><th>Games</th><th>Score</th><th>Batch Elo</th><th>v5 W/D/L</th><th>v8 W/D/L</th><th>Validated v5 maps</th><th>Mean final longest</th><th>Strong / weak maps</th></tr>'
    for r in sorted(report,key=lambda r:r['elo'],reverse=True):
        label=Path(r['candidate']).name
        if r['status']=='champion source':label=Path(state['CURRENT_CHAMPION']).name+' (tested as '+label+') ★'
        cells=[label,str(r['games']),f'{100*r["score_rate"]:.1f}%',str(r['elo']),str(r['v5']),str(r['incumbent']),str(len(r['v5_maps_beaten'])),f'{r["mean_final_longest"]:.1f}',', '.join(r['strongest'])+' / '+', '.join(r['weakest'])]
        body+='<tr>'+''.join('<td>'+esc(c)+'</td>' for c in cells)+'</tr>'
    body+='</table><h2>Recent experiments</h2><ul>'
    for exp in dict.fromkeys(r['experiment'] for r in rows):
        xs=[r for r in rows if r['experiment']==exp and r['status']=='ok'];body+=f'<li>{esc(exp)}: {len(xs)} parsed games, {sum(r["score"] for r in xs):g} points</li>'
    body+='</ul><p>Raw results and source snapshots: ../experiments/research-20260926/</p>'
    body+='<h2>Available replay metrics</h2><p class="note">FEED counts are logged intentions, not guaranteed pearls collected. Missing measurements remain unknown. King length is unavailable for these replays.</p><table><tr><th>Bot</th><th>Recorded FEED intents</th><th>Missing FEED measurements</th><th>Mean successful portal transitions (measured games)</th></tr>'
    for candidate,rs in groups.items():
        metrics=[r['metrics']['teams']['B' if r['reverse'] else 'A'] for r in rs]
        feeds=[m.get('sacrifice_count') for m in metrics];portals=[m['portal_entries'] for m in metrics if m.get('portal_entries') is not None]
        body+='<tr><td>'+esc(Path(candidate).name)+'</td><td>'+str(sum(x for x in feeds if x is not None))+'</td><td>'+str(sum(x is None for x in feeds))+'</td><td>'+(f'{sum(portals)/len(portals):.1f}' if portals else 'unknown')+'</td></tr>'
    body+='</table><p><a href="../experiments/research-20260926/REPORT.md">Full report and acceptance evidence</a></p>'
    dest=ROOT/'battlecode-automation/reports/research.html';dest.parent.mkdir(exist_ok=True);dest.write_text(body)
    return payload
if __name__=='__main__':print(json.dumps(build(),indent=2))
