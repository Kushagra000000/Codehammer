#!/usr/bin/env python3
"""UNSW Battlecode local ladder / tournament runner.

Discovers every bot project under bots/**/bot.toml, every map under maps/**/*.map,
runs matches in isolated temporary workspaces, stores compact match results, and
recomputes an Elo ladder plus a self-contained HTML dashboard.

Typical use from repository root:
    python3 scripts/autorun_matches.py --mode bootstrap --workers 2
    python3 scripts/autorun_matches.py --mode new --workers 2
    python3 scripts/autorun_matches.py --mode all --workers 2
    python3 scripts/autorun_matches.py --mode new --sandbox --workers 2
    python3 scripts/autorun_matches.py --list

The first run should normally be bootstrap or all, because Elo needs some
historical matches before there is a meaningful champion.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable

PROJECT_ROOT = Path(__file__).resolve().parents[2]
AUTOMATION_ROOT = Path(__file__).resolve().parents[1]

# The automation package lives at <repo>/battlecode-automation,
# while bots/maps/replays live at the repository root.
ROOT = PROJECT_ROOT
BOTS_ROOT = ROOT / "bots"
MAPS_ROOT = ROOT / "maps"
DATA_ROOT = AUTOMATION_ROOT / "data"
REPLAYS_ROOT = ROOT / "replays"
REPORTS_ROOT = AUTOMATION_ROOT / "reports"
CONFIG_PATH = AUTOMATION_ROOT / "scripts" / "automation_config.json"
MATCHES_PATH = DATA_ROOT / "matches.jsonl"
BOTS_PATH = DATA_ROOT / "bots.json"
RATINGS_PATH = DATA_ROOT / "ratings.json"
HISTORY_PATH = DATA_ROOT / "elo_history.jsonl"
CHAMPION_PATH = DATA_ROOT / "champion.json"

VERSION_RE = re.compile(r"(?:bot[-_]?v|v)(\d+(?:\.\d+)*)(?:-exp\d+)?$", re.IGNORECASE)


@dataclass(frozen=True)
class Bot:
    label: str
    path: str
    uid: str
    fingerprint: str
    version: tuple[int, ...]


@dataclass(frozen=True)
class MapInfo:
    name: str
    path: str


@dataclass
class MatchResult:
    match_id: str
    started_at: str
    completed_at: str
    duration_seconds: float
    map_name: str
    bot_a: str
    bot_b: str
    bot_a_uid: str
    bot_b_uid: str
    winner_uid: str | None
    winner_label: str | None
    result: str
    exit_code: int
    sandbox: bool
    side_order: str
    replay: str | None
    stdout_tail: str
    stderr_tail: str


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def load_json(path: Path, default):
    if not path.exists():
        return default
    try:
        return json.loads(path.read_text())
    except (OSError, json.JSONDecodeError):
        return default


def append_jsonl(path: Path, rows: Iterable[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False, separators=(",", ":")) + "\n")


def read_jsonl(path: Path) -> list[dict]:
    if not path.exists():
        return []
    rows: list[dict] = []
    with path.open(encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    return rows


def load_config() -> dict:
    defaults = {
        "initial_elo": 1000,
        "k_factor": 24,
        "workers": 2,
        "sandbox": False,
        "both_sides": True,
        "games_per_order": 1,
        "opponents_per_new": 5,
        "champion_min_games": 8,
        "bootstrap_opponents": 4,
        "keep_replays": True,
        "command": "unswbc",
        "bot_glob": "**/bot.toml",
        "map_glob": "**/*.map",
    }
    loaded = load_json(CONFIG_PATH, {})
    if isinstance(loaded, dict):
        defaults.update(loaded)
    local_cli = ROOT / ".battlecode-codex/bin/unswbc"
    if local_cli.exists():
        defaults["command"] = str(local_cli)
    return defaults


def parse_version(label: str) -> tuple[int, ...]:
    m = VERSION_RE.search(label)
    if not m:
        return (0,)
    return tuple(int(x) for x in m.group(1).split("."))


def fingerprint_dir(path: Path) -> str:
    h = hashlib.sha256()
    excluded_names = {".DS_Store"}
    excluded_suffixes = {".o", ".a", ".wasm", ".bak", ".tmp"}
    files = []
    for p in path.rglob("*"):
        if not p.is_file():
            continue
        if any(part in {".unswbc-build", "__pycache__", ".git"} for part in p.relative_to(path).parts):
            continue
        if p.name in excluded_names or p.suffix in excluded_suffixes:
            continue
        files.append(p)
    for p in sorted(files):
        rel = p.relative_to(path).as_posix().encode()
        h.update(rel + b"\0")
        try:
            h.update(p.read_bytes())
        except OSError:
            continue
        h.update(b"\0")
    return h.hexdigest()


def discover_bots() -> list[Bot]:
    if not BOTS_ROOT.exists():
        return []
    by_key: dict[tuple[str, str], Bot] = {}
    for toml in sorted(BOTS_ROOT.glob("**/bot.toml")):
        project = toml.parent
        if project.name.startswith("."):
            continue
        label = project.name
        fingerprint = fingerprint_dir(project)
        uid = f"{label}@{fingerprint[:10]}"
        bot = Bot(label, str(project.relative_to(ROOT)), uid, fingerprint, parse_version(label))
        # Identical label+fingerprint is one model; prefer the shorter/current path.
        by_key[(label, fingerprint)] = bot
    return sorted(by_key.values(), key=lambda b: (b.version, b.label.lower(), b.path))


def discover_maps() -> list[MapInfo]:
    if not MAPS_ROOT.exists():
        return []
    paths = sorted(p for p in MAPS_ROOT.glob("**/*.map") if p.is_file())
    return [MapInfo(p.name, str(p.relative_to(ROOT))) for p in paths]


def bot_by_name(bots: list[Bot], value: str) -> Bot | None:
    value_path = Path(value)
    matches = [b for b in bots if b.label == value or b.path == value or str(ROOT / b.path) == value_path.as_posix()]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        exact = [b for b in matches if b.path == value]
        return exact[0] if exact else None
    return None


def existing_match_keys(rows: list[dict]) -> set[tuple]:
    keys = set()
    for r in rows:
        if r.get("result") not in {"win", "draw"}:
            continue
        a, b = r.get("bot_a_uid"), r.get("bot_b_uid")
        if not a or not b:
            continue
        keys.add((r.get("map_name"), a, b, r.get("side_order", "")))
    return keys


def short_tail(value: str, limit: int = 5000) -> str:
    if len(value) <= limit:
        return value
    return value[-limit:]


def find_replays(workdir: Path) -> list[Path]:
    replay_root = workdir / "replays"
    if not replay_root.exists():
        return []
    return sorted(replay_root.glob("*.replay"), key=lambda p: p.stat().st_mtime)


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]+", "_", value)


def run_one_match(task: dict) -> MatchResult:
    match_id = task["match_id"]
    map_info: MapInfo = MapInfo(**task["map"])
    bot_a: Bot = Bot(**task["bot_a"])
    bot_b: Bot = Bot(**task["bot_b"])
    sandbox = bool(task["sandbox"])
    keep_replays = bool(task["keep_replays"])
    command_name = task["command"]
    if "/" in command_name and not Path(command_name).is_absolute():
        command_name = str(ROOT / command_name)
    started_at = utc_now()
    started = time.monotonic()
    stdout = ""
    stderr = ""
    exit_code = -999
    replay_target: str | None = None

    with tempfile.TemporaryDirectory(prefix=f"bc-{match_id}-") as td:
        workdir = Path(td)
        (workdir / "maps").mkdir()
        (workdir / "replays").mkdir()
        # Full copies avoid symlink/path surprises and guarantee independent build/replay dirs.
        # Header edits must invalidate unswbc 1.1.0's source-only wasm cache.
        alias_a = "bot_a-" + bot_a.fingerprint[:16]
        alias_b = "bot_b-" + bot_b.fingerprint[:16]
        a_dst = workdir / alias_a
        b_dst = workdir / alias_b
        shutil.copytree(ROOT / bot_a.path, a_dst)
        shutil.copytree(ROOT / bot_b.path, b_dst)
        shutil.copy2(ROOT / map_info.path, workdir / "maps" / map_info.name)

        cmd = [command_name, "run", f"maps/{map_info.name}", alias_a, alias_b]
        if sandbox:
            cmd.append("--sandbox")
        env = os.environ.copy()
        env.setdefault("NO_COLOR", "1")
        env["UNSWBC_NO_UPDATE"] = "1"
        env["UNSWBC_NO_VSCODE"] = "1"

        try:
            proc = subprocess.run(
                cmd,
                cwd=workdir,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            exit_code = proc.returncode
            stdout = proc.stdout or ""
            stderr = proc.stderr or ""
        except FileNotFoundError as exc:
            stderr = str(exc)
            exit_code = 127
        except Exception as exc:  # pragma: no cover
            stderr = f"{type(exc).__name__}: {exc}"
            exit_code = 1

        replay_files = find_replays(workdir)
        if replay_files and keep_replays:
            replay_src = replay_files[-1]
            REPLAYS_ROOT.mkdir(parents=True, exist_ok=True)
            replay_name = (
                f"{safe_name(bot_a.label)}-vs-{safe_name(bot_b.label)}-on-"
                f"{safe_name(map_info.name)}-{match_id}.replay"
            )
            target = REPLAYS_ROOT / replay_name
            try:
                shutil.move(str(replay_src), str(target))
                replay_target = str(target.relative_to(ROOT))
            except OSError as exc:
                stderr += f"\nFailed to move replay: {exc}"
        elif replay_files and not keep_replays:
            replay_target = None

    raw_dir = DATA_ROOT / "raw_logs" / match_id
    raw_dir.mkdir(parents=True, exist_ok=True)
    (raw_dir / "stdout.txt").write_text(stdout)
    (raw_dir / "stderr.txt").write_text(stderr)
    winner_alias = parse_winner(stdout, stderr, {"bot_a": bot_a, "bot_b": bot_b}) if exit_code == 0 else None
    if winner_alias == "bot_a":
        winner_uid, winner_label, result = bot_a.uid, bot_a.label, "win"
    elif winner_alias == "bot_b":
        winner_uid, winner_label, result = bot_b.uid, bot_b.label, "win"
    elif winner_alias == "draw":
        winner_uid, winner_label, result = None, None, "draw"
    elif exit_code == 0:
        winner_uid, winner_label, result = None, None, "unparsed"
    else:
        winner_uid, winner_label, result = None, None, "error"

    return MatchResult(
        match_id=match_id,
        started_at=started_at,
        completed_at=utc_now(),
        duration_seconds=round(time.monotonic() - started, 3),
        map_name=map_info.name,
        bot_a=bot_a.label,
        bot_b=bot_b.label,
        bot_a_uid=bot_a.uid,
        bot_b_uid=bot_b.uid,
        winner_uid=winner_uid,
        winner_label=winner_label,
        result=result,
        exit_code=exit_code,
        sandbox=sandbox,
        side_order=f"{bot_a.label}>{bot_b.label}",
        replay=replay_target,
        stdout_tail=short_tail(stdout),
        stderr_tail=short_tail(stderr),
    )


def parse_winner(stdout: str, stderr: str, aliases: dict[str, Bot]) -> str | None:
    # Only authoritative result sentences; unknown output remains unparsed.
    outcomes = re.findall(r"^team ([AB]) wins after \d+ rounds(?: .*|)$", stdout, re.M)
    draws = re.findall(r"^(?:draw after \d+ rounds.*|game ended in a draw after \d+ rounds.*)$", stdout, re.M)
    if len(outcomes) + len(draws) != 1:
        return None
    if draws:
        return "draw"
    return "bot_a" if outcomes[0] == "A" else "bot_b"


def generate_match_id() -> str:
    # Millisecond UTC timestamp + small random suffix without needing UUID import.
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")[:-3]
    suffix = hashlib.sha1(os.urandom(16)).hexdigest()[:6]
    return f"{stamp}-{suffix}"


def schedule_pair(a: Bot, b: Bot, maps: list[MapInfo], config: dict, existing: set[tuple], force: bool = False) -> list[dict]:
    if a.uid == b.uid:
        return []
    tasks = []
    orders = [(a, b)]
    if config.get("both_sides", True) and a.uid != b.uid:
        orders.append((b, a))
    repeats = max(1, int(config.get("games_per_order", 1)))
    for map_info in maps:
        for left, right in orders:
            key = (map_info.name, left.uid, right.uid, f"{left.label}>{right.label}")
            for _ in range(repeats):
                if not force and key in existing:
                    # Existing match key suppresses duplicate one-per-side runs.
                    continue
                tasks.append({
                    "match_id": generate_match_id(),
                    "map": asdict(map_info),
                    "bot_a": asdict(left),
                    "bot_b": asdict(right),
                    "sandbox": bool(config.get("sandbox", False)),
                    "keep_replays": bool(config.get("keep_replays", True)),
                    "command": config.get("command", "unswbc"),
                })
    return tasks


def ratings_from_matches(rows: list[dict], config: dict) -> tuple[dict, list[dict], dict]:
    initial = float(config.get("initial_elo", 1000))
    k = float(config.get("k_factor", 24))
    ratings: dict[str, float] = {}
    stats: dict[str, dict] = {}
    history: list[dict] = []

    def ensure(uid: str, label: str | None = None):
        if uid not in ratings:
            ratings[uid] = initial
        if uid not in stats:
            stats[uid] = {"label": label or uid, "games": 0, "wins": 0, "losses": 0, "draws": 0}

    ordered = sorted(
        [r for r in rows if r.get("result") in {"win", "draw"}],
        key=lambda r: (r.get("completed_at", ""), r.get("match_id", "")),
    )
    for r in ordered:
        a, b = r.get("bot_a_uid"), r.get("bot_b_uid")
        if not a or not b or a == b:
            continue
        ensure(a, r.get("bot_a"))
        ensure(b, r.get("bot_b"))
        ra, rb = ratings[a], ratings[b]
        expected_a = 1.0 / (1.0 + 10 ** ((rb - ra) / 400.0))
        expected_b = 1.0 - expected_a
        if r["result"] == "draw":
            sa, sb = 0.5, 0.5
            stats[a]["draws"] += 1
            stats[b]["draws"] += 1
        elif r.get("winner_uid") == a:
            sa, sb = 1.0, 0.0
            stats[a]["wins"] += 1
            stats[b]["losses"] += 1
        elif r.get("winner_uid") == b:
            sa, sb = 0.0, 1.0
            stats[a]["losses"] += 1
            stats[b]["wins"] += 1
        else:
            continue
        ratings[a] = ra + k * (sa - expected_a)
        ratings[b] = rb + k * (sb - expected_b)
        stats[a]["games"] += 1
        stats[b]["games"] += 1
        history.append({
            "match_id": r.get("match_id"),
            "completed_at": r.get("completed_at"),
            "a": a,
            "b": b,
            "a_rating": round(ratings[a], 2),
            "b_rating": round(ratings[b], 2),
            "map": r.get("map_name"),
        })

    for uid in ratings:
        games = stats[uid]["games"]
        stats[uid]["win_rate"] = round(((stats[uid]["wins"] + 0.5 * stats[uid]["draws"]) / games) * 100, 2) if games else 0.0
        stats[uid]["rating"] = round(ratings[uid], 2)
    return ratings, history, stats


def choose_champion(bots: list[Bot], ratings: dict[str, float], stats: dict[str, dict], config: dict) -> Bot | None:
    # Elo ranking is not a promotion certificate. Retain the explicitly recorded
    # incumbent; research promotion must satisfy the preregistered v5/map gates.
    incumbent = load_json(CHAMPION_PATH, {}).get("champion_uid")
    if incumbent:
        return next((b for b in bots if b.uid == incumbent), None)
    return None


def highest_rated(bots: list[Bot], ratings: dict[str, float], stats: dict[str, dict], config: dict) -> Bot | None:
    ranked = []
    min_games = int(config.get("champion_min_games", 8))
    bot_map = {b.uid: b for b in bots}
    for uid, r in ratings.items():
        b = bot_map.get(uid)
        if not b:
            continue
        ranked.append((stats.get(uid, {}).get("games", 0), r, b.version, b.label.lower(), b))
    qualified = [x for x in ranked if x[0] >= min_games]
    pool = qualified or ranked
    if not pool:
        return None
    return max(pool, key=lambda x: (x[1], x[0], x[2], x[3]))[-1]


def update_state_and_dashboard(bots: list[Bot], rows: list[dict], config: dict) -> None:
    ratings, history, stats = ratings_from_matches(rows, config)
    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    report = []
    for b in bots:
        s = stats.get(b.uid, {"games": 0, "wins": 0, "losses": 0, "draws": 0, "win_rate": 0.0})
        report.append({
            "uid": b.uid,
            "label": b.label,
            "path": b.path,
            "version": list(b.version),
            "fingerprint": b.fingerprint,
            "rating": round(ratings.get(b.uid, config.get("initial_elo", 1000)), 2),
            **s,
        })
    report.sort(key=lambda x: (x["rating"], x["games"], x["version"]), reverse=True)
    RATINGS_PATH.write_text(json.dumps({"generated_at": utc_now(), "bots": report}, indent=2) + "\n")

    with HISTORY_PATH.open("w", encoding="utf-8") as f:
        for item in history:
            f.write(json.dumps(item, separators=(",", ":")) + "\n")

    champion = choose_champion(bots, ratings, stats, config)
    champion_payload = {
        "generated_at": utc_now(),
        "champion_uid": champion.uid if champion else None,
        "champion_label": champion.label if champion else None,
        "rating": round(ratings.get(champion.uid, config.get("initial_elo", 1000)), 2) if champion else None,
        "games": stats.get(champion.uid, {}).get("games", 0) if champion else 0,
        "selection": "highest Elo among bots meeting champion_min_games; otherwise highest Elo among rated bots",
    }
    # Preserve the historical incumbent record; dashboard selection is read-only.
    champion_payload = load_json(CHAMPION_PATH, champion_payload)

    BOTS_PATH.write_text(json.dumps([asdict(b) for b in bots], indent=2) + "\n")

    generate_dashboard(report, rows, history, champion_payload)
    print_rankings(report, champion_payload)


def generate_dashboard(report: list[dict], rows: list[dict], history: list[dict], champion: dict) -> None:
    REPORTS_ROOT.mkdir(parents=True, exist_ok=True)
    dashboard_path = REPORTS_ROOT / "ladder.html"
    payload = {
        "generated_at": utc_now(),
        "bots": report,
        "matches": rows,
        "history": history,
        "champion": champion,
    }
    raw = json.dumps(payload, ensure_ascii=False).replace("</script>", "<\\/script>")
    html = r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>UNSW Battlecode — Local Elo Ladder</title>
<style>
:root{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:#16202a;background:#f5f7fa}
body{margin:0}.wrap{max-width:1400px;margin:0 auto;padding:28px}
h1{margin:0 0 6px;font-size:30px}.sub{color:#5b6875;margin-bottom:24px}
.cards{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:14px;margin-bottom:18px}
.card{background:white;border:1px solid #dfe4ea;border-radius:14px;padding:16px;box-shadow:0 2px 8px rgba(0,0,0,.04)}
.label{font-size:12px;text-transform:uppercase;letter-spacing:.07em;color:#6b7785}.value{font-size:27px;font-weight:700;margin-top:6px}
section{background:white;border:1px solid #dfe4ea;border-radius:14px;padding:18px;margin:18px 0;box-shadow:0 2px 8px rgba(0,0,0,.04)}
h2{margin:0 0 14px;font-size:19px}.row{display:grid;grid-template-columns:1fr 1fr;gap:18px}
.chart{width:100%;overflow:auto}.axis{stroke:#ccd3da;stroke-width:1}.line{fill:none;stroke-width:3}.bar{fill:#64748b}.bar.champion{fill:#0f766e}.gridline{stroke:#e8ecf0;stroke-width:1}
table{width:100%;border-collapse:collapse;font-size:14px}th,td{padding:10px 8px;border-bottom:1px solid #edf0f3;text-align:left}th{color:#65717d;font-weight:600}td.num{text-align:right;font-variant-numeric:tabular-nums}.rank{font-weight:700}.champ{font-weight:700}.muted{color:#6d7884}.pill{display:inline-block;border-radius:999px;padding:3px 8px;background:#eef2f6;font-size:12px}
@media(max-width:900px){.cards{grid-template-columns:repeat(2,1fr)}.row{grid-template-columns:1fr}}
@media(max-width:560px){.cards{grid-template-columns:1fr}}
</style>
</head>
<body><div class="wrap">
<h1>UNSW Battlecode Local Elo Ladder</h1><div class="sub" id="sub"></div>
<div class="cards" id="cards"></div>
<section><h2>Current rankings</h2><table><thead><tr><th>#</th><th>Bot</th><th>Elo</th><th>Games</th><th>W-L-D</th><th>Win%</th><th>Path</th></tr></thead><tbody id="table"></tbody></table></section>
<div class="row"><section><h2>Elo ratings</h2><div class="chart" id="eloChart"></div></section><section><h2>Elo history</h2><div class="chart" id="historyChart"></div></section></div>
<section><h2>Map performance</h2><div class="chart" id="mapTable"></div></section>
<section><h2>Head-to-head</h2><div class="chart" id="h2h"></div></section>
</div>
<script>
const DATA = __DATA__;
const esc=s=>String(s??'').replace(/[&<>"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));
const bots=[...DATA.bots].sort((a,b)=>b.rating-a.rating||b.games-a.games);
const matchRows=DATA.matches.filter(m=>m.result==='win'||m.result==='draw');
const labels={};bots.forEach(b=>labels[b.uid]=b.label);
document.getElementById('sub').textContent=`Generated ${new Date(DATA.generated_at).toLocaleString()} · ${matchRows.length} rated matches · Champion: ${DATA.champion.champion_label||'none'} (${DATA.champion.rating??'—'})`;
document.getElementById('cards').innerHTML=[
  ['Champion',DATA.champion.champion_label||'—'],['Champion Elo',DATA.champion.rating??'—'],['Bots',bots.length],['Rated matches',matchRows.length]
].map(x=>`<div class="card"><div class="label">${esc(x[0])}</div><div class="value">${esc(x[1])}</div></div>`).join('');
document.getElementById('table').innerHTML=bots.map((b,i)=>`<tr><td class="rank">${i+1}</td><td class="${b.uid===DATA.champion.champion_uid?'champ':''}">${esc(b.label)} ${b.uid===DATA.champion.champion_uid?'<span class="pill">champion</span>':''}</td><td class="num">${b.rating.toFixed(1)}</td><td class="num">${b.games}</td><td class="num">${b.wins}-${b.losses}-${b.draws}</td><td class="num">${b.win_rate.toFixed(1)}%</td><td class="muted">${esc(b.path)}</td></tr>`).join('');
function svgEl(tag,attrs,children=''){return `<${tag} ${Object.entries(attrs).map(([k,v])=>`${k}="${String(v).replace(/"/g,'&quot;')}"`).join(' ')}>${children}</${tag}>`}
function ratingBars(){const w=760,h=Math.max(250,bots.length*38+70),left=180,right=50,top=24,bottom=35;const max=Math.max(...bots.map(b=>b.rating),1000),min=Math.min(...bots.map(b=>b.rating),900);const scale=x=>(x-min)/(max-min||1)*(w-left-right);let s=`<svg viewBox="0 0 ${w} ${h}" role="img">`;bots.forEach((b,i)=>{const y=top+i*38,bar=scale(b.rating);s+=svgEl('rect',{x:left,y:y+4,width:bar,height:24,rx:6,class:b.uid===DATA.champion.champion_uid?'bar champion':'bar'})+svgEl('text',{x:left-10,y:y+21,'text-anchor':'end','font-size':13,fill:'#34404c'},esc(b.label))+svgEl('text',{x:left+bar+8,y:y+21,'font-size':13,fill:'#34404c'},b.rating.toFixed(1))});s+=svgEl('line',{x1:left,y1:h-bottom,x2:w-right,y2:h-bottom,class:'axis'});s+='</svg>';return s}
document.getElementById('eloChart').innerHTML=ratingBars();
function historyChart(){const topN=bots.slice(0,8);const ids=new Set(topN.map(b=>b.uid));const points={};topN.forEach(b=>points[b.uid]=[{i:0,r:1000}]);let i=0;for(const h of DATA.history){i++;if(ids.has(h.a))points[h.a].push({i,r:h.a_rating});if(ids.has(h.b))points[h.b].push({i,r:h.b_rating});}const w=760,h=300,l=55,r=20,t=20,b=35;const vals=Object.values(points).flat().map(x=>x.r);const min=Math.floor(Math.min(...vals,900)/50)*50,max=Math.ceil(Math.max(...vals,1100)/50)*50;const x=i2=>l+(i2/Math.max(i,1))*(w-l-r),y=v=>t+(max-v)/(max-min||1)*(h-t-b);let s=`<svg viewBox="0 0 ${w} ${h}">`;for(let g=min;g<=max;g+=50){const yy=y(g);s+=svgEl('line',{x1:l,y1:yy,x2:w-r,y2:yy,class:'gridline'})+svgEl('text',{x:l-8,y:yy+4,'text-anchor':'end','font-size':11,fill:'#6b7785'},g)}Object.entries(points).forEach(([uid,arr])=>{if(arr.length<2)return;const d=arr.map((p,j)=>(j?'L':'M')+x(p.i)+','+y(p.r)).join(' ');s+=svgEl('path',{d,class:'line',stroke:'#64748b'})+svgEl('text',{x:w-r,y:y(arr[arr.length-1].r)+4,'font-size':11,fill:'#334155'},esc(labels[uid]||uid));});s+='</svg>';return s}
document.getElementById('historyChart').innerHTML=historyChart();
function mapStats(){const by={};for(const m of matchRows){by[m.map_name]??={};for(const u of [m.bot_a_uid,m.bot_b_uid])by[m.map_name][u]??={g:0,w:0,d:0};by[m.map_name][m.bot_a_uid].g++;by[m.map_name][m.bot_b_uid].g++;if(m.result==='draw'){by[m.map_name][m.bot_a_uid].d++;by[m.map_name][m.bot_b_uid].d++;}else{by[m.map_name][m.winner_uid].w++;}}let html='<table><thead><tr><th>Map</th><th>Bot</th><th>Win%</th><th>Games</th></tr></thead><tbody>';for(const map of Object.keys(by).sort()){const rows=Object.entries(by[map]).map(([uid,s])=>({uid,...s})).sort((a,b)=>(b.w/b.g)-(a.w/a.g));for(const x of rows.slice(0,10)){html+=`<tr><td>${esc(map)}</td><td>${esc(labels[x.uid]||x.uid)}</td><td class="num">${(100*(x.w+.5*x.d)/x.g).toFixed(1)}%</td><td class="num">${x.g}</td></tr>`;}}html+='</tbody></table>';return html}document.getElementById('mapTable').innerHTML=mapStats();
function h2h(){const d={};for(const m of matchRows){const key=[m.bot_a_uid,m.bot_b_uid].sort().join('|');d[key]??={a:m.bot_a_uid,b:m.bot_b_uid,ag:0,bg:0,draw:0};const z=d[key];z.ag++;z.bg++;if(m.result==='draw')z.draw++;else if(m.winner_uid===z.a)z.a_w=(z.a_w||0)+1;else z.b_w=(z.b_w||0)+1;}let html='<table><thead><tr><th>Bot A</th><th>Bot B</th><th>A wins</th><th>B wins</th><th>Draws</th></tr></thead><tbody>';Object.values(d).sort((x,y)=>(y.a_w||0)+(y.b_w||0)-(x.a_w||0)-(x.b_w||0)).forEach(z=>html+=`<tr><td>${esc(labels[z.a]||z.a)}</td><td>${esc(labels[z.b]||z.b)}</td><td class="num">${z.a_w||0}</td><td class="num">${z.b_w||0}</td><td class="num">${z.draw}</td></tr>`);html+='</tbody></table>';return html}document.getElementById('h2h').innerHTML=h2h();
</script></body></html>'''.replace('__DATA__', raw)
    dashboard_path.write_text(html, encoding="utf-8")


def print_rankings(report: list[dict], champion: dict) -> None:
    print("\n=== UNSW Battlecode Ladder ===")
    for i, b in enumerate(report[:15], 1):
        marker = " <- CHAMPION" if b["uid"] == champion.get("champion_uid") else ""
        print(f"{i:>2}. {b['label']:<16} Elo {b['rating']:>7.1f}  {b['wins']}-{b['losses']}-{b['draws']}  ({b['games']} games){marker}")
    print(f"\nDashboard: {REPORTS_ROOT / 'ladder.html'}")


def choose_new_candidates(bots: list[Bot], known_uids: set[str]) -> list[Bot]:
    return [b for b in bots if b.uid not in known_uids]


def top_opponents(bots: list[Bot], rows: list[dict], config: dict, exclude: set[str]) -> list[Bot]:
    ratings, _, stats = ratings_from_matches(rows, config)
    bot_map = {b.uid: b for b in bots}
    ranked = sorted(
        [b for b in bots if b.uid not in exclude],
        key=lambda b: (ratings.get(b.uid, config.get("initial_elo", 1000)), stats.get(b.uid, {}).get("games", 0), b.version),
        reverse=True,
    )
    n = max(1, int(config.get("opponents_per_new", 5)))
    return ranked[:n]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["new", "all", "bootstrap"], default="new")
    parser.add_argument("--workers", type=int, default=None, help="Parallel match processes (default from config)")
    parser.add_argument("--sandbox", action="store_true", help="Pass --sandbox to unswbc run")
    parser.add_argument("--no-sandbox", action="store_true")
    parser.add_argument("--maps", nargs="*", help="Only these map names, otherwise all maps under maps/")
    parser.add_argument("--bots", nargs="*", help="Only these bot folder names/paths; otherwise auto-discover")
    parser.add_argument("--opponents", type=int, default=None, help="Number of opponents per new bot")
    parser.add_argument("--games-per-order", type=int, default=None)
    parser.add_argument("--one-side", action="store_true", help="Do not run the reversed side order")
    parser.add_argument("--force", action="store_true", help="Ignore duplicate suppression and rerun requested matches")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--list", action="store_true", help="List discovered bots and maps")
    args = parser.parse_args()

    config = load_config()
    if args.workers is not None:
        config["workers"] = args.workers
    if args.sandbox:
        config["sandbox"] = True
    if args.no_sandbox:
        config["sandbox"] = False
    if args.opponents is not None:
        config["opponents_per_new"] = args.opponents
    if args.games_per_order is not None:
        config["games_per_order"] = args.games_per_order
    if args.one_side:
        config["both_sides"] = False

    bots = discover_bots()
    active = set(config.get("active_bot_names", []))
    if active:
        bots = [b for b in bots if b.label in active]
    maps = discover_maps()
    if args.maps:
        wanted = set(args.maps)
        maps = [m for m in maps if m.name in wanted or m.path in wanted]
    if args.bots:
        selected = []
        for value in args.bots:
            b = bot_by_name(bots, value)
            if b:
                selected.append(b)
            else:
                print(f"ERROR: bot not found or ambiguous: {value}", file=sys.stderr)
                return 2
        bots = selected

    if args.list:
        print("Bots:")
        for b in bots:
            print(f"  {b.label:16} v={'.'.join(map(str,b.version)):8} {b.uid}  {b.path}")
        print("\nMaps:")
        for m in maps:
            print(f"  {m.name:24} {m.path}")
        return 0

    if len(bots) < 2:
        print("Need at least 2 bot projects.", file=sys.stderr)
        return 2
    if not maps:
        print("No .map files found under maps/", file=sys.stderr)
        return 2

    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    REPLAYS_ROOT.mkdir(parents=True, exist_ok=True)
    REPORTS_ROOT.mkdir(parents=True, exist_ok=True)
    rows = read_jsonl(MATCHES_PATH)
    existing = existing_match_keys(rows)

    tasks: list[dict] = []
    if args.mode == "all":
        for i, a in enumerate(bots):
            for b in bots[i + 1:]:
                tasks.extend(schedule_pair(a, b, maps, config, existing, args.force))
    elif args.mode == "bootstrap":
        # Efficient initial ladder: newest versions play a small opponent set.
        ordered = sorted(bots, key=lambda b: (b.version, b.label.lower()), reverse=True)
        n = max(1, int(config.get("bootstrap_opponents", 4)))
        for idx, candidate in enumerate(ordered):
            opponents = ordered[idx + 1:idx + 1 + n]
            for opponent in opponents:
                tasks.extend(schedule_pair(candidate, opponent, maps, config, existing, args.force))
    else:
        known_uids = {r.get("bot_a_uid") for r in rows} | {r.get("bot_b_uid") for r in rows}
        known_uids.discard(None)
        new_bots = choose_new_candidates(bots, known_uids)
        # Schedule gaps on newly added maps even when every bot is already known.
        known_maps = {r.get("map_name") for r in rows if r.get("result") in {"win", "draw"}}
        if any(m.name not in known_maps for m in maps):
            new_bots = bots
        ratings, _, stats = ratings_from_matches(rows, config)
        champion = choose_champion(bots, ratings, stats, config)
        if champion is None:
            # No rated history yet: choose the highest-version bot as a provisional baseline.
            champion = max(bots, key=lambda b: (b.version, b.label.lower()))
            print(f"No rated champion exists yet; using {champion.label} as provisional baseline.")
        print("New model candidates:", ", ".join(b.label for b in new_bots) if new_bots else "none")
        for candidate in new_bots:
            opponents = [champion]
            opponents.extend(top_opponents(bots, rows, config, {candidate.uid, champion.uid}))
            seen = {b.uid for b in opponents}
            for b in bots:
                if b.uid in seen or b.uid == candidate.uid:
                    continue
                # Baseline guardrail: include oldest version when available.
                if parse_version(b.label) in {(1,), (5,)}:
                    opponents.append(b)
                    seen.add(b.uid)
            for opponent in opponents:
                tasks.extend(schedule_pair(candidate, opponent, maps, config, existing, args.force))

    if args.dry_run:
        # New-map gap filling can reach a pairing from either candidate.
        unique = {}
        for task in tasks:
            key = (task["map"]["path"], task["bot_a"]["uid"], task["bot_b"]["uid"])
            unique.setdefault(key, task)
        if int(config.get("games_per_order", 1)) == 1:
            tasks = list(unique.values())
        print(f"Scheduled {len(tasks)} matches with workers={config['workers']}, sandbox={config['sandbox']}.")
        for t in tasks[:25]:
            print(f"  {t['bot_a']['label']} vs {t['bot_b']['label']} on {t['map']['name']}")
        if len(tasks) > 25:
            print(f"  ... {len(tasks)-25} more")
        return 0

    if not tasks:
        # Still refresh dashboard so newly discovered bots appear with initial Elo.
        update_state_and_dashboard(bots, rows, config)
        print("No new matches required.")
        return 0

    if int(config.get("games_per_order", 1)) == 1:
        unique = {}
        for task in tasks:
            key = (task["map"]["path"], task["bot_a"]["uid"], task["bot_b"]["uid"])
            unique.setdefault(key, task)
        tasks = list(unique.values())

    print(f"Running {len(tasks)} matches with {config['workers']} parallel workers on {len(maps)} maps.")
    print("Parallel runs use isolated temporary workspaces; replays are moved into ./replays/.")
    new_rows: list[dict] = []
    workers = max(1, int(config.get("workers", 2)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        futures = {ex.submit(run_one_match, t): t for t in tasks}
        completed = 0
        for fut in concurrent.futures.as_completed(futures):
            completed += 1
            res = fut.result()
            row = asdict(res)
            new_rows.append(row)
            if res.result == "win":
                outcome = f"winner={res.winner_label}"
            else:
                outcome = res.result
            print(f"[{completed:>4}/{len(tasks)}] {res.bot_a} vs {res.bot_b} | {res.map_name} | {outcome} | {res.duration_seconds:.1f}s")
            if res.result == "unparsed":
                print("       WARNING: match completed but winner could not be parsed; not added to Elo.")
            elif res.result == "error":
                print("       ERROR: unswbc failed. Inspect stored stderr_tail / unswbc log.")

    append_jsonl(MATCHES_PATH, new_rows)
    all_rows = rows + new_rows
    update_state_and_dashboard(bots, all_rows, config)
    print(f"\nStored results in {MATCHES_PATH}")
    print(f"Dashboard: open {REPORTS_ROOT / 'ladder.html'}")

    if any(r["result"] == "unparsed" for r in new_rows):
        print("\nIMPORTANT: Some successful matches had unparsed winners.")
        print("Run one battle manually and paste its output to the agent so the parser can be tightened to the exact 0.3.13 output format.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
