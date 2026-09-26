"""Evidence dashboard for immutable v11 match cohorts. No promotion side effects."""
import json, html, math
from pathlib import Path
from collections import defaultdict
import research
OUT=research.ROOT/'battlecode-automation/experiments/v11'
def build():
 rows=research.ladder.read_jsonl(OUT/'matches.jsonl');groups={};elo=defaultdict(lambda:1000.)
 for r in rows:
  if r['status']=='ok' and not r.get('sandbox'):
   a,b=r['a_hash'],r['b_hash'];score=1-r['score'] if r['reverse'] else r['score']
   k=24*(score-1/(1+10**((elo[b]-elo[a])/400)));elo[a]+=k;elo[b]-=k
  key=(r['experiment'],r['candidate'],r['b_hash'] if r['reverse'] else r['a_hash'])
  g=groups.setdefault(key,dict(experiment=key[0],candidate=Path(key[1]).name,fingerprint=key[2],sandbox=r.get('sandbox',False),games=0,W=0,D=0,L=0,errors=0,maps={},opponents={},lengths=[],feeds=0,runtime_failures=0))
  if r['status']!='ok':g['errors']+=1;continue
  g['games']+=1;kind={1:'W',.5:'D',0:'L'}[r['score']];g[kind]+=1
  for field,name in [('maps',Path(r['map']).stem),('opponents',Path(r['opponent']).name)]:
   sub=g[field].setdefault(name,dict(W=0,D=0,L=0));sub[kind]+=1
  t=r['metrics']['teams']['B' if r['reverse'] else 'A'];g['lengths'].append(t['longestDragon']);g['feeds']+=t['sacrifice_count'];g['runtime_failures']+=t['runtime_failures']
 for g in groups.values():
  n=g['games'];p=(g['W']+.5*g['D'])/n if n else 0;z=1.96
  mid=(p+z*z/(2*n))/(1+z*z/n) if n else 0
  half=z*math.sqrt(p*(1-p)/n+z*z/(4*n*n))/(1+z*z/n) if n else 0
  g.update(score_rate=p,win_rate=g['W']/n if n else 0,score_interval=[mid-half,mid+half],internal_elo=round(elo[g['fingerprint']],1),mean_longest=sum(g['lengths'])/n if n else 0,positive_maps=[m for m,v in g['maps'].items() if v['W']>v['L']])
 data=list(groups.values());(OUT/'summary.json').write_text(json.dumps(data,indent=2)+'\n')
 esc=html.escape
 style='body{font:15px system-ui;background:#0e1723;color:#e1e9f4;max-width:1300px;margin:40px auto;padding:20px}h1{font-size:32px}td,th{text-align:left;padding:12px;border-bottom:1px solid #31445a}small,p{color:#aebed1}.good{color:#83e4b2}.bad{color:#ffaeae}details{margin:20px 0;padding:15px;background:#172538}pre{white-space:pre-wrap}a{color:#82c8ff}'
 doc=f'<meta charset="utf-8"><title>v11 evidence</title><style>{style}</style><h1>Did v11 improve?</h1><p>All cohorts are immutable source fingerprints. Native and judge sandbox results are separate. No automatic champion promotion. The historical five-map v5 gate is not tested in this opponent pool.</p><table><tr><th>Experiment</th><th>Games</th><th>W / D / L</th><th>Score</th><th>Internal Elo</th><th>Mean / max final longest</th><th>Positive maps</th><th>Errors</th></tr>'
 for g in data:
  cls='good' if g['W']>g['L'] else 'bad' if g['W']<g['L'] else ''
  doc+=f'<tr><td>{esc(g["experiment"])}<br><small>{g["fingerprint"][:12]} {"SANDBOX" if g["sandbox"] else "native"}</small></td><td>{g["games"]}</td><td class="{cls}">{g["W"]} / {g["D"]} / {g["L"]}</td><td>{g["score_rate"]:.1%}</td><td>{g["internal_elo"]}</td><td>{g["mean_longest"]:.1f} / {max(g["lengths"],default=0)}</td><td>{len(g["positive_maps"])}</td><td>{g["errors"]}</td></tr>'
 doc+='</table><h2>Current source identities</h2><p>Only validation and mutual matches using the current files appear below; diagnostic screens and sandbox games are excluded. Historical revisions above remain separate.</p>'
 for name in ['bot-v11','bot-v11.1']:
  directory=research.ROOT/'bots/candidate'/name
  if not directory.exists():continue
  fingerprint=research.ladder.fingerprint_dir(directory)
  current=[]
  for r in rows:
   if r['status']!='ok' or r.get('sandbox') or not ('validation' in r['experiment'] or 'mutual' in r['experiment']):continue
   candidate_hash=r['b_hash'] if r['reverse'] else r['a_hash']
   opponent_hash=r['a_hash'] if r['reverse'] else r['b_hash']
   if candidate_hash==fingerprint:current.append(r)
   elif opponent_hash==fingerprint:current.append({**r,'score':1-r['score'],'opponent':r['candidate']})
  opponents=defaultdict(lambda:defaultdict(int));maps=defaultdict(lambda:defaultdict(int))
  for r in current:
   kind={1:'W',.5:'D',0:'L'}[r['score']]
   opponents[Path(r['opponent']).name][kind]+=1;maps[Path(r['map']).stem][kind]+=1
  doc+=f'<h3>{esc(name)} · {fingerprint[:12]} · {len(current)} native games</h3><p>Unpromoted research candidate. v5 acceptance gate: untested.</p><table><tr><th>Opponent</th><th>W / D / L</th><th>Score</th></tr>'
  for opponent,v in sorted(opponents.items()):
   score=(v['W']+.5*v['D'])/sum(v.values());cls='good' if score>.5 else 'bad' if score<.5 else ''
   doc+=f'<tr><td>{esc(opponent)}</td><td>{v["W"]} / {v["D"]} / {v["L"]}</td><td class="{cls}">{score:.1%}</td></tr>'
  doc+='</table><p>Map results, pooled over the displayed opponents:</p><table><tr>'
  for m,v in sorted(maps.items()):
   cls='good' if v['W']>v['L'] else 'bad' if v['W']<v['L'] else ''
   doc+=f'<td class="{cls}">{esc(m)}<br>{v["W"]} / {v["D"]} / {v["L"]}</td>'
  doc+='</tr></table>'
 doc+='<p>Elo starts at 1000, includes native matches only, is order-dependent and is unrelated to official team ratings. Positive maps means observed W &gt; L, not statistical significance. Feeding counts below are logged attempts, not confirmed transfers.</p>'
 for g in reversed(data):
  doc+=f'<details><summary>{esc(g["experiment"])} — opponent and map breakdown</summary><pre>{esc(json.dumps({k:v for k,v in g.items() if k!="lengths"},indent=2))}</pre></details>'
 doc+='<p><b>Final decision: no promotion. v11.1 led against four older versions, but did not beat v10.2 or v11 in its final match sets.</b></p>'
 doc+='<p><a href="../experiments/v11/REPORT.md">Final report (when available)</a></p>'
 (research.ROOT/'battlecode-automation/reports/v11.html').write_text(doc)
 import resource_diagnostics
 resource_diagnostics.OUT=OUT;resource_diagnostics.collect()
 return data
if __name__=='__main__':build()
