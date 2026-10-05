#!/usr/bin/env python3
"""Queen wait-split regression tests using the real wasm bot and game engine.

Run: python3 battlecode-automation/scripts/test_v251.py
Requires the repository's installed unswbc 1.2.9 toolchain. Build copies and
compiler caches stay in temporary directories; the source bots are not modified.
"""
from __future__ import annotations

import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
W = H = 16
HEAD = (8, 8)
CURVE = [(8, 8), (7, 8), (6, 8), (6, 9), (6, 10), (7, 10), (8, 10), (9, 10)]
CHECKS = 0


def check(condition, message):
    global CHECKS
    CHECKS += 1
    if not condition:
        raise AssertionError(message)
    print(f"PASS {message}", flush=True)


def spawn_block(did=0):
    return f"ID {did}\nTEAM {'B' if did == 1 else 'A'}\nMAP {W} {H}\nUNIT_LIMIT 64\n".encode()


def turn_block(length, *, did=0, round_num=100, units=8, blocked=True, curved=False):
    """A valid 7x7 observation: head boxed by kelp and neck, or an open exit."""
    team = 'B' if did == 1 else 'A'
    body = CURVE[:length] if curved else [(8-i, 8) for i in range(length)]
    lines = [f"ROUND {round_num}", "DIR E", f"LENGTH {length}",
             f"UNIT_COUNT {units}", "NUM_MSGS 0"]
    visible = {(x, y) for y in range(5, 12) for x in range(5, 12)}
    lines += [f"{x} {y} 0 -1" for y in range(5, 12) for x in range(5, 12)]
    parts = []
    for i, (x, y) in enumerate(body):
        if (x, y) not in visible:
            continue
        delta = (1, 0) if i == 0 else (body[i-1][0]-x, body[i-1][1]-y)
        facing = {(1, 0): 'E', (-1, 0): 'W', (0, 1): 'S', (0, -1): 'N'}[delta]
        parts.append(f"{team} {did} {x} {y} {facing} {int(i == 0)}")
    lines += [f"DRAGON_BODIES {len(parts)}", *parts]
    horizontal = [['.'] * 7 for _ in range(8)]
    vertical = [['.'] * 8 for _ in range(7)]
    horizontal[3][3] = horizontal[4][3] = 'w'
    if blocked:
        vertical[3][4] = 'w'
    lines += [' '.join(row) for row in horizontal + vertical]
    return ('\n'.join(lines) + '\n').encode()


def action(response):
    actions = [line for line in response.decode().splitlines()
               if line.startswith(('MOVE ', 'SPLIT '))]
    if len(actions) != 1:
        raise AssertionError(f"Expected exactly one action: {response!r}")
    return actions[0]


def ask(bot, block):
    response = bot.ask(block)
    if bot.error is not None:
        raise AssertionError(f"Sandbox bot error: {bot.error}")
    return action(response)


def one_turn(pool, **kwargs):
    from unswbc.sandbox import SandboxBot
    bot = SandboxBot(pool, init=spawn_block(kwargs.get('did', 0)), name='fixture')
    try:
        return ask(bot, turn_block(**kwargs))
    finally:
        bot.stop()


