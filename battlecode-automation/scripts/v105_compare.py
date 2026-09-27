#!/usr/bin/env python3
"""Compare two fixed experiment ledgers using only identically scheduled games.

Pairing requires the same opponent source, map bytes, seed, candidate side,
engine and sandbox setting. Errors and unmatched games are shown, not imputed.
The exact sign test and seeded paired bootstrap are exploratory: opponents/maps
may be correlated and repeated candidate selection is not accounted for.
"""
from __future__ import annotations

import argparse
import json
import math
import random
import statistics
from pathlib import Path


def ledger_path(value: Path) -> Path:
    return value / 'matches.jsonl' if value.is_dir() else value


def read_ledger(path: Path) -> list[dict]:
    with path.open() as stream:
        return [json.loads(line) for line in stream if line.strip()]


def pair_key(row: dict) -> tuple:
    opponent = row.get('opponent_name', Path(row['opponent']).name)
    map_name = row.get('map_name', Path(row['map']).name)
    opponent_hash = row.get('opponent_hash') or row['a_hash' if row['reverse'] else 'b_hash']
    return (opponent, map_name, int(row['seed']), bool(row['reverse']),
            opponent_hash, row['map_hash'], row['toolkit'], bool(row['sandbox']))


def index_rows(rows: list[dict]) -> tuple[dict, int]:
    indexed, errors = {}, 0
    for row in rows:
        if row.get('status') != 'ok':
            errors += 1
            continue
        key = pair_key(row)
        if key in indexed:
            raise ValueError(f'Duplicate schedule entry: {key[:4]}; do not merge replications without distinct seeds')
        if row['score'] not in (0, .5, 1):
            raise ValueError('Expected scores of 0, 0.5 or 1 from a validated result')
        indexed[key] = row
    return indexed, errors


def end_reason(row):
    return row.get('end_reason') or row['metrics']['result']['endReason']


def longest(row):
    if 'candidate_final' in row:
        return row['candidate_final']['longestDragon']
    return row['metrics']['teams']['B' if row['reverse'] else 'A']['longestDragon']


def exact_sign_test(positive: int, negative: int) -> float:
    """Two-sided binomial sign test, ignoring tied scores; null p = 0.5."""
    n = positive + negative
    if not n:
        return 1.0
    # Integer arithmetic avoids loss of precision in the binomial coefficient.
    return min(1.0, 2 * sum(math.comb(n, i) for i in range(min(positive, negative) + 1)) / (1 << n))


def percentile(sorted_values, quantile):
    position = (len(sorted_values) - 1) * quantile
    lower = math.floor(position)
    upper = math.ceil(position)
    return sorted_values[lower] + (position - lower) * (sorted_values[upper] - sorted_values[lower])


def paired_bootstrap(deltas: list[float], seed: int, samples: int):
    if not deltas:
        return None
    rng = random.Random(seed)
    estimates = sorted(statistics.mean(rng.choices(deltas, k=len(deltas))) for _ in range(samples))
    return [percentile(estimates, .025), percentile(estimates, .975)]


def side_statistics(rows: list[dict]) -> dict:
    return dict(wins=sum(r['score'] == 1 for r in rows),
                draws=sum(r['score'] == .5 for r in rows),
                losses=sum(r['score'] == 0 for r in rows),
                score_rate=statistics.mean(r['score'] for r in rows) if rows else None,
                elimination_wins=sum(end_reason(r) == 'teamEliminated' and r['score'] == 1 for r in rows),
                elimination_draws=sum(end_reason(r) == 'teamEliminated' and r['score'] == .5 for r in rows),
                elimination_losses=sum(end_reason(r) == 'teamEliminated' and r['score'] == 0 for r in rows),
                mean_final_longest=statistics.mean(longest(r) for r in rows) if rows else None)


def summarize(pairs: list[tuple[dict, dict]], seed: int, samples: int, bootstrap=True) -> dict:
    deltas = [candidate['score'] - baseline['score'] for baseline, candidate in pairs]
    positive, negative = sum(d > 0 for d in deltas), sum(d < 0 for d in deltas)
    base = side_statistics([b for b, c in pairs])
    candidate = side_statistics([c for b, c in pairs])
    return dict(paired_games=len(pairs), baseline=base, candidate=candidate,
                mean_score_delta=statistics.mean(deltas) if deltas else None,
                positive_pairs=positive, negative_pairs=negative, tied_pairs=sum(d == 0 for d in deltas),
                candidate_wins_baseline_nonwins=sum(c['score'] == 1 and b['score'] < 1 for b, c in pairs),
                baseline_wins_candidate_nonwins=sum(b['score'] == 1 and c['score'] < 1 for b, c in pairs),
                candidate_wins_baseline_losses=sum(c['score'] == 1 and b['score'] == 0 for b, c in pairs),
                baseline_wins_candidate_losses=sum(b['score'] == 1 and c['score'] == 0 for b, c in pairs),
                exact_two_sided_sign_p=exact_sign_test(positive, negative),
                bootstrap_95_score_delta=paired_bootstrap(deltas, seed, samples) if bootstrap else None,
                elimination_win_delta=candidate['elimination_wins'] - base['elimination_wins'],
                elimination_loss_delta=candidate['elimination_losses'] - base['elimination_losses'],
                mean_final_longest_delta=(candidate['mean_final_longest'] - base['mean_final_longest']
                                          if pairs else None))


