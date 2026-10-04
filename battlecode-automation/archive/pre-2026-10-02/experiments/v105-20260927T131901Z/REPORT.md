Final selected-source results and limitations: [FINAL_REPORT.md](FINAL_REPORT.md).

# v10.5 experiments

Experiments may use different opponents, seeds and maps; compare matched schedules before claiming improvement.

| Group | W | D | L | Errors | Win rate | Score rate | Elimination W/D/L | Round-limit W/D/L | Mean longest | Max longest |
|---|---:|---:|---:|---:|---:|---:|---|---|---:|---:|
| extension-control40 | 16 | 2 | 3 | 0 | 76.2% | 81.0% | 14/2/3 | 2/0/0 | 4.5 | 28 |
| extension-opening15 | 13 | 2 | 6 | 0 | 61.9% | 66.7% | 13/2/6 | 0/0/0 | 2.2 | 4 |
| extension-opening25 | 12 | 2 | 7 | 0 | 57.1% | 61.9% | 11/2/7 | 1/0/0 | 3.0 | 25 |
| growers16 | 11 | 0 | 5 | 0 | 68.8% | 68.8% | 0/0/0 | 11/0/5 | 28.3 | 54 |
| growers32 | 11 | 0 | 5 | 0 | 68.8% | 68.8% | 0/0/0 | 11/0/5 | 27.9 | 63 |
| late-alpha450 | 11 | 0 | 5 | 0 | 68.8% | 68.8% | 0/0/0 | 11/0/5 | 29.8 | 57 |
| late-alpha475 | 9 | 0 | 7 | 0 | 56.2% | 56.2% | 0/0/0 | 9/0/7 | 29.5 | 62 |
| opening-parent | 8 | 2 | 11 | 0 | 38.1% | 42.9% | 8/2/10 | 0/0/1 | 2.6 | 24 |
| opening-point200 | 12 | 3 | 6 | 0 | 57.1% | 64.3% | 12/3/5 | 0/0/1 | 2.4 | 9 |
| opening-point300 | 14 | 2 | 5 | 0 | 66.7% | 71.4% | 13/2/5 | 1/0/0 | 4.4 | 31 |
| opening-region300 | 16 | 2 | 3 | 0 | 76.2% | 81.0% | 14/2/3 | 2/0/0 | 4.0 | 22 |
| opening-region300-r80 | 12 | 2 | 7 | 0 | 57.1% | 61.9% | 12/2/7 | 0/0/0 | 2.3 | 7 |
| validation-candidate | 74 | 6 | 30 | 0 | 67.3% | 70.0% | 44/6/18 | 30/0/12 | 12.9 | 56 |
| validation-head-to-head | 11 | 0 | 11 | 0 | 50.0% | 50.0% | 8/0/6 | 3/0/5 | 13.8 | 65 |
| validation-parent | 73 | 6 | 31 | 0 | 66.4% | 69.1% | 41/6/22 | 32/0/9 | 12.6 | 68 |
| validation-sandbox | 3 | 0 | 1 | 0 | 75.0% | 75.0% | 0/0/0 | 3/0/1 | 40.0 | 58 |

Each experiment directory contains its immutable manifest, source references, raw logs and append-only match ledger.