def protocol_tests(candidate, baseline):
    from unswbc.sandbox import SandboxBot
    for did in (0, 1):
        for length in (4, 5, 8, 10):
            check(one_turn(candidate, did=did, length=length) == 'SPLIT 2',
                  f"queen {did}, length {length}: detach exactly two")
    for round_num in (1, 75, 100, 380, 450):
        check(one_turn(candidate, length=8, round_num=round_num) == 'SPLIT 2',
              f"blocked queen waits in phase at round {round_num}")
    bot = SandboxBot(candidate, init=spawn_block(), name='repeat')
    try:
        for r, length in enumerate((8, 6), 100):
            check(ask(bot, turn_block(length, round_num=r, curved=True)) == 'SPLIT 2',
                  f"same queen repeats wait: {length} -> {length-2}")
        check(ask(bot, turn_block(4, round_num=102, blocked=False, curved=True)).startswith('MOVE E'),
              "same queen resumes moving as soon as the exit opens")
    finally:
        bot.stop()
    # Identical observations must preserve actions outside the changed branch.
    unchanged = [
        ('short queen 2', dict(length=2)),
        ('short queen 3', dict(length=3)),
        ('team cap', dict(length=8, units=64)),
        ('existing reserved slot', dict(length=8, units=63)),
        ('queen open exit', dict(length=8, blocked=False)),
        ('queen ordinary seeding', dict(length=8, blocked=False, round_num=1)),
        ('worker escape', dict(length=8, did=10)),
        ('worker ordinary move', dict(length=3, did=10, blocked=False)),
        ('worker ordinary split', dict(length=8, did=10, blocked=False, round_num=1)),
    ]
    for name, kwargs in unchanged:
        new, old = one_turn(candidate, **kwargs), one_turn(baseline, **kwargs)
        check(new == old, f"unchanged {name}: {new}")
    check(one_turn(candidate, length=8, units=63, round_num=450) == 'SPLIT 2',
          "crowned queen can use her existing last-slot privilege")
    check(one_turn(baseline, length=8) == 'SPLIT 6',
          "regression fixture exposes the baseline's large tail split")


def scenario_map(length=8, curved=False, tail_boxed=False):
    """Queen's front is held by an ally; kelp blocks north/south, neck west."""
    body = CURVE[:length] if curved else [(8-i, 8) for i in range(length)]
    dragons = [(0, body), (1, [(3, 1), (2, 1), (1, 1)]),
               (0, [(9+i, 8) for i in range(6)])]
    lines = [f"MAP {W} {H}", "SYMMETRY xy", "MAP_NAME Queen Wait Fixture",
             f"TILE_COUNT {W*H}"]
    lines += [f"TILE {x} {y} 1000 1000" for y in range(H) for x in range(W)]
    lines.append(f"EDGE_COUNT {2*W*H}")
    for y in range(H):
        for orientation in range(2):
            for x in range(W):
                edge = (2*y + orientation)*(W+1) + x
                wall = orientation == 0 and x == 8 and y in (8, 9)
                if tail_boxed:
                    assert curved and length == 8
                    # The tail at (9,10) has kelp N/S/E and its neck to the west.
                    wall |= ((orientation == 0 and x == 9 and y in (10, 11)) or
                             (orientation == 1 and x == 10 and y == 10))
                lines.append(f"EDGE {edge} {int(wall)} -1")
    lines.append(f"DRAGON_COUNT {len(dragons)}")
    for team, segments in dragons:
        coords = ' '.join(f'{x} {y}' for x, y in segments)
        lines.append(f"DRAGON {team} {len(segments)} {coords}")
    lines.append('END')
    return ('\n'.join(lines) + '\n').encode()


def engine_case(pool, *, release=True, length=8, curved=False, tail_boxed=False):
    from unswbc.engine import EngineModule, DEBUG_ALL, DEBUG_LIMITS
    from unswbc.sandbox import SandboxBot
    bot = None
    queen_turns, deaths, notices = [], [], []

    def spawn(did, init):
        nonlocal bot
        if did == 0:
            bot = SandboxBot(pool, init=init, name='engine-queen')

    def reply(did, block):
        fields = block.decode().splitlines()
        round_num = int(fields[0].split()[1])
        if did == 0:
            response = bot.ask(block)
            if bot.error is not None:
                raise AssertionError(str(bot.error))
            head = next(line.split()[2:4] for line in fields
                        if line.startswith('A 0 ') and line.endswith(' 1'))
            queen_turns.append(dict(round=round_num, head=list(map(int, head)),
                                    length=int(fields[2].split()[1]), action=action(response)))
            return response
        if did == 1:
            move = 'E' if round_num < 5 else 'W'  # End the fixture after six rounds (0..5).
        elif did == 2:
            if round_num == 0 or (not release and round_num == 1):
                return b'SPLIT 2\nPROTOCOL 3\nENDTURN\n'
            # Three steps clear its body from the queen's exit too, not only its head.
            move = ('NNN' if round_num == 1 else 'N') if release else 'E'
        else:
            move = 'S'  # Children leave the old tail tiles.
        return f'MOVE {move}\nPROTOCOL 3\nENDTURN\n'.encode()

    try:
        engine = EngineModule()
        engine.run(scenario_map(length, curved, tail_boxed), reply,
                   lambda did, r, why: deaths.append((did, r, why)), spawn,
                   notices.append, DEBUG_ALL | DEBUG_LIMITS, 25101)
        check(not notices, f"engine accepts fixture actions: {notices}")
        return queen_turns, deaths
    finally:
        if bot is not None:
            bot.stop()


