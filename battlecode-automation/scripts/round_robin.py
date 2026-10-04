#!/usr/bin/env python3
"""Single-leg C++ Battlecode round robin, with authoritative engine results."""
from __future__ import annotations
import argparse
import collections
import csv
import dataclasses
import datetime as dt
import hashlib
import html
import importlib.metadata
import itertools
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
SUPPORTED = '1.2.9'  # Engine adapter inspected against this release.


def schedule(bots, maps, seeds):
    # One fixed orientation per pair; no reverse fixtures or self matches.
    return [dict(id=f'{i:05d}', a=a, b=b, map=m, seed=s)
            for i, (a, b, m, s) in enumerate(
                ((a, b, m, s) for a, b in itertools.combinations(bots, 2)
                 for m in maps for s in seeds), 1)]


def fingerprint(path):
    digest = hashlib.sha256()
    for p in sorted(path.rglob('*')):
        if p.is_file() and not any(x.startswith('.') or x == '__pycache__'
                                   for x in p.relative_to(path).parts):
            digest.update(p.relative_to(path).as_posix().encode() + b'\0' + p.read_bytes())
    return digest.hexdigest()


def decision(r):
    if r['end_reason'] == 0:
        return 'elimination' if r['winner'] else 'mutual elimination'
    for key, label in [('queen', 'queen'), ('longest', 'longest dragon'), ('length', 'total length')]:
        if r['a_' + key] != r['b_' + key]:
            return label
    return 'draw'


