"""Bounded v11 evaluation; no candidate generation or automatic promotion.

Only v11/v11.1 against one another and frozen v8/v9/v10/v10.1/v10.2. Exact source archives,
fresh seeds, both sides, strict CLI/replay agreement and append-only results.
"""
import argparse, concurrent.futures, hashlib, html, json, tarfile
from collections import defaultdict
from pathlib import Path
import research

OUT=research.ROOT/'battlecode-automation/experiments/v11'
ALLOWED={'bot-v8','bot-v9','bot-v10','bot-v10.1','bot-v10.2','bot-v11','bot-v11.1'}

def report():
    from v11_report import build
    return build()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate',choices=['bot-v11','bot-v11.1','bot-v10.2'],default='bot-v11')
    p.add_argument('--opponents',nargs='+',default=['bot-v8','bot-v9'])
    p.add_argument('--maps',nargs='*');p.add_argument('--seeds',nargs='+',type=int,default=[8201])
    p.add_argument('--experiment',required=True);p.add_argument('--sandbox',action='store_true');p.add_argument('--dry-run',action='store_true')
    a=p.parse_args()
    if not set(a.opponents)<=ALLOWED or a.candidate in a.opponents:p.error('Use distinct bots in the declared v8–v11.1 comparison pool')
    research.OUT=OUT;research.LEDGER=OUT/'matches.jsonl';OUT.mkdir(parents=True,exist_ok=True)
    bots={b.label:b.path for b in research.ladder.discover_bots()}
    maps=research.ladder.discover_maps()
    # Analyse newly discovered map identities without rewriting old records.
    from analyse_maps import analyse
    history=OUT/'map_inventory.jsonl'
    known={r['sha256'] for r in research.ladder.read_jsonl(history)}
    if not a.dry_run:
        for m in maps:
            digest=hashlib.sha256((research.ROOT/m.path).read_bytes()).hexdigest()
            if digest not in known:
                research.ladder.append_jsonl(history,[analyse(research.ROOT/m.path)]);known.add(digest)
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
    report()
if __name__=='__main__':main()
