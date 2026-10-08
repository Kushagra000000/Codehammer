#!/usr/bin/env python3
"""Focused v28 queen regressions reusing the v25.1 protocol/engine fixtures.

These seven tiny scripted engine fixtures are unit/regression checks, not full
bot-vs-bot tournament matches. Worker/newborn behavior is intentionally excluded.
No bots are edited. Use --candidate-snapshot to reuse an immutable evaluator build.
"""
from __future__ import annotations

import argparse
import importlib.metadata
import os
from pathlib import Path
import tempfile

import test_v251 as fixture
import v28_eval as evaluation

ENGINE_FIXTURES = 0


def protocol_tests(candidate, baseline):
    from unswbc.sandbox import SandboxBot
    check, one_turn = fixture.check, fixture.one_turn
    for did in (0, 1):
        for length in (4, 5, 8, 10):
            check(one_turn(candidate, did=did, length=length) == 'SPLIT 2',
                  f'queen {did}, length {length}: detach exactly two')
    for round_num in (1, 75, 100, 380, 450):
        check(one_turn(candidate, length=8, round_num=round_num) == 'SPLIT 2',
              f'blocked queen waits in phase at round {round_num}')
    bot = SandboxBot(candidate, init=fixture.spawn_block(), name='v28-repeat')
    try:
        for rnd, length in enumerate((8, 6), 100):
            check(fixture.ask(bot, fixture.turn_block(length, round_num=rnd, curved=True)) == 'SPLIT 2',
                  f'same queen repeats wait: {length} -> {length-2}')
        check(fixture.ask(bot, fixture.turn_block(4, round_num=102, blocked=False, curved=True)).startswith('MOVE E'),
              'same queen resumes moving when exit opens')
    finally:
        bot.stop()
    for length in (2, 3):
        check(not one_turn(candidate, length=length).startswith('SPLIT'),
              f'queen length {length} cannot illegally repeat split')
    check(not one_turn(candidate, length=8, units=64).startswith('SPLIT'),
          'full population cap prevents queen split')
    check(not one_turn(candidate, length=8, units=63, round_num=100).startswith('SPLIT'),
          'uncrowned queen respects the reserved final slot')
    check(one_turn(candidate, length=8, units=63, round_num=450) == 'SPLIT 2',
          'crowned queen can use the final population slot')
    check(one_turn(candidate, length=8, blocked=False).startswith('MOVE E'),
          'unblocked post-seeding queen moves through its open exit')
    check(one_turn(baseline, length=8) == 'SPLIT 6',
          'v25.0 regression baseline exposes the large tail split')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate', default='bots/New/current/bot-v28.0.0-Kushagra')
    ap.add_argument('--candidate-snapshot', help='existing immutable snapshot whose fingerprint must equal candidate')
    ap.add_argument('--baseline', default='bots/New/current/bot-v25.0.0-omar')
    args = ap.parse_args()
    fixture.CHECKS = 0
    fixture.check(importlib.metadata.version('unswbc') == '1.2.9', 'tested toolkit version 1.2.9')
    os.environ.setdefault('XDG_CACHE_HOME', str(Path(tempfile.gettempdir()) / 'unswbc-v28-cache'))
    os.environ.setdefault('UNSWBC_WARM', '1')
    from unswbc.run import _resolve
    from unswbc.sandbox import WasmPool
    pools = []
    try:
        for index, name in enumerate((args.candidate, args.baseline)):
            source = (evaluation.ROOT / name).resolve()
            digest = evaluation.base.fingerprint(source)
            if index == 0 and args.candidate_snapshot:
                target = (evaluation.ROOT / args.candidate_snapshot).resolve()
                fixture.check(evaluation.base.fingerprint(target) == digest,
                              'candidate snapshot matches current frozen source')
            else:
                target = Path(tempfile.gettempdir()) / 'v28-queen-tests' / (source.name + '-' + digest[:16])
                evaluation.stage_project(source, target, digest)
            argv, directory, kind = _resolve(str(target), True)
            fixture.check(kind == 'wasm', f'{source.name} builds for sandbox')
            fixture.check(evaluation.base.fingerprint(target) == digest,
                          f'{source.name} source snapshot remains unchanged after build')
            pools.append(WasmPool(argv, cwd=str(directory), key='v28-queen-tests'))
        protocol_tests(*pools)
        # Reuse all seven real-engine scenarios, including the older regression
        # baseline; count them separately from protocol assertions and tournament games.
        original = fixture.engine_case
        def counted_engine_case(*args, **kwargs):
            global ENGINE_FIXTURES
            ENGINE_FIXTURES += 1
            return original(*args, **kwargs)
        fixture.engine_case = counted_engine_case
        try:
            fixture.engine_tests(*pools)
        finally:
            fixture.engine_case = original
    finally:
        for pool in pools:
            pool.close()
    print(f'{fixture.CHECKS} checks passed; {ENGINE_FIXTURES} tiny scripted engine fixtures '
          '(6 candidate, 1 historical baseline); 0 tournament matches.')


if __name__ == '__main__':
    main()
