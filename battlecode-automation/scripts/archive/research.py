"""Seeded, append-only research harness. Never promotes by Elo automatically."""
from pathlib import Path
import argparse, concurrent.futures, dataclasses, hashlib, json, os, re, shutil, subprocess, tempfile, time, zipfile
import autorun_matches as ladder
from importlib.metadata import version

ROOT=ladder.ROOT
OUT=ROOT/'battlecode-automation/experiments/research-20260926'
CLI=ROOT/'.battlecode-codex/bin/unswbc'
LEDGER=OUT/'matches.jsonl'
TOOLKIT_VERSION=version('unswbc')
if TOOLKIT_VERSION != '1.1.0':
    raise RuntimeError('Research protocol pins unswbc 1.1.0; declare a new engine cohort before upgrading')

def decoder():
    import unswbc
    z=zipfile.ZipFile(Path(unswbc.__file__).parent/'replay-viewer.vsix')
    s=z.read('extension/dist/webview/webview.js').decode()
    marker='var Ff=z('
    if marker not in s:raise RuntimeError('Toolkit viewer changed; inspect parser before use')
    dest=OUT/'decoder.cjs'
    dest.write_text(s[:s.index(marker)]+';globalThis.decodeBattlecode=Pf;})();')
    return dest

def task_identity(task):
    candidate,opponent,mapfile,seed,reverse,sandbox,experiment=task
    a,b=(opponent,candidate) if reverse else (candidate,opponent)
    identity={'candidate':candidate,'opponent':opponent,'map':mapfile,'seed':seed,'reverse':reverse,'sandbox':sandbox,'experiment':experiment,'toolkit':'1.1.0'}
    for k,p in [('a',a),('b',b)]:identity[k+'_hash']=ladder.fingerprint_dir(ROOT/p)
    identity['map_hash']=hashlib.sha256((ROOT/mapfile).read_bytes()).hexdigest()
    key=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()
    return identity,key,a,b

def run(task):
    candidate,opponent,mapfile,seed,reverse,sandbox,experiment=task
    identity,key,a,b=task_identity(task)
    row={**identity,'id':key,'a':a,'b':b,'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()}
    raw=OUT/'raw'/key;raw.mkdir(parents=True,exist_ok=True)
    replay=ROOT/'replays'/f'research-{key}.replay'
    started=time.monotonic()
    with tempfile.TemporaryDirectory(prefix='bc-research-') as td:
        td=Path(td)
        # Toolkit 1.1.0's wasm cache hashes .cpp but omits included headers.
        # Put the full source fingerprint into the project basename so a
        # header-only change can never reuse an older wasm build.
        alias_a='a-'+identity['a_hash'][:16];alias_b='b-'+identity['b_hash'][:16]
        for alias,p in [(alias_a,a),(alias_b,b)]:shutil.copytree(ROOT/p,td/alias)
        cmd=[str(CLI),'run',str(ROOT/mapfile),alias_a,alias_b,'--seed',str(seed),'-o',str(replay)]
        if sandbox:cmd.append('--sandbox')
        env={**os.environ,'UNSWBC_NO_UPDATE':'1','UNSWBC_NO_VSCODE':'1','NO_COLOR':'1'}
        try:
            p=subprocess.run(cmd,cwd=td,env=env,text=True,capture_output=True,timeout=300)
            (raw/'stdout.txt').write_text(p.stdout);(raw/'stderr.txt').write_text(p.stderr)
            row['exit_code']=p.returncode
            # Exact current-toolkit result sentence only; logs cannot invent outcomes.
            wins=re.findall(r'^team ([AB]) wins after (\d+) rounds .*$',p.stdout,re.M)
            metrics=subprocess.run(['node',str(ROOT/'battlecode-automation/scripts/replay_metrics.cjs'),str(OUT/'decoder.cjs'),str(replay)],capture_output=True,text=True,check=True)
            row['metrics']=json.loads(metrics.stdout)
            result=row['metrics']['result'];winner=result['winner']
            if p.returncode:raise RuntimeError('Nonzero CLI exit')
            if winner in ['A','B'] and (len(wins)!=1 or wins[0][0]!=winner):raise RuntimeError('CLI/replay disagreement or missing result')
            if winner not in ['A','B'] and not re.search(r'^.*draw.*after \d+ rounds',p.stdout,re.M):raise RuntimeError('Unrecognised draw sentence')
            row['winner']=winner;row['score']=.5 if winner not in ['A','B'] else float(winner==('B' if reverse else 'A'))
            row['status']='ok';row['replay']=str(replay.relative_to(ROOT))
        except Exception as e:
            row['status']='error';row['error']=str(e)
            if isinstance(e,subprocess.TimeoutExpired):
                (raw/'stdout.txt').write_bytes(e.stdout or b'');(raw/'stderr.txt').write_bytes(e.stderr or b'')
    row['seconds']=round(time.monotonic()-started,2)
    (raw/'result.json').write_text(json.dumps(row,indent=2)+'\n')
    return row

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--candidate',required=True);ap.add_argument('--opponents',nargs='+',default=['bot-v5','bot-v8']);ap.add_argument('--maps',nargs='*');ap.add_argument('--seeds',nargs='+',type=int,default=[101]);ap.add_argument('--experiment',required=True);ap.add_argument('--sandbox',action='store_true');ap.add_argument('--dry-run',action='store_true');args=ap.parse_args()
    bots={b.label:b.path for b in ladder.discover_bots()};maps=ladder.discover_maps()
    from analyse_maps import analyse
    map_history=OUT/'map_inventory.jsonl'
    known={r['sha256'] for r in ladder.read_jsonl(map_history)}
    for m in maps:
        digest=hashlib.sha256((ROOT/m.path).read_bytes()).hexdigest()
        if digest not in known:ladder.append_jsonl(map_history,[analyse(ROOT/m.path)])
    if args.maps:maps=[m for m in maps if m.name in args.maps]
    assert maps and args.candidate in bots
    tasks=[(bots[args.candidate],bots[o],m.path,s,rev,args.sandbox,args.experiment) for o in args.opponents for m in maps for s in args.seeds for rev in [False,True]]
    old=ladder.read_jsonl(LEDGER)
    # Explicit replication requires a new experiment ID, and remains labelled.
    done={r['id'] for r in old}
    tasks=[t for t in tasks if task_identity(t)[1] not in done]
    print('Scheduled',len(tasks),'games',flush=True)
    if args.dry_run:return
    decoder()
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for i,row in enumerate(pool.map(run,tasks),1):
            ladder.append_jsonl(LEDGER,[row]);print(i,len(tasks),Path(row['map']).name,row.get('score'),row['status'],row['seconds'],flush=True)
            import research_report
            research_report.build()
            if row['status']!='ok':print(row.get('error'),flush=True)
    summarize()

def summarize():
    rows=ladder.read_jsonl(LEDGER);groups={}
    for r in rows:
        g=groups.setdefault(r['experiment'],{'candidate':r['candidate'],'games':0,'errors':0,'wins':0,'draws':0,'losses':0,'maps':{},'opponents':{}})
        if r['status']!='ok':g['errors']+=1;continue
        g['games']+=1;kind={1:'wins',.5:'draws',0:'losses'}[r['score']];g[kind]+=1
        for key,val in [('maps',Path(r['map']).name),('opponents',Path(r['opponent']).name)]:
            x=g[key].setdefault(val,{'wins':0,'draws':0,'losses':0});x[kind]+=1
    (OUT/'summary.json').write_text(json.dumps(groups,indent=2)+'\n')
    print(json.dumps(groups,indent=2))
if __name__=='__main__':main()
