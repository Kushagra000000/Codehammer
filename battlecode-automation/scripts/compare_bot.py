#!/usr/bin/env python3
"""Compare one bot against bots/current, once per opponent/map, without promotion.

Uses the existing strict unswbc 1.1.0 CLI/replay parser (requires Node.js).
Run with .battlecode-codex/bin/python from the repository root.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

import autorun_matches as ladder


def write_report(out, rows, opponents):
    counts = {name: dict(W=0, D=0, L=0, ERROR=0) for name in opponents}
    for row in rows:
        counts[Path(row['opponent']).name][row['outcome']] += 1
    lines = ['# Manual bot comparison', '',
             'Results are from the selected bot’s perspective. One game per opponent/map;',
             'sides alternate across maps, with no reversed duplicate. This is a screening',
             'table, not statistical proof of superiority. Errors are excluded from win rate.', '',
             '| Opponent | Wins | Draws | Losses | Errors | Win rate |',
             '|---|---:|---:|---:|---:|---:|']
    totals = {k: sum(c[k] for c in counts.values()) for k in ('W', 'D', 'L', 'ERROR')}
    for name, c in [*counts.items(), ('TOTAL', totals)]:
        played = c['W'] + c['D'] + c['L']
        rate = f"{100*c['W']/played:.1f}%" if played else '—'
        lines.append(f"| {name} | {c['W']} | {c['D']} | {c['L']} | {c['ERROR']} | {rate} |")
    lines += ['', '| Map | Opponent | Selected bot side | Result |', '|---|---|---|---|']
    for row in sorted(rows, key=lambda r: (r['map'], r['opponent'])):
        lines.append(f"| {Path(row['map']).name} | {Path(row['opponent']).name} | "
                     f"{'B' if row['reverse'] else 'A'} | {row['outcome']} |")
    (out / 'results.md').write_text('\n'.join(lines) + '\n')
    (out / 'summary.json').write_text(json.dumps(counts, indent=2) + '\n')
    return '\n'.join(lines[:lines.index('| Map | Opponent | Selected bot side | Result |')])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bot', required=True, help='Project name in bots/current, or project path')
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--seed', type=int, default=10400)
    parser.add_argument('--sandbox', action='store_true', help='Use judge-like WebAssembly limits')
    parser.add_argument('--dry-run', action='store_true', help='List schedule without building or running')
    args = parser.parse_args()
    if args.workers < 1:
        parser.error('--workers must be positive')
    current = ladder.ROOT / 'bots/current'
    projects = sorted({p.parent.resolve() for p in current.rglob('bot.toml')})
    supplied = Path(args.bot).expanduser()
    target = (supplied if supplied.is_absolute() else ladder.ROOT / supplied).resolve()
    if not (target / 'bot.toml').is_file():
        matches = [p for p in projects if p.name == args.bot]
        if len(matches) != 1:
            parser.error('Specify a bot project path or an unambiguous name in bots/current')
        target = matches[0]
    target_hash = ladder.fingerprint_dir(target)
    # Exclude the selected project and byte-identical aliases of its source.
    opponents = [p for p in projects if p != target and ladder.fingerprint_dir(p) != target_hash]
    maps = ladder.discover_maps()
    if not opponents or not maps:
        parser.error('Need at least one distinct opponent in bots/current and one map under maps/')
    names = [p.name for p in [target, *opponents]]
    if len(names) != len(set(names)):
        parser.error('Project directory names must be unique for unambiguous reports')
    schedule = [(opponent, m, bool((oi + mi) % 2))
                for oi, opponent in enumerate(opponents) for mi, m in enumerate(maps)]
    print(f'{target.name}: {len(opponents)} opponents × {len(maps)} maps = {len(schedule)} games', flush=True)
    if args.dry_run:
        for opponent, m, reverse in schedule:
            print(f'{m.path}: {target.name} vs {opponent.name}; selected side {"B" if reverse else "A"}')
        return 0

    # Import only for execution: this harness explicitly pins engine/parser compatibility.
    import research
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + uuid4().hex[:8]
    out = ladder.ROOT / 'battlecode-automation/experiments/comparisons' / run_id
    out.mkdir(parents=True)
    research.OUT = out
    research.LEDGER = out / 'matches.jsonl'
    sources = {}
    bot_metadata = []
    for project in [target, *opponents]:
        dest = out / 'sources' / project.name
        shutil.copytree(project, dest)
        sources[project] = str(dest.relative_to(ladder.ROOT))
        bot_metadata.append(dict(name=project.name, original_path=str(project),
                                 snapshot=sources[project], sha256=ladder.fingerprint_dir(dest)))
    map_sources = {}
    map_metadata = []
    for m in maps:
        # Preserve subdirectories: two different maps may have the same filename.
        dest = out / 'maps' / Path(m.path).relative_to('maps')
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ladder.ROOT / m.path, dest)
        map_sources[m.path] = str(dest.relative_to(ladder.ROOT))
        map_metadata.append(dict(original_path=m.path, snapshot=map_sources[m.path],
                                 sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
    tasks = [(sources[target], sources[o], map_sources[m.path], args.seed, reverse,
              args.sandbox, run_id) for o, m, reverse in schedule]
    metadata = dict(selected=target.name, toolkit=research.TOOLKIT_VERSION, seed=args.seed,
                    sandbox=args.sandbox, bots=bot_metadata, maps=map_metadata,
                    schedule=tasks, policy='one game per opponent/map; alternating sides; no promotion')
    (out / 'manifest.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(f'Results: {out}', flush=True)
    research.decoder()
    rows = []
    opponent_names = [p.name for p in opponents]
    write_report(out, rows, opponent_names)
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(research.run, task): task for task in tasks}
        for future in concurrent.futures.as_completed(futures):
            task = futures[future]
            try:
                row = future.result()
            except Exception as exc:
                # Setup failures also remain explicit failures, never inferred draws/losses.
                row = dict(candidate=task[0], opponent=task[1], map=task[2], reverse=task[4],
                           status='error', error=f'{type(exc).__name__}: {exc}')
            row['outcome'] = {1: 'W', .5: 'D', 0: 'L'}[row['score']] if row['status'] == 'ok' else 'ERROR'
            ladder.append_jsonl(research.LEDGER, [row])
            rows.append(row)
            write_report(out, rows, opponent_names)
            print(f"{len(rows)}/{len(tasks)} {Path(row['map']).name} vs "
                  f"{Path(row['opponent']).name}: {row['outcome']} {row.get('error', '')}", flush=True)
    print(write_report(out, rows, opponent_names))
    print(f"Full table: {out / 'results.md'}")
    return int(any(row['status'] != 'ok' for row in rows))


if __name__ == '__main__':
    raise SystemExit(main())
