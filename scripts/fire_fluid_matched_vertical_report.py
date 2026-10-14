import argparse,json
from pathlib import Path
from coupling.matched_vertical_report import report,baseline_report
p=argparse.ArgumentParser();p.add_argument('root');p.add_argument('--baseline-only',action='store_true');a=p.parse_args()
r=Path(a.root);v=baseline_report(r) if a.baseline_only else report(r)
(r/('baseline-verdict.json' if a.baseline_only else 'matched-verdict.json')).write_text(json.dumps(v,indent=2)+'\n');print(json.dumps(v,indent=2))