def worker(task):
    # Run the installed engine itself, never a reconstructed rule simulator.
    from unswbc.engine import EngineModule, DEBUG_ALL, DEBUG_LIMITS
    from unswbc.run import _resolve
    from unswbc.sandbox import SandboxBot, WasmPool
    pools, live, teams = {}, {}, {}
    errors, notices, deaths = [], [], collections.Counter()
    points = {'A': [], 'B': []}
    row = {k: task[k] for k in ('id', 'a', 'b', 'map', 'seed')}
    started = time.monotonic()
    try:
        for team in 'AB':
            argv, directory, kind = _resolve(task['staged'][team], True)
            if kind != 'wasm':
                raise ValueError('This sandbox adapter supports C/C++/WASM bots only')
            pools[team] = WasmPool(argv, cwd=str(directory), key=f"{task['seed']:016x}-{team.lower()}")

        def spawn(did, init):
            team = next(line.split()[1] for line in init.decode().splitlines() if line.startswith('TEAM '))
            teams[did] = team
            live[did] = SandboxBot(pools[team], init=init, name=str(did))

        def reply(did, block):
            bot = live[did]
            response = bot.ask(block)
            if bot.error is not None:
                errors.append(dict(dragon=did, team=teams[did], error=str(bot.error)))
            metrics = getattr(bot, 'live', None)
            if metrics and metrics[0]:
                points[teams[did]].append(metrics[0])
            return response

        def death(did, round_number, reason):
            deaths[teams[did] + ':' + reason] += 1
            if reason == 'A':
                errors.append(dict(dragon=did, team=teams[did], error='Engine rejected or received no valid action'))
            print(f'Round {round_number + 1}: dragon {did}, team {teams[did]}, death {reason}', flush=True)
            bot = live.pop(did, None)
            if bot is not None:
                bot.stop()

        engine = EngineModule()
        result = engine.run(Path(task['map_file']).read_bytes(), reply, death, spawn,
                            notices.append, DEBUG_ALL | DEBUG_LIMITS, task['seed'])
        row.update(dataclasses.asdict(result))
        row['rounds'] += 1
        row['decided_by'] = decision(row)
        Path(task['replay']).write_bytes(engine.replay(task['a'], task['b']))
        row['status'] = 'bot_error' if errors else 'ok'
    except Exception as exc:
        row.update(status='error', error=f'{type(exc).__name__}: {exc}')
    finally:
        for bot in live.values():
            bot.stop()
        for pool in pools.values():
            pool.close()
    row.update(seconds=round(time.monotonic() - started, 3), bot_errors=errors,
               notices=notices, deaths=dict(deaths), points={})
    for team, values in points.items():
        values.sort()
        row['points'][team] = dict(turns=len(values), mean=sum(values) / len(values) if values else None,
                                  maximum=max(values, default=None),
                                  p99=values[max(0, (99 * len(values) + 99) // 100 - 1)] if values else None)
    Path(task['result']).write_text(json.dumps(row, indent=2) + '\n')


def standings(rows, bots):
    result = []
    for bot in bots:
        involved = [r for r in rows if bot in (r['a'], r['b'])]
        valid = [r for r in involved if r['status'] == 'ok']
        wins = sum(r['winner'] == ('A' if r['a'] == bot else 'B') for r in valid)
        draws = sum(r['winner'] is None for r in valid)
        n = len(valid)
        lengths = [r[('a_' if r['a'] == bot else 'b_') + 'queen'] for r in valid]
        result.append(dict(bot=bot, games=n, wins=wins, draws=draws, losses=n-wins-draws,
                           errors=len(involved)-n, win_rate=wins/n if n else None,
                           score_rate=(wins+.5*draws)/n if n else None,
                           mean_rounds=sum(r['rounds'] for r in valid)/n if n else None,
                           mean_final_queen=sum(lengths)/n if n else None,
                           queen_survival=sum(x > 0 for x in lengths)/n if n else None,
                           as_A=sum(r['a'] == bot for r in valid), as_B=sum(r['b'] == bot for r in valid)))
    return sorted(result, key=lambda r: (-(r['score_rate'] if r['score_rate'] is not None else -1), -r['wins'], r['bot']))


def reports(out, rows, manifest):
    bots = list(manifest['bots'])
    ranked = standings(rows, bots)
    summary = dict(scheduled=len(manifest['schedule']), attempted=len(rows),
                   successful=sum(r['status'] == 'ok' for r in rows),
                   errors=sum(r['status'] != 'ok' for r in rows), standings=ranked,
                   decisions=dict(collections.Counter(r['decided_by'] for r in rows if r['status'] == 'ok')),
                   by_map={m: standings([r for r in rows if r['map'] == m], bots) for m in manifest['maps']},
                   matchups={f'{a} vs {b}': standings([r for r in rows if {r['a'], r['b']} == {a, b}], [a, b])
                             for a, b in itertools.combinations(bots, 2)})
    (out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    (out/'matches.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows))
    for name, data in [('standings', ranked), ('matches', rows)]:
        if not data:
            continue
        fields = list(dict.fromkeys(k for r in data for k, v in r.items() if not isinstance(v, (dict, list))))
        with (out/f'{name}.csv').open('w', newline='') as f:
            writer = csv.DictWriter(f, fields, extrasaction='ignore'); writer.writeheader(); writer.writerows(data)
    esc = lambda v: html.escape(str(v))
    def fmt(v):
        return '—' if v is None else f'{v:.1%}'
    def table(data):
        return '<table><tr><th>Bot</th><th>W / D / L</th><th>Win rate</th><th>Score rate</th><th>Errors</th><th>A / B</th><th>Queen alive</th><th>Mean final queen</th><th>Mean rounds</th></tr>' + ''.join(
            f"<tr><td>{esc(r['bot'])}</td><td>{r['wins']} / {r['draws']} / {r['losses']}</td><td>{fmt(r['win_rate'])}</td><td>{fmt(r['score_rate'])}</td><td>{r['errors']}</td><td>{r['as_A']} / {r['as_B']}</td><td>{fmt(r['queen_survival'])}</td><td>{r['mean_final_queen'] if r['mean_final_queen'] is not None else '—'}</td><td>{round(r['mean_rounds'],1) if r['mean_rounds'] is not None else '—'}</td></tr>" for r in data) + '</table>'
    chart = ''.join(f"<div class='barrow'><b>{esc(r['bot'])}</b><div class='track'><div class='bar' style='width:{100*(r['win_rate'] or 0):.3f}%'></div></div><span>{fmt(r['win_rate'])} ({r['games']} games)</span></div>" for r in ranked)
    matches = ''.join(f"<tr><td>{esc(r['a'])} vs {esc(r['b'])}</td><td>{esc(r['map'])}</td><td>{r['seed']}</td><td>{esc(r['status'])}</td><td>{esc((r['a'] if r.get('winner') == 'A' else r['b']) if r.get('winner') else 'draw' if r['status'] == 'ok' else '—')}</td><td>{esc(r.get('decided_by','—'))}</td><td>{r.get('rounds','—')}</td><td>{r['seconds']}</td><td><a href='logs/{r['id']}.log'>log</a> · <a href='replays/{r['id']}.replay'>replay</a></td></tr>" for r in rows)
    page = f'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Battlecode round robin</title>
<style>body{{font:15px system-ui;max-width:1300px;margin:36px auto;padding:0 24px;background:#f5f7fa;color:#182738}}h1{{font-size:32px}}section{{background:white;padding:22px;margin:20px 0;border-radius:12px;overflow:auto}}table{{border-collapse:collapse;width:100%;white-space:nowrap}}td,th{{padding:10px;text-align:left;border-bottom:1px solid #dbe2e9}}.barrow{{display:grid;grid-template-columns:200px 1fr 170px;gap:14px;margin:18px 0;align-items:center}}.track{{background:#e9eef5;height:25px;border-radius:4px}}.bar{{background:#137b79;height:100%;border-radius:4px}}a{{color:#086caa}}summary{{cursor:pointer;font-weight:bold}}@media(max-width:650px){{.barrow{{grid-template-columns:130px 1fr}}.barrow span{{grid-column:2}}}}</style>
<h1>Battlecode round robin</h1><p>Engine {esc(manifest['engine'])} · {len(bots)} bots · {len(manifest['maps'])} maps · seeds {esc(manifest['seeds'])}</p>
<p>{summary['attempted']} / {summary['scheduled']} attempted · {summary['successful']} valid · {summary['errors']} errors. No match wall-time limit.</p>
<p>One orientation per pair. Side order can affect results; this is not a paired comparison. Win rate = wins / valid games; score rate = (wins + ½ draws) / valid games. Rankings use score rate. Errors are excluded, never counted as losses. Judge turn limits remain active.</p>
<section><h2>Win-rate chart</h2>{chart}</section><section><h2>Standings</h2>{table(ranked)}</section>
<section><h2>How games ended</h2><p>{esc(summary['decisions'])}</p></section>
<section><h2>Head-to-head</h2>{''.join('<h3>'+esc(k)+'</h3>'+table(v) for k,v in summary['matchups'].items())}</section>
<section><h2>Map breakdowns</h2>{''.join('<details><summary>'+esc(k)+'</summary>'+table(v)+'</details>' for k,v in summary['by_map'].items())}</section>
<section><h2>Every match</h2><table><tr><th>Pair</th><th>Map</th><th>Seed</th><th>Status</th><th>Engine winner</th><th>Decided by</th><th>Rounds</th><th>Seconds</th><th>Files</th></tr>{matches}</table></section>
<p>Raw final lengths, queen lengths, surviving dragons, death reasons, map notices, runtime errors and CPU-point statistics: <a href="matches.jsonl">matches.jsonl</a>. <a href="standings.csv">Standings CSV</a> · <a href="matches.csv">Matches CSV</a> · <a href="summary.json">Summary JSON</a></p></html>'''
    (out/'report.html').write_text(page)
    return summary


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--bots-dir', type=Path, default=ROOT/'bots/New/current')
    ap.add_argument('--maps-dir', type=Path, default=ROOT/'maps')
    ap.add_argument('--seeds', type=int, nargs='+', default=[1])
    ap.add_argument('--out', type=Path)
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--offline', action='store_true', help='Skip PyPI latest-version check; still require the supported engine')
    ap.add_argument('--worker', type=Path, help=argparse.SUPPRESS)
    args = ap.parse_args()
    if args.worker:
        worker(json.loads(args.worker.read_text())); return
    if len(set(args.seeds)) != len(args.seeds) or any(not 0 <= s < 2**64 for s in args.seeds):
        ap.error('Seeds must be distinct unsigned 64-bit integers')
    engine = importlib.metadata.version('unswbc')
    if engine != SUPPORTED:
        ap.error(f'Inspected engine adapter requires unswbc {SUPPORTED}; installed {engine}. Review the adapter before changing versions.')
    if not args.offline:
        try:
            url = 'https://pypi.org/pypi/unswbc/json'
            # macOS system curl uses the OS trust store; framework Python may lack CA files.
            if shutil.which('curl'):
                response = subprocess.run(['curl', '--fail', '--silent', '--show-error', '--max-time', '15', url],
                                          capture_output=True, text=True, check=True)
                latest = json.loads(response.stdout)['info']['version']
            else:
                with urllib.request.urlopen(url, timeout=15) as response:
                    latest = json.load(response)['info']['version']
        except Exception as exc:
            ap.error(f'Cannot check latest unswbc: {exc}. Use --offline only to deliberately skip the network check.')
        if latest != engine:
            ap.error(f'PyPI latest is {latest}; installed {engine}. Upgrade and review this engine adapter before running.')
    bots = {p.parent.name: p.parent.resolve() for p in sorted(args.bots_dir.glob('*/bot.toml'))}
    maps = {str(p.relative_to(args.maps_dir)): p.resolve() for p in sorted(args.maps_dir.rglob('*.map'))}
    if len(bots) < 2 or not maps:
        ap.error('Need at least two bot projects and one map')
    jobs = schedule(list(bots), list(maps), args.seeds)
    print(f'unswbc {engine}: {len(bots)} bots × {len(maps)} maps × {len(args.seeds)} seed(s) = {len(jobs)} single-leg matches', flush=True)
    if args.dry_run:
        for a,b in itertools.combinations(bots,2): print(f'  A={a}, B={b}')
        return
    out = (args.out or ROOT/'battlecode-automation/experiments'/dt.datetime.now().strftime('round-robin-%Y%m%d-%H%M%S')).resolve()
    out.mkdir(parents=True, exist_ok=False)
    for name in ('logs','replays','results'): (out/name).mkdir()
    manifest = dict(engine=engine, latest_checked=not args.offline, seeds=args.seeds, bots={n:fingerprint(p) for n,p in bots.items()},
                    maps={n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in maps.items()}, schedule=jobs, match_timeout=None)
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    rows=[]
    reports(out,rows,manifest)
    try:
        with tempfile.TemporaryDirectory(prefix='bc-round-robin-') as tmp:
            tmp=Path(tmp)
            for name,p in bots.items():
                shutil.copytree(p,tmp/(name+'-'+manifest['bots'][name][:16]),ignore=shutil.ignore_patterns('.unswbc-build','__pycache__','.git'))
            for job in jobs:
                print(f"[{job['id']}/{len(jobs)}] {job['a']} vs {job['b']} · {job['map']} · seed {job['seed']}", flush=True)
                task={**job, 'staged':{t:str(tmp/(job[k]+'-'+manifest['bots'][job[k]][:16])) for t,k in [('A','a'),('B','b')]},
                      'map_file':str(maps[job['map']]),'replay':str(out/'replays'/f"{job['id']}.replay"),'result':str(out/'results'/f"{job['id']}.json")}
                taskfile=tmp/'task.json';taskfile.write_text(json.dumps(task))
                started=time.monotonic()
                with (out/'logs'/f"{job['id']}.log").open('w') as log:
                    proc=subprocess.Popen([sys.executable,str(Path(__file__).resolve()),'--worker',str(taskfile)],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
                    try:
                        code=proc.wait()  # Intentionally no match timeout.
                    except KeyboardInterrupt:
                        os.killpg(proc.pid,signal.SIGKILL);proc.wait();raise
                result=Path(task['result'])
                if code==0 and result.exists(): row=json.loads(result.read_text())
                else: row={**job,'status':'error','error':f'worker exit {code}; see log','seconds':round(time.monotonic()-started,3)}
                rows.append(row);reports(out,rows,manifest)
                print(f"  {row['status']} · winner={row.get('winner')} · rounds={row.get('rounds')}",flush=True)
    except KeyboardInterrupt:
        print('\nInterrupted; completed results retained.',flush=True)
    finally:
        summary=reports(out,rows,manifest)
        for r in summary['standings']:
            print(f"{r['bot']}: {r['wins']}W {r['draws']}D {r['losses']}L, win rate {r['win_rate']:.1%}, errors {r['errors']}" if r['games'] else f"{r['bot']}: no valid games, errors {r['errors']}")
        print(f'Report: {out / "report.html"}')
    return 1 if len(rows)!=len(jobs) or summary['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
