"""Supplement immutable match rows with authoritative CLI CPU diagnostics."""
import json,re
from pathlib import Path
import autorun_matches as ladder
OUT=ladder.ROOT/'battlecode-automation/experiments/research-20260926'
def collect():
    result=[]
    for r in ladder.read_jsonl(OUT/'matches.jsonl'):
        if not r.get('sandbox'):continue
        text=(OUT/'raw'/r['id']/'stdout.txt').read_text()
        teams={t:{'cpu_limit_events':len(re.findall(r'bot \d+ \(team '+t+r'\) exceeded CPU limit',text)),'max_turn_points':None,'invalid_action_deaths':r.get('metrics',{}).get('teams',{}).get(t,{}).get('deaths',{}).get('A',0)} for t in ['A','B']}
        for t,value,suffix in re.findall(r'team ([AB]) points per turn:.*? max ([\d.]+)([kKM]?) ',text):
            teams[t]['max_turn_points']=float(value)*{'':1,'k':1000,'K':1000,'M':1000000}[suffix]
        result.append({'id':r['id'],'map':r['map'],'candidate_side':'B' if r['reverse'] else 'A','teams':teams,'score':r.get('score')})
    (OUT/'resource_diagnostics.json').write_text(json.dumps(result,indent=2)+'\n')
    return result
if __name__=='__main__':print(json.dumps(collect(),indent=2))
