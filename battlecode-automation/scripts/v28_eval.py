#!/usr/bin/env python3
"""Budgeted, resumable v28 comparison using the installed unswbc 1.2.9 engine.

Default campaign: screen = 4 maps x 3 opponents x 2 sides (24 games), final =
9 disjoint maps x 3 opponents x 2 sides (54). All output directories share one
append-only campaign ledger capped at 80 submitted attempts, including errors
and interrupted attempts. No automatic retry can silently consume extra games.
Sources/maps are snapshotted and manifests must match exactly on resume.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import fcntl
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import uuid

import v221_eval as base

ROOT = base.ROOT
HARD_LIMIT = 80
SNAPSHOTS = (25, 50, 75, 100, 200, 380, 440)
SCREEN = ('default_small', 'maze', 'portals', 'stripes')
FINAL = ('arena', 'trophy', 'default', 'big_empty', 'stronghold', 'dilemma',
         'slithery_fight', 'schooltime', 'islands')
DEFAULT_OPPONENTS = ('../Top_20_bot', 'bots/New/current/bot-v27.3.0-leo',
                     'bots/New/current/bot-v27.2.0-leo')

# Extend the already inspected 1.2.9 authoritative decoder metrics, retaining its
# actor/movement validation for every consumed pearl and its early-loop proxy.
# Exact anchors intentionally fail closed if the reused evaluator changes.
def metrics_script() -> str:
    script = base.METRICS_JS
    replacements = [
        ('population:{}, first_split_round:null, splits:0, early_splits:0,',
         'population:{}, snapshots:{}, log_counts:{}, queen_death_round:null, queen_length_at_death:null, first_split_round:null, splits:0, early_splits:0,'),
        ('if (e.type===\'dragonDeath\') {\n    const s=stats[teams.get(e.id)];',
         '''if (e.type==='dragonLog' && teams.has(e.id)) {
    const tag=e.text.trim().split(/\\s+/)[0];
    if(tag) {const c=stats[teams.get(e.id)].log_counts; c[tag]=(c[tag]||0)+1;}
  }
  if (e.type==='dragonDeath') {
    const s=stats[teams.get(e.id)];
    if(e.id===0 || e.id===1) {
      s.queen_death_round=round+1;
      s.queen_length_at_death=bodies.getById(e.id)?.body.length ?? null;
    }'''),
        ('''for(const completed of [25,50,75]) {
  const state=d.match.roundAt(Math.min(completed,d.match.maxRound));
  for(const t of ['A','B']) stats[t].population[completed]=
    [...state.bodies.dragons.values()].filter(b=>b.team===t).length;
}''', '''for(const completed of [25,50,75,100,200,380,440]) {
  const state=d.match.roundAt(Math.min(completed,d.match.maxRound));
  for(const t of ['A','B']) {
    const alive=[...state.bodies.dragons.values()].filter(b=>b.team===t);
    stats[t].population[completed]=alive.length;
    stats[t].snapshots[completed]={
      completed_rounds:Math.min(completed,d.match.maxRound),
      population:alive.length,
      total_length:alive.reduce((n,b)=>n+b.body.length,0),
      longest:Math.max(0,...alive.map(b=>b.body.length)),
      queen_length:alive.find(b=>b.id===0 || b.id===1)?.body.length ?? 0
    };
  }
}'''),
    ]
    for old, new in replacements:
        if script.count(old) != 1:
            raise RuntimeError('v221 metrics source changed; inspect before extending')
        script = script.replace(old, new)
    return script


def prepare_decoder(out: Path):
    decoder, metrics = base.prepare_decoder(out)
    metrics.write_text(metrics_script())
    return decoder, metrics


def read_jsonl(path: Path):
    if not path.exists():
        return []
    rows = []
    for number, line in enumerate(path.read_text().splitlines(), 1):
        if line.strip():
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise RuntimeError(f'{path}:{number}: damaged JSONL; refusing ambiguous resume') from exc
    return rows


def append_jsonl(path: Path, row):
    with path.open('a') as stream:
        stream.write(json.dumps(row, sort_keys=True) + '\n')
        stream.flush()
        os.fsync(stream.fileno())


class Budget:
    """Lock-protected conservative submission ledger, shared across stage outputs."""
    def __init__(self, path: Path, limit: int):
        self.path, self.limit = path, limit

    def rows(self):
        return read_jsonl(self.path)

    def used(self):
        return sum(r.get('event') == 'submit' for r in self.rows())

    def submit(self, out: Path, game: str):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.with_suffix(self.path.suffix + '.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            used = self.used()
            if used >= self.limit:
                raise RuntimeError(f'Campaign budget exhausted: {used}/{self.limit} submitted attempts')
            attempt = uuid.uuid4().hex
            append_jsonl(self.path, dict(event='submit', attempt=attempt, out=str(out),
                                        game=game, time=time.time(), number=used+1))
            return attempt

    def finish(self, attempt: str, row):
        with self.path.with_suffix(self.path.suffix + '.lock').open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            append_jsonl(self.path, dict(event='finish', attempt=attempt,
                                        status=row['status'], time=time.time()))


def stage_project(project: Path, dest: Path, digest: str):
    if dest.exists():
        if base.fingerprint(dest) != digest:
            raise RuntimeError(f'Source snapshot changed: {dest}')
        return
    dest.mkdir(parents=True)
    for source in base.source_files(project):
        target = dest / source.relative_to(project)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    if base.fingerprint(dest) != digest:
        raise RuntimeError(f'Source changed while copying: {project}')


def summarize(out: Path, rows: list[dict], budget: Budget, planned: int):
    good = [r for r in rows if r['status'] == 'ok']
    groups = {}
    for r in good:
        us = r['candidate_side']; them = 'B' if us == 'A' else 'A'
        opponent = r['b'] if us == 'A' else r['a']
        for group in ('all', opponent):
            s = groups.setdefault(group, dict(games=0, wins=0, draws=0, losses=0,
                elimination_wins=0, elimination_losses=0,
                candidate_runtime_errors=0, opponent_runtime_errors=0,
                candidate_points_max=0, opponent_points_max=0,
                candidate_queen_deaths=0, opponent_queen_deaths=0,
                candidate_early_pearls=0, opponent_early_pearls=0,
                candidate_logs={}, opponent_logs={}, snapshots={}))
            s['games'] += 1
            verdict = 'draws' if r['winner'] is None else 'wins' if r['winner'] == us else 'losses'
            s[verdict] += 1
            if r['elimination'] and verdict != 'draws': s['elimination_'+verdict] += 1
            for who, team in (('candidate',us),('opponent',them)):
                m = r['metrics'][team]
                s[who+'_runtime_errors'] += sum(e['team']==team for e in r['runtime_errors'])
                s[who+'_points_max'] = max(s[who+'_points_max'],m['points_max'] or 0)
                s[who+'_queen_deaths'] += m['queen_death_round'] is not None
                s[who+'_early_pearls'] += m['early_pearls']
                for key, n in m['log_counts'].items():
                    logs=s[who+'_logs']; logs[key]=logs.get(key,0)+n
                for rnd, snap in m['snapshots'].items():
                    dest=s['snapshots'].setdefault(rnd,{}).setdefault(who,{})
                    for key in ('population','total_length','longest','queen_length'):
                        dest[key]=dest.get(key,0)+snap[key]
    for group in groups.values():
        for snap in group['snapshots'].values():
            for who in snap.values():
                for key in who: who[key] /= group['games']
    summary=dict(planned_games=planned, completed_games=len(good),
                 evaluation_errors=sum(r['status']!='ok' for r in rows),
                 attempted_rows=len(rows), campaign_attempts=budget.used(),
                 campaign_limit=budget.limit, groups=groups)
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    report=['# v28 paired comparison','',
      f"unswbc {base.TOOLKIT} sandbox; {len(good)}/{planned} completed games; "
      f"{summary['evaluation_errors']} evaluation errors; campaign attempts {summary['campaign_attempts']}/{budget.limit}.",'',
      '| Opponent | Games | W / D / L | Elimination W / L | Runtime errors ours / theirs | Max points ours / theirs |',
      '|---|---:|---|---|---|---|']
    for opponent,s in groups.items():
        report.append(f"| {Path(opponent).name} | {s['games']} | {s['wins']} / {s['draws']} / {s['losses']} | "
          f"{s['elimination_wins']} / {s['elimination_losses']} | {s['candidate_runtime_errors']} / {s['opponent_runtime_errors']} | "
          f"{s['candidate_points_max']:,} / {s['opponent_points_max']:,} |")
    report += ['', 'Snapshots in summary.json and matches.jsonl are after completed rounds 25/50/75/100/200/380/440; '
      'early means the first 75 completed rounds. Early endings retain terminal values and record the actual completed_rounds. '
      'Queen death rounds are one-based (null means alive at match end). Pearls are replay removals verified against actual actor movement. '
      'LOG counts record emitted intent, not proof an action succeeded; replay split/death/pearl events provide outcome evidence. '
      'The replay result is checked against engine queen/longest/total/population tiebreak fields. '
      'Submitted errors or interrupted attempts consume the shared budget and are never automatically retried.', '',
      '| Map | Opponent | Side | Seed | Result | Rounds | Queens at end ours / theirs | Queen death rounds ours / theirs | Early pearls ours / theirs |',
      '|---|---|:---:|---:|---|---:|---|---|---|']
    for r in good:
        us=r['candidate_side'];them='B' if us=='A' else 'A';c=r['metrics'][us];b=r['metrics'][them]
        opp=r['b'] if us=='A' else r['a']
        verdict='D' if r['winner'] is None else 'W' if r['winner']==us else 'L'
        report.append(f"| {Path(r['map']).stem} | {Path(opp).name} | {us} | {r['seed']} | {verdict} | {r['rounds']} | "
          f"{r[us.lower()+'_queen']} / {r[them.lower()+'_queen']} | {c['queen_death_round']} / {b['queen_death_round']} | "
          f"{c['early_pearls']} / {b['early_pearls']} |")
    for r in rows:
        if r['status']!='ok':report += ['',f"Evaluation error {r['id']}: {r.get('error','unknown')}"]
    (out/'report.md').write_text('\n'.join(report)+'\n')
    return summary


def main():
    ap=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--a',default='bots/New/current/bot-v28.0.0-Kushagra')
    ap.add_argument('--b',nargs='+',default=list(DEFAULT_OPPONENTS))
    ap.add_argument('--stage',choices=('screen','final','custom'),default='screen')
    ap.add_argument('--maps',nargs='+')
    ap.add_argument('--seeds',nargs='+',type=int,default=[28001])
    ap.add_argument('--out')
    ap.add_argument('--workers',type=int,default=4)
    ap.add_argument('--budget-ledger',default='battlecode-automation/experiments/v28/game-budget.jsonl')
    ap.add_argument('--max-games',type=int,default=80,help='campaign limit; cannot exceed80')
    ap.add_argument('--dry-run',action='store_true')
    ap.add_argument('--build-only',action='store_true')
    ap.add_argument('--retry-failed',action='store_true',help='explicitly spend new attempts on failed/interrupted games')
    ap.add_argument('--inspect-replay',type=Path,help='parse an existing replay without launching or budgeting games')
    args=ap.parse_args()
    if importlib.metadata.version('unswbc')!=base.TOOLKIT:ap.error('Requires installed unswbc1.2.9; no installs/upgrades')
    if args.inspect_replay:
        with tempfile.TemporaryDirectory(prefix='v28-parse-') as td:
            decoder,metrics=prepare_decoder(Path(td))
            subprocess.run(['node',str(metrics),str(decoder),str(args.inspect_replay.resolve())],check=True)
        return 0
    if not 1<=args.max_games<=HARD_LIMIT or args.workers<1:ap.error('max-games must be1..80 and workers positive')
    if any(s<0 or s>=1<<64 for s in args.seeds):ap.error('seeds must be unsigned64-bit')
    if len(set(args.seeds))!=len(args.seeds):ap.error('duplicate seeds')
    if args.stage=='custom' and not args.maps:ap.error('custom stage needs --maps')
    maps=args.maps or ['maps/'+m+'.map' for m in (SCREEN if args.stage=='screen' else FINAL)]
    if len(set(maps))!=len(maps):ap.error('duplicate maps')
    projects={p:(ROOT/p).resolve() for p in [args.a,*args.b]}
    if len(projects)!=len(args.b)+1 or len(set(projects.values()))!=len(projects):ap.error('candidate/opponents must be distinct')
    for p in [*projects.values(),*((ROOT/m).resolve() for m in maps)]:
        if not p.exists():ap.error(f'missing path: {p}')
    manifest=dict(schema=1,stage=args.stage,candidate=args.a,opponents=args.b,maps=maps,seeds=args.seeds,
      sandbox=True,toolkit=base.TOOLKIT,fingerprints={n:base.fingerprint(p) for n,p in projects.items()},
      maps_sha256={m:hashlib.sha256((ROOT/m).read_bytes()).hexdigest() for m in maps},
      harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
      base_harness_sha256=hashlib.sha256(Path(base.__file__).read_bytes()).hexdigest(),
      budget_ledger=str((ROOT/args.budget_ledger).resolve()))
    schedule=[]
    for m in maps:
        for seed in args.seeds:
            for opp in args.b:
                for a,b in ((args.a,opp),(opp,args.a)):
                    ident=hashlib.sha256(json.dumps([m,seed,a,b,manifest],sort_keys=True).encode()).hexdigest()[:20]
                    schedule.append(dict(id=ident,map=m,a=a,b=b,seed=seed,sandbox=True,
                                         candidate_side='A' if a==args.a else 'B'))
    budget=Budget(Path(manifest['budget_ledger']),args.max_games)
    print(f'{len(schedule)} scheduled games: {len(maps)} maps x{len(args.b)} opponents x{len(args.seeds)} seeds x2sides; '
          f'workers={args.workers}, campaign attempts={budget.used()}/{args.max_games}',flush=True)
    if len(schedule)>args.max_games:ap.error('schedule alone exceeds campaign limit')
    if args.dry_run:
        print(json.dumps(manifest,indent=2));return 0
    if not args.out:ap.error('--out is required except for --dry-run/--inspect-replay')
    out=(ROOT/args.out).resolve();out.mkdir(parents=True,exist_ok=True)
    # Prevent two drivers writing the same output while allowing distinct stages.
    with (out/'.run.lock').open('a') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:ap.error('another driver owns this output directory')
        return run(args,manifest,schedule,projects,out,budget)


def run(args,manifest,schedule,projects,out,budget):
    path=out/'manifest.json'
    if path.exists() and json.loads(path.read_text())!=manifest:
        raise RuntimeError('Output manifest differs; use a new --out after any source/map/harness change')
    path.write_text(json.dumps(manifest,indent=2)+'\n')
    (out/'schedule.json').write_text(json.dumps(schedule,indent=2)+'\n')
    rows={r['id']:r for r in read_jsonl(out/'matches.jsonl')}
    consumed={r['game'] for r in budget.rows() if r.get('event')=='submit' and r['out']==str(out)}
    todo=[r for r in schedule if rows.get(r['id'],{}).get('status')!='ok'
          and (r['id'] not in consumed or args.retry_failed)]
    if not args.build_only and budget.used()+len(todo)>budget.limit:
        raise RuntimeError(f'{len(todo)} pending games exceed remaining campaign budget {budget.limit-budget.used()}')
    os.environ.setdefault('XDG_CACHE_HOME',str(Path(tempfile.gettempdir())/'unswbc-v28-cache'))
    os.environ.setdefault('UNSWBC_WARM','1')
    artifacts=out/'decoder';artifacts.mkdir(exist_ok=True)
    decoder,metrics=prepare_decoder(artifacts)
    staged={}
    from unswbc.run import _resolve
    for name,project in projects.items():
        digest=manifest['fingerprints'][name];dest=out/'sources'/(project.name+'-'+digest[:16])
        stage_project(project,dest,digest);staged[name]=str(dest)
        _resolve(str(dest),True)
    for m,digest in manifest['maps_sha256'].items():
        dest=out/'maps'/(digest[:16]+'-'+Path(m).name);dest.parent.mkdir(exist_ok=True)
        data=(ROOT/m).read_bytes()
        if hashlib.sha256(data).hexdigest()!=digest:raise RuntimeError('Map changed during staging')
        if dest.exists() and dest.read_bytes()!=data:raise RuntimeError('Map snapshot changed')
        dest.write_bytes(data)
    if args.build_only:
        print('All four (or specified) source snapshots compiled; decoder loaded; no matches launched.');return 0
    print(f'{len(todo)} games to launch; {len(consumed)} previous attempts in this output. Interrupted/error attempts '
          'require explicit --retry-failed.',flush=True)
    pending={};queue=iter(todo);completed=0
    with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers) as executor:
        def launch():
            task=next(queue,None)
            if task is None:return False
            task=dict(task)
            attempt=budget.submit(out,task['id'])
            task.update(out=str(out),decoder=str(decoder),metrics=str(metrics),
                staged_a=staged[task['a']],staged_b=staged[task['b']],
                map_absolute=str(out/'maps'/(manifest['maps_sha256'][task['map']][:16]+'-'+Path(task['map']).name)))
            pending[executor.submit(base.play,task)]=(attempt,task)
            return True
        for _ in range(args.workers):
            if not launch():break
        while pending:
            done,_=concurrent.futures.wait(pending,return_when=concurrent.futures.FIRST_COMPLETED)
            for future in done:
                attempt,task=pending.pop(future)
                try:row=future.result()
                except Exception as exc:
                    row={k:task[k] for k in ('id','map','a','b','seed','sandbox','candidate_side')}
                    row.update(status='error',error=f'{type(exc).__name__}: {exc}',seconds=None)
                row['attempt']=attempt
                append_jsonl(out/'matches.jsonl',row);budget.finish(attempt,row);rows[row['id']]=row
                completed+=1
                summarize(out,list(rows.values()),budget,len(schedule))
                verdict='ERROR' if row['status']!='ok' else 'D' if row['winner'] is None else 'W' if row['winner']==row['candidate_side'] else 'L'
                print(f"{completed}/{len(todo)} {Path(row['map']).stem} vs{Path(row['b'] if row['candidate_side']=='A' else row['a']).name} "
                      f"side={row['candidate_side']} {verdict} rounds={row.get('rounds')} {row.get('seconds')}s {row.get('error','')}",flush=True)
                launch()
    summary=summarize(out,list(rows.values()),budget,len(schedule))
    print(json.dumps({k:v for k,v in summary.items() if k!='groups'},indent=2))
    failed=summary['evaluation_errors'] or summary['completed_games']!=len(schedule)
    failed=failed or any(g['candidate_runtime_errors'] or g['opponent_runtime_errors'] for g in summary['groups'].values())
    return int(bool(failed))


if __name__=='__main__':
    raise SystemExit(main())
