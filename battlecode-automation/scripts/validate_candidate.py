"""Deterministic five-map v5 gate. Fresh seeds, both sides, all discovered maps."""
import argparse,json,subprocess,sys
from pathlib import Path
from research_report import build

ap=argparse.ArgumentParser();ap.add_argument('candidate');ap.add_argument('--run',action='store_true');ap.add_argument('--check',action='store_true');args=ap.parse_args()
root=Path(__file__).resolve().parents[2]
if not args.check:
    cmd=[str(root/'.battlecode-codex/bin/python'),str(Path(__file__).with_name('research.py')),'--candidate',args.candidate,'--opponents','bot-v5','--seeds','303','404','505','606','--experiment',args.candidate+'-validation-v5']
    if not args.run:cmd.append('--dry-run')
    subprocess.run(cmd,cwd=root,check=True)
else:
    report=next((r for r in build()['bots'] if Path(r['candidate']).name==args.candidate),None)
    if report is None:raise SystemExit('No results for candidate')
    print(json.dumps({'candidate':args.candidate,'beaten_maps':report['v5_maps_beaten'],'passes_five_maps':report['passes_five_maps'],'note':'Passing this gate alone does not promote a champion.'},indent=2))
    raise SystemExit(0 if report['passes_five_maps'] else 1)
