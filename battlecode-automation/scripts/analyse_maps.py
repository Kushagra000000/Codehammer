"""Discover and fingerprint maps; topology metrics exclude portal transport."""
from pathlib import Path
from collections import Counter
import hashlib,json,sys

ROOT=Path(__file__).resolve().parents[2]
def analyse(p):
    lines=[s.split() for s in p.read_text().splitlines() if s.strip() and not s.startswith('#')]
    w,h=map(int,next(s[1:] for s in lines if s[0]=='MAP'))
    edges={}; portals=set(); starts=[]; pearls=0
    for s in lines:
        if s[0]=='EDGE':
            i,k,pid=map(int,s[1:]);row,x=divmod(i,w+1)
            edges[(row%2,x%w,(row//2)%h)]=k
            if k==2:portals.add(pid)
        elif s[0]=='TILE':pearls+=int(s[4])>0
        elif s[0]=='DRAGON':starts.append({'id':len(starts),'team':int(s[1]),'length':int(s[2]),'head':list(map(int,s[3:5]))})
    adj={}
    for y in range(h):
        for x in range(w):
            candidates=[((0,x,y),(x,(y-1)%h)),((1,(x+1)%w,y),((x+1)%w,y)),((0,x,(y+1)%h),(x,(y+1)%h)),((1,x,y),((x-1)%w,y))]
            adj[x,y]=[q for e,q in candidates if edges.get(e,0)==0]
    seen=set();sizes=[]
    for v in adj:
        if v in seen:continue
        todo=[v];seen.add(v);n=0
        while todo:
            a=todo.pop();n+=1
            for b in adj[a]:
                if b not in seen:seen.add(b);todo.append(b)
        sizes.append(n)
    deg=Counter(map(len,adj.values()))
    sys.setrecursionlimit(max(sys.getrecursionlimit(),2*w*h+100))
    discovery={};low={};cuts=set()
    def visit(v,parent=None):
        discovery[v]=low[v]=len(discovery);children=0
        for u in adj[v]:
            if u==parent:continue
            if u not in discovery:
                children+=1;visit(u,v);low[v]=min(low[v],low[u])
                if parent is not None and low[u]>=discovery[v]:cuts.add(v)
            else:low[v]=min(low[v],discovery[u])
        if parent is None and children>1:cuts.add(v)
    for v in adj:
        if v not in discovery:visit(v)
    return {'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'width':w,'height':h,'symmetry':next((s[1] for s in lines if s[0]=='SYMMETRY'),None),'portal_pairs':len(portals),'kelp_density':sum(k==1 for k in edges.values())/(2*w*h),'pearl_capable_tiles':pearls,'starts':starts,'ordinary_components':sorted(sizes,reverse=True),'dead_ends':deg[1],'degree_two_corridor_tiles':deg[2],'ordinary_articulation_tiles':len(cuts),'family':('large-open' if w*h>=2000 and not portals else 'portal-partitioned' if portals and len(sizes)>1 else 'corridor' if deg[1]>0 else 'open-or-enclosed')}
if __name__=='__main__':
    rows=[analyse(p) for p in sorted((ROOT/'maps').rglob('*.map'))]
    out=ROOT/'battlecode-automation/experiments/research-20260926/maps.json';out.write_text(json.dumps(rows,indent=2)+'\n')
    text='# Map analysis — 2026-09-26\n\nOBSERVED from every discovered map. Components exclude portal transport; corridor counts are only a bottleneck proxy.\n\n|Map|Size|Symmetry|Portal pairs|Kelp edges %|Pearl tiles|Dead ends|Ordinary components|\n|---|---|---|---:|---:|---:|---:|---:|\n'
    for r in rows:text+=f"|{Path(r['path']).name}|{r['width']}×{r['height']}|{r['symmetry']}|{r['portal_pairs']}|{100*r['kelp_density']:.1f}|{r['pearl_capable_tiles']}|{r['dead_ends']}|{len(r['ordinary_components'])}|\n"
    (ROOT/'Knowledge/map_analysis.md').write_text(text+'\nFull hashes, starting dragons and component sizes: `battlecode-automation/experiments/research-20260926/maps.json`. Rerun `analyse_maps.py` after adding maps.\n')
    print(text)
