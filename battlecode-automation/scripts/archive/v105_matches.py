#!/usr/bin/env python3
"""Bounded, reproducible v10.5 evaluation; no strategy generation or promotion.

Run with .battlecode-codex/bin/python. Uses the installed unswbc 1.1.0 engine
and the existing strict CLI/replay parser. Source and map snapshots are immutable.
By default there is one game per opponent/map/seed, alternating sides; use
--both-sides for paired validation. Reusing an experiment resumes its schedule,
but changing its source, maps, seeds or sides requires a new experiment ID.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import html
import json
import os
import re
import shutil
import statistics
import tempfile
from pathlib import Path

import autorun_matches as ladder


def atomic_text(path: Path, contents: str):
    """Publish a complete derived report; other evaluator processes never read half a JSON file."""
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=path.parent,
                                         prefix=f'.{path.name}.', suffix='.tmp', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(contents)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def resolve_bot(value: str, parser: argparse.ArgumentParser) -> Path:
    supplied = Path(value).expanduser()
    path = (supplied if supplied.is_absolute() else ladder.ROOT / supplied).resolve()
    if (path / 'bot.toml').is_file():
        return path
    matches = sorted({(ladder.ROOT / b.path).resolve()
                      for b in ladder.discover_bots() if b.label == value})
    if len(matches) != 1:
        parser.error(f'Bot {value!r} is missing or ambiguous; supply its project path')
    return matches[0]


def snapshot_bot(project: Path, out: Path) -> dict:
    digest = ladder.fingerprint_dir(project)
    destination = out / 'sources' / f'{project.name}-{digest}'
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Copy everything: helper headers and configuration belong to identity too.
        shutil.copytree(project, destination)
    if ladder.fingerprint_dir(destination) != digest:
        raise RuntimeError(f'Snapshot mismatch or source changed during copy: {project}')
    return dict(name=project.name, original=str(project), snapshot=str(destination), sha256=digest)


def snapshot_map(path: Path, out: Path) -> dict:
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    destination = out / 'maps' / digest / path.name
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    if hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f'Map snapshot mismatch: {path}')
    return dict(name=path.name, original=str(path), snapshot=str(destination), sha256=digest)


def record_statistics(rows: list[dict]) -> dict:
    result = dict(games=0, wins=0, draws=0, losses=0, errors=0,
                  elimination_wins=0, elimination_draws=0, elimination_losses=0,
                  round_limit_wins=0, round_limit_draws=0, round_limit_losses=0,
                  other_end_reasons={}, runtime_failures=0)
    lengths, rounds_won, rounds_lost, durations = [], [], [], []
    for row in rows:
        if row.get('status') != 'ok':
            result['errors'] += 1
            continue
        result['games'] += 1
        suffix = {1: 'wins', .5: 'draws', 0: 'losses'}[row['score']]
        result[suffix] += 1
        reason = row['end_reason']
        if reason == 'teamEliminated':
            result['elimination_' + suffix] += 1
            if suffix == 'wins':
                rounds_won.append(row['rounds'])
            elif suffix == 'losses':
                rounds_lost.append(row['rounds'])
        elif reason == 'roundLimit':
            result['round_limit_' + suffix] += 1
        else:
            result['other_end_reasons'][reason] = result['other_end_reasons'].get(reason, 0) + 1
        lengths.append(row['candidate_final']['longestDragon'])
        durations.append(row['seconds'])
        result['runtime_failures'] += row['candidate_runtime_failures']
    result['win_rate'] = result['wins'] / result['games'] if result['games'] else None
    result['score_rate'] = ((result['wins'] + .5 * result['draws']) / result['games']
                            if result['games'] else None)
    result['mean_final_longest'] = statistics.mean(lengths) if lengths else None
    result['maximum_final_longest'] = max(lengths, default=None)
    result['mean_elimination_win_round'] = statistics.mean(rounds_won) if rounds_won else None
    result['mean_elimination_loss_round'] = statistics.mean(rounds_lost) if rounds_lost else None
    result['elimination_win_rounds'] = rounds_won
    result['elimination_loss_rounds'] = rounds_lost
    result['mean_seconds'] = statistics.mean(durations) if durations else None
    return result


def enrich(row: dict, candidate: dict, opponent: dict, map_info: dict) -> dict:
    row.update(candidate_name=candidate['name'], opponent_name=opponent['name'],
               map_name=map_info['name'], candidate_hash=candidate['sha256'],
               opponent_hash=opponent['sha256'])
    row['outcome'] = ({1: 'W', .5: 'D', 0: 'L'}[row['score']]
                      if row.get('status') == 'ok' else 'ERROR')
    if row.get('status') == 'ok':
        metric = row['metrics']
        selected_side = 'B' if row['reverse'] else 'A'
        other_side = 'A' if row['reverse'] else 'B'
        selected = metric['teams'][selected_side]
        other = metric['teams'][other_side]
        row.update(end_reason=metric['result']['endReason'], rounds=metric['rounds'],
                   elimination_round=(metric['rounds'] if metric['result']['endReason'] == 'teamEliminated' else None),
                   candidate_final={key: selected[key] for key in ('longestDragon', 'totalLength', 'dragonCount')},
                   opponent_final={key: other[key] for key in ('longestDragon', 'totalLength', 'dragonCount')},
                   candidate_runtime_failures=selected.get('runtime_failures', 0),
                   opponent_runtime_failures=other.get('runtime_failures', 0))
    return row


def number(value, percent=False):
    return '—' if value is None else (f'{100 * value:.1f}%' if percent else f'{value:.1f}')


def statistics_table(groups: dict[str, dict]) -> list[str]:
    lines = ['| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |',
             '|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|']
    for name, s in groups.items():
        elimination = '/'.join(str(s['elimination_' + suffix]) for suffix in ('wins', 'draws', 'losses'))
        limit = '/'.join(str(s['round_limit_' + suffix]) for suffix in ('wins', 'draws', 'losses'))
        lines.append(f"| {name} | {s['wins']} | {s['draws']} | {s['losses']} | {s['errors']} | "
                     f"{number(s['win_rate'], True)} | {number(s['score_rate'], True)} | {elimination} | {limit} | "
                     f"{number(s['mean_final_longest'])} | {s['maximum_final_longest'] if s['maximum_final_longest'] is not None else '—'} |")
    return lines


def report_experiment(directory: Path, manifest: dict, rows: list[dict]):
    groups = {}
    for field, key in [('opponent_name', 'opponents'), ('map_name', 'maps')]:
        names = sorted({r[field] for r in rows})
        groups[key] = {name: record_statistics([r for r in rows if r[field] == name]) for name in names}
    summary = dict(experiment=manifest['experiment'], candidate=manifest['candidate']['name'],
                   candidate_hash=manifest['candidate']['sha256'], parent=manifest.get('parent'),
                   hypothesis=manifest['hypothesis'], scheduled=len(manifest['schedule']),
                   completed=len(rows), **record_statistics(rows), **groups)
    atomic_text(directory / 'summary.json', json.dumps(summary, indent=2) + '\n')
    lines = [f"# {manifest['experiment']}", '', f"Candidate: **{manifest['candidate']['name']}** (`{manifest['candidate']['sha256']}`).", '',
             f"Hypothesis: {manifest['hypothesis']}", '',
             f"Completed {len(rows)}/{len(manifest['schedule'])}; toolkit {manifest['toolkit']}; "
             f"{'both sides' if manifest['both_sides'] else 'one alternating side per opponent/map/seed'}.", '',
             'Errors remain errors. Elimination comes from the replay end reason, not an inferred round cutoff. '
             'Score rate counts a draw as half; these reports do not promote a champion.', '',
             *statistics_table({'TOTAL': record_statistics(rows)}), '',
             '## Opponents', '', *statistics_table(groups['opponents']), '',
             '## Maps', '', *statistics_table(groups['maps']), '',
             '## Matches', '',
             '| Map | Opponent | Seed | Side | Result | End reason | Round | Longest | Opponent longest | Seconds | Replay / error |',
             '|---|---|---:|---|---|---|---:|---:|---:|---:|---|']
    for r in sorted(rows, key=lambda x: (x['opponent_name'], x['map_name'], x['seed'], x['reverse'])):
        detail = (f"[replay]({ladder.ROOT / r['replay']})" if r.get('status') == 'ok'
                  else str(r.get('error', 'unknown error')).replace('|', '\\|').replace('\n', ' '))
        lines.append(f"| {r['map_name']} | {r['opponent_name']} | {r['seed']} | {'B' if r['reverse'] else 'A'} | "
                     f"{r['outcome']} | {r.get('end_reason', 'error')} | {r.get('rounds', '—')} | "
                     f"{r.get('candidate_final', {}).get('longestDragon', '—')} | "
                     f"{r.get('opponent_final', {}).get('longestDragon', '—')} | {r.get('seconds', '—')} | {detail} |")
    atomic_text(directory / 'REPORT.md', '\n'.join(lines) + '\n')
    return summary


def report_dashboard(out: Path):
    summaries = []
    for path in sorted(out.glob('experiments/*/summary.json')):
        summary = json.loads(path.read_text())
        summaries.append(summary)
    lines = ['# v10.5 experiments', '',
             'Experiments may use different opponents, seeds and maps; compare matched schedules before claiming improvement.', '',
             *statistics_table({s['experiment']: s for s in summaries}), '',
             'Each experiment directory contains its immutable manifest, source references, raw logs and append-only match ledger.']
    atomic_text(out / 'REPORT.md', '\n'.join(lines) + '\n')
    atomic_text(out / 'summary.json', json.dumps(summaries, indent=2) + '\n')
    tr = []
    for s in summaries:
        name = html.escape(s['experiment'])
        tr.append(f'<tr><td><a href="experiments/{name}/REPORT.md">{name}</a></td>'
                  f'<td>{html.escape(s["candidate"])}</td><td>{s["completed"]}/{s["scheduled"]}</td>'
                  f'<td>{s["wins"]}/{s["draws"]}/{s["losses"]}</td><td>{number(s["win_rate"], True)}</td>'
                  f'<td>{s["elimination_wins"]}/{s["elimination_losses"]}</td><td>{number(s["mean_final_longest"])}</td>'
                  f'<td>{s["errors"]}</td><td>{html.escape(s["hypothesis"])}</td></tr>')
    atomic_text(out / 'dashboard.html', '<!doctype html><meta charset="utf-8"><title>v10.5 experiments</title>'
        '<style>body{font:15px system-ui;margin:40px;background:#111827;color:#e5e7eb}table{border-collapse:collapse;width:100%}'
        'th,td{padding:12px;text-align:left;border-bottom:1px solid #374151}a{color:#7dd3fc}h1{font-size:30px}</style>'
        '<h1>v10.5 experiments</h1><p>Only compare matched schedules. No automatic champion promotion.</p>'
        '<table><thead><tr><th>Experiment</th><th>Candidate</th><th>Games</th><th>W/D/L</th><th>Win rate</th>'
        '<th>Elimination W/L</th><th>Mean longest</th><th>Errors</th><th>Hypothesis</th></tr></thead><tbody>' + ''.join(tr) + '</tbody></table>\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--parent', default='bot-v10.4.1', help='Lineage label; not automatically scheduled')
    parser.add_argument('--opponents', nargs='+', default=['bot-v9', 'bot-v10', 'bot-v10.1', 'bot-v10.3.1', 'bot-v11.1'])
    parser.add_argument('--maps', nargs='+', help='Map filenames or paths; omitted means every map under maps/')
    parser.add_argument('--seeds', nargs='+', type=int, default=[10501])
    parser.add_argument('--experiment', required=True)
    parser.add_argument('--hypothesis', default='Baseline replication')
    parser.add_argument('--out', type=Path, default=ladder.ROOT / 'battlecode-automation/experiments/v105',
                        help='Output directory (not a file containing a directory path)')
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--both-sides', action='store_true')
    parser.add_argument('--sandbox', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', args.experiment) or args.experiment in {'.', '..'}:
        parser.error('--experiment must be a simple directory name')
    if args.workers < 1 or len(set(args.seeds)) != len(args.seeds):
        parser.error('Use positive workers and distinct seeds')
    if args.out.expanduser().exists() and not args.out.expanduser().is_dir():
        parser.error('--out must be a directory, not a file containing a directory path')
    candidate = resolve_bot(args.candidate, parser)
    opponents = sorted({resolve_bot(value, parser) for value in args.opponents}, key=lambda p: (p.name, str(p)))
    if candidate in opponents:
        parser.error('Candidate cannot play itself')
    maps = ladder.discover_maps()
    if args.maps:
        chosen = []
        for value in args.maps:
            found = [m for m in maps if value in {m.name, m.path, str(ladder.ROOT / m.path)}]
            if len(found) != 1:
                parser.error(f'Map {value!r} is missing or ambiguous; use its repository-relative path')
            if found[0] not in chosen:
                chosen.append(found[0])
        maps = chosen
    maps = sorted(maps, key=lambda m: m.path)
    if not opponents or not maps:
        parser.error('Need at least one opponent and one map')
    # One orientation per map/opponent/seed, unless paired sides explicitly requested.
    schedule = [(o, m, seed, reverse) for oi, o in enumerate(opponents)
                for mi, m in enumerate(maps) for si, seed in enumerate(args.seeds)
                for reverse in ([False, True] if args.both_sides else [bool((oi + mi + si) % 2)])]
    print(f'{candidate.name}: {len(schedule)} games; {len(opponents)} opponents, {len(maps)} maps, '
          f'{len(args.seeds)} seeds; {"paired sides" if args.both_sides else "one alternating side"}', flush=True)
    if args.dry_run:
        for o, m, seed, reverse in schedule:
            print(f'{m.name} vs {o.name}: seed {seed}, selected side {"B" if reverse else "A"}')
        return 0

    import research
    out = args.out.expanduser().resolve()
    directory = out / 'experiments' / args.experiment
    directory.mkdir(parents=True, exist_ok=True)
    snapshots = {p: snapshot_bot(p, out) for p in [candidate, *opponents]}
    map_snapshots = {m.path: snapshot_map(ladder.ROOT / m.path, out) for m in maps}
    tasks = [(snapshots[candidate]['snapshot'], snapshots[o]['snapshot'], map_snapshots[m.path]['snapshot'],
              seed, reverse, args.sandbox, args.experiment) for o, m, seed, reverse in schedule]
    manifest = dict(experiment=args.experiment, candidate=snapshots[candidate], parent=args.parent,
                    opponents=[snapshots[p] for p in opponents], maps=list(map_snapshots.values()),
                    toolkit=research.TOOLKIT_VERSION, seeds=args.seeds, both_sides=args.both_sides,
                    sandbox=args.sandbox, hypothesis=args.hypothesis, schedule=tasks)
    manifest_path = directory / 'manifest.json'
    if manifest_path.exists():
        if json.loads(manifest_path.read_text()) != json.loads(json.dumps(manifest)):
            parser.error('Experiment identity changed; choose a new --experiment ID instead of overwriting history')
    else:
        atomic_text(manifest_path, json.dumps(manifest, indent=2) + '\n')
    research.OUT = directory
    research.LEDGER = directory / 'matches.jsonl'
    rows = ladder.read_jsonl(research.LEDGER)
    done = {r['id'] for r in rows}
    pending = []
    for task, (opponent, map_info, seed, reverse) in zip(tasks, schedule):
        _, identity, _, _ = research.task_identity(task)
        if identity not in done:
            pending.append((task, opponent, map_info, identity))
    print(f'{len(pending)} pending; results: {directory}', flush=True)
    report_experiment(directory, manifest, rows)
    report_dashboard(out)
    if pending:
        research.decoder()
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
            futures = {pool.submit(research.run, task): (task, opponent, map_info, identity)
                       for task, opponent, map_info, identity in pending}
            for future in concurrent.futures.as_completed(futures):
                task, opponent, map_info, identity = futures[future]
                try:
                    row = future.result()
                except Exception as exc:
                    row = dict(id=identity, candidate=task[0], opponent=task[1], map=task[2], seed=task[3],
                               reverse=task[4], sandbox=task[5], experiment=task[6], status='error',
                               error=f'{type(exc).__name__}: {exc}')
                row = enrich(row, snapshots[candidate], snapshots[opponent], map_snapshots[map_info.path])
                ladder.append_jsonl(research.LEDGER, [row])
                rows.append(row)
                report_experiment(directory, manifest, rows)
                report_dashboard(out)
                print(f"{len(rows)}/{len(tasks)} {map_info.name} vs {opponent.name} seed={task[3]} "
                      f"side={'B' if task[4] else 'A'} {row['outcome']} {row.get('end_reason', '')} "
                      f"r={row.get('rounds', '—')} longest={row.get('candidate_final', {}).get('longestDragon', '—')} "
                      f"seconds={row.get('seconds', '—')} {row.get('error', '')}", flush=True)
    summary = report_experiment(directory, manifest, rows)
    report_dashboard(out)
    print('\n'.join(statistics_table({'TOTAL': summary})), flush=True)
    return int(any(row.get('status') != 'ok' for row in rows))


if __name__ == '__main__':
    raise SystemExit(main())
