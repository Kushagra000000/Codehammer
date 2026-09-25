"""Bounded v10 evaluation; no candidate generation or automatic promotion.

Only v10/v10.1 against one another and frozen v8/v9. Exact source archives,
fresh seeds, both sides, strict CLI/replay agreement and append-only results.
"""
import argparse, concurrent.futures, hashlib, html, json, tarfile
from collections import defaultdict
from pathlib import Path
import research

OUT=research.ROOT/'battlecode-automation/experiments/v10'
ALLOWED={'bot-v8','bot-v9','bot-v10','bot-v10.1'}

def report():
    rows=research.ladder.read_jsonl(OUT/'matches.jsonl')
    ratings=defaultdict(lambda:1000.)
    for r in rows:
        if r['status']!='ok' or r.get('sandbox'):continue
        a=r['a_hash'];b=r['b_hash']
        if a==b:continue
        score=1-r['score'] if r['reverse'] else r['score']
        change=24*(score-1/(1+10**((ratings[b]-ratings[a])/400)))
        ratings[a]+=change;ratings[b]-=change
    groups={}
    for r in rows:
        key=(r['experiment'],r['candidate'],r['a_hash'] if not r['reverse'] else r['b_hash'])
        g=groups.setdefault(key,{'games':0,'W':0,'D':0,'L':0,'errors':0,'maps':{},'opponents':{},'lengths':[],'feeds':0,'runtime_failures':0})
        if r['status']!='ok':g['errors']+=1;continue
        g['games']+=1;kind={1:'W',.5:'D',0:'L'}[r['score']];g[kind]+=1
        for field,value in [('maps',Path(r['map']).stem),('opponents',Path(r['opponent']).name)]:
            x=g[field].setdefault(value,{'W':0,'D':0,'L':0});x[kind]+=1
        m=r['metrics']['teams']['B' if r['reverse'] else 'A']
        g['lengths'].append(m['longestDragon']);g['feeds']+=m['sacrifice_count'];g['runtime_failures']+=m['runtime_failures']
    data=[dict(experiment=k[0],candidate=k[1],fingerprint=k[2],**v) for k,v in groups.items()]
    for r in data:
        r['internal_elo']=round(ratings[r['fingerprint']],1)
        r['win_rate']=r['W']/r['games'] if r['games'] else None
        r['positive_maps']=[m for m,x in r['maps'].items() if x['W']>x['L']]
    (OUT/'summary.json').write_text(json.dumps(data,indent=2)+'\n')
    body='<meta charset="utf-8"><title>v10 focused evaluation</title><style>body{font:16px system-ui;margin:40px;background:#101923;color:#eee}td,th{padding:12px;border-bottom:1px solid #555}pre{white-space:pre-wrap}</style><h1>v10 focused evaluation</h1><p>Champion remains v9 until a documented decision. Revisions are separated by source fingerprint. Feeding counts measure logged attempts, not confirmed recipient growth. Sandbox games are labelled separately. Internal Elo starts at 1000, excludes sandbox games, and is not an official team rating. Positive maps means W &gt; L in this batch, not statistical validation. v5 is excluded by the requested four-bot scope.</p><table><tr><th>Revision</th><th>Games</th><th>W/D/L</th><th>Win rate</th><th>Internal Elo</th><th>Positive maps</th><th>Mean final longest</th><th>Feed attempts</th><th>Errors / runtime failures</th></tr>'
    for r in data:
        mean=sum(r['lengths'])/len(r['lengths']) if r['lengths'] else 0
        win=f'{100*r["win_rate"]:.1f}%' if r['win_rate'] is not None else '—'
        body+=f'<tr><td>{html.escape(r["experiment"])}<br><small>{r["fingerprint"][:12]}</small></td><td>{r["games"]}</td><td>{r["W"]}/{r["D"]}/{r["L"]}</td><td>{win}</td><td>{r["internal_elo"]}</td><td>{len(r["positive_maps"])}</td><td>{mean:.1f}</td><td>{r["feeds"]}</td><td>{r["errors"]} / {r["runtime_failures"]}</td></tr>'
    validation=next((r for r in data if r['experiment']=='v10.1-r4-validation'),None)
    if validation:
        comparisons='; '.join(f'{html.escape(name)}: {x["W"]}W / {x["D"]}D / {x["L"]}L' for name,x in validation['opponents'].items())
        body=body.replace('<table>',f'<p style="padding:18px;background:#203a35"><b>Latest all-map validation — {comparisons}.</b><br>Positive observed improvement; statistical superiority over v9 is not established. <a style="color:#9fe8cf" href="../experiments/v10/REPORT.md">Full report and limitations</a></p><table>',1)
    body+='</table><h2>Per-map and opponent results</h2><pre>'+html.escape(json.dumps(data,indent=2))+'</pre>'
    (research.ROOT/'battlecode-automation/reports/v10.html').write_text(body)
    import resource_diagnostics
    resource_diagnostics.OUT=OUT
    resource_diagnostics.collect()
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate',choices=['bot-v10','bot-v10.1'],default='bot-v10')
    p.add_argument('--opponents',nargs='+',default=['bot-v8','bot-v9'])
    p.add_argument('--maps',nargs='*');p.add_argument('--seeds',nargs='+',type=int,default=[7102])
    p.add_argument('--experiment',required=True);p.add_argument('--sandbox',action='store_true');p.add_argument('--dry-run',action='store_true')
    a=p.parse_args()
    if not set(a.opponents)<=ALLOWED or a.candidate in a.opponents:p.error('Only distinct v10/v10.1/v8/v9 comparisons are permitted')
    research.OUT=OUT;research.LEDGER=OUT/'matches.jsonl';OUT.mkdir(parents=True,exist_ok=True)
    bots={b.label:b.path for b in research.ladder.discover_bots()}
    maps=research.ladder.discover_maps()
    if a.maps:
        names={m.name for m in maps}
        if set(a.maps)-names:p.error('Unknown map: '+str(set(a.maps)-names))
        maps=[m for m in maps if m.name in a.maps]
    tasks=[(bots[a.candidate],bots[o],m.path,s,rev,a.sandbox,a.experiment) for o in a.opponents for m in maps for s in a.seeds for rev in [False,True]]
    done={r['id'] for r in research.ladder.read_jsonl(research.LEDGER)}
    tasks=[t for t in tasks if research.task_identity(t)[1] not in done]
    print('Scheduled',len(tasks),'games',flush=True)
    if a.dry_run:return
    # Capture the exact source BEFORE any game; never modify while this batch runs.
    for name in [a.candidate,*a.opponents]:
        directory=research.ROOT/bots[name];fingerprint=research.ladder.fingerprint_dir(directory)
        archive=OUT/f'{name}-{fingerprint}.tar.gz'
        if not archive.exists():
            with tarfile.open(archive,'w:gz') as tf:
                for f in sorted(directory.iterdir()):
                    if f.suffix in {'.cpp','.hpp','.toml','.md'}:tf.add(f,arcname=name+'/'+f.name)
    research.decoder()
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for i,r in enumerate(pool.map(research.run,tasks),1):
            research.ladder.append_jsonl(research.LEDGER,[r]);report()
            print(i,len(tasks),Path(r['map']).name,Path(r['opponent']).name,r.get('score'),r['status'],r.get('error',''),flush=True)
    print(json.dumps(report(),indent=2))
if __name__=='__main__':main()