def pct(value):
    return '—' if value is None else f'{100 * value:+.2f} pp'


def table(groups: dict[str, dict]) -> list[str]:
    lines = ['| Group | Paired games | Baseline W/D/L | Candidate W/D/L | Score delta | Better / worse / tied | Elimination W delta | Elimination L delta | Longest delta |',
             '|---|---:|---|---|---:|---|---:|---:|---:|']
    for name, s in groups.items():
        b, c = s['baseline'], s['candidate']
        length = '—' if s['mean_final_longest_delta'] is None else f"{s['mean_final_longest_delta']:+.2f}"
        lines.append(f"| {name} | {s['paired_games']} | {b['wins']}/{b['draws']}/{b['losses']} | "
                     f"{c['wins']}/{c['draws']}/{c['losses']} | {pct(s['mean_score_delta'])} | "
                     f"{s['positive_pairs']}/{s['negative_pairs']}/{s['tied_pairs']} | "
                     f"{s['elimination_win_delta']:+d} | {s['elimination_loss_delta']:+d} | {length} |")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True, help='Experiment directory or matches.jsonl')
    parser.add_argument('--candidate', type=Path, required=True, help='Experiment directory or matches.jsonl')
    parser.add_argument('--out', type=Path, help='Derived report directory; defaults inside candidate experiment')
    parser.add_argument('--bootstrap-seed', type=int, default=1052026)
    parser.add_argument('--bootstrap-samples', type=int, default=20000)
    args = parser.parse_args()
    if args.bootstrap_samples < 1000:
        parser.error('Use at least 1000 bootstrap samples')
    baseline_path, candidate_path = ledger_path(args.baseline.resolve()), ledger_path(args.candidate.resolve())
    if baseline_path == candidate_path:
        parser.error('Specify two different experiment ledgers')
    try:
        baseline, baseline_errors = index_rows(read_ledger(baseline_path))
        candidate, candidate_errors = index_rows(read_ledger(candidate_path))
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    keys = sorted(set(baseline) & set(candidate))
    pairs = [(baseline[key], candidate[key]) for key in keys]
    total = summarize(pairs, args.bootstrap_seed, args.bootstrap_samples)
    groups = {}
    for label, index in [('opponents', 0), ('maps', 1)]:
        groups[label] = {name: summarize([(baseline[k], candidate[k]) for k in keys if k[index] == name],
                                         args.bootstrap_seed, args.bootstrap_samples, bootstrap=False)
                         for name in sorted({k[index] for k in keys})}
    report = dict(baseline_ledger=str(baseline_path), candidate_ledger=str(candidate_path),
                  baseline_errors=baseline_errors, candidate_errors=candidate_errors,
                  baseline_unmatched=len(set(baseline) - set(candidate)),
                  candidate_unmatched=len(set(candidate) - set(baseline)),
                  bootstrap_seed=args.bootstrap_seed, bootstrap_samples=args.bootstrap_samples,
                  bootstrap_unit='paired game; not adjusted for map/opponent dependence or adaptive selection',
                  overall=total, **groups)
    out = args.out.resolve() if args.out else candidate_path.parent / ('comparison-vs-' + baseline_path.parent.name)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    interval = total['bootstrap_95_score_delta']
    lines = ['# Paired experiment comparison', '',
             f'Baseline: `{baseline_path}`. Candidate: `{candidate_path}`.', '',
             'Pairs match opponent source, map bytes, seed, side, engine and sandbox setting. '
             'Differences below are candidate minus baseline; lower elimination losses is better.', '',
             f"Errors: baseline {baseline_errors}, candidate {candidate_errors}. "
             f"Unmatched valid games: baseline {report['baseline_unmatched']}, candidate {report['candidate_unmatched']}.", '',
             *table({'TOTAL': total}), '',
             f"Exact two-sided sign-test p = {total['exact_two_sided_sign_p']:.6g}; "
             f"{total['positive_pairs']} improved, {total['negative_pairs']} worsened; tied scores excluded.", '',
             (f'Paired bootstrap 95% mean score-delta interval: [{pct(interval[0])}, {pct(interval[1])}]; '
              f'{args.bootstrap_samples} samples, fixed seed {args.bootstrap_seed}.' if interval else 'No matched games; no interval.'), '',
             f"Discordant wins: candidate wins / baseline nonwins {total['candidate_wins_baseline_nonwins']}; "
             f"baseline wins / candidate nonwins {total['baseline_wins_candidate_nonwins']}. "
             f"Strict W↔L flips: {total['candidate_wins_baseline_losses']} for candidate, "
             f"{total['baseline_wins_candidate_losses']} for baseline.", '',
             '**Exploratory evidence:** maps/opponents may be correlated; these statistics do not correct for '
             'repeated candidate selection or certify superiority on unseen opponents. Final validation needs fresh seeds/maps.', '',
             '## Opponents', '', *table(groups['opponents']), '', '## Maps', '', *table(groups['maps']), '']
    (out / 'REPORT.md').write_text('\n'.join(lines))
    print('\n'.join(lines[:lines.index('## Opponents')]))
    print(f'Full report: {out / "REPORT.md"}')
    return 0 if pairs else 1


if __name__ == '__main__':
    raise SystemExit(main())