def engine_tests(candidate, baseline):
    for curved in (False, True):
        turns, deaths = engine_case(candidate, curved=curved)
        check([t['action'] for t in turns[:2]] == ['SPLIT 2', 'SPLIT 2'],
              f"engine repeated two-segment splits, complete body={curved}")
        check([t['length'] for t in turns[:3]] == [8, 6, 4] and
              all(t['head'] == list(HEAD) for t in turns[:3]),
              "engine keeps original queen head fixed and deducts only two each turn")
        check(turns[2]['action'].startswith('MOVE E') and turns[3]['head'] != list(HEAD),
              "engine moves queen after blocking ally moves away")
        check(not any(did == 0 for did, _, _ in deaths), "queen survives the released-blocker fixture")
        print(json.dumps(turns[:4]), flush=True)
    for length in (4, 5):
        turns, _ = engine_case(candidate, length=length)
        check(turns[0]['action'] == 'SPLIT 2' and turns[1]['length'] == length-2,
              f"engine minimum-size boundary {length} -> {length-2}")
        check(not turns[1]['action'].startswith('SPLIT'),
              f"queen of length {length-2} cannot issue an illegal repeat split")
    turns, _ = engine_case(candidate, release=False)
    check([t['action'] for t in turns[:3]] == ['SPLIT 2']*3 and
          [t['length'] for t in turns[:4]] == [8, 6, 4, 2],
          "engine repeats while blocked until only two queen segments remain")
    turns, deaths = engine_case(candidate, curved=True, tail_boxed=True)
    check([t['action'] for t in turns[:2]] == ['SPLIT 2', 'SPLIT 2'] and
          turns[2]['action'].startswith('MOVE E') and not any(did == 0 for did, _, _ in deaths),
          "boxed tail does not prevent queen waiting and then escaping")
    check(any(did > 2 and r == 0 for did, r, _ in deaths),
          "engine accepts wait split even when its boxed child dies immediately")
    old, deaths = engine_case(baseline)
    check(old[0]['action'] == 'SPLIT 6' and any(did == 0 for did, _, _ in deaths),
          "baseline loses queen in the same delayed-release fixture")


def main():
    check(importlib.metadata.version('unswbc') == '1.2.9', 'tested toolkit version 1.2.9')
    os.environ.setdefault('XDG_CACHE_HOME', str(Path(tempfile.gettempdir()) / 'unswbc-v251-cache'))
    os.environ.setdefault('UNSWBC_WARM', '1')
    from unswbc.run import _resolve
    from unswbc.sandbox import WasmPool
    pools = []
    with tempfile.TemporaryDirectory(prefix='v251-tests-') as tmp:
        try:
            for name in ('bot-v25.1.0', 'bot-v25.0.0-omar'):
                source = ROOT / 'bots/New/current' / name
                files = sorted(p for p in source.iterdir() if p.is_file())
                digest = hashlib.sha256(b''.join(p.name.encode()+p.read_bytes() for p in files)).hexdigest()
                target = Path(tmp) / f'{name}-{digest[:16]}'
                target.mkdir()
                for p in files:
                    shutil.copyfile(p, target / p.name)
                argv, directory, kind = _resolve(str(target), True)
                check(kind == 'wasm', f'{name} builds for sandbox')
                pools.append(WasmPool(argv, cwd=str(directory), key='v251-tests'))
            protocol_tests(*pools)
            engine_tests(*pools)
        finally:
            for pool in pools:
                pool.close()
    print(f'{CHECKS} checks passed (sandbox protocol and actual game engine).')


if __name__ == '__main__':
    main()
