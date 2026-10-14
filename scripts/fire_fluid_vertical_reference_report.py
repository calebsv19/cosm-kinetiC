"""Analyze one completed independent directional reference."""
import argparse,json
from pathlib import Path
from coupling.vertical_reference_report import report
p=argparse.ArgumentParser();p.add_argument('root');p.add_argument('prior');a=p.parse_args()
r=Path(a.root);v=report(r,a.prior)
(r/'vertical-verdict.json').write_text(json.dumps(v,indent=2)+'\n')
print(json.dumps(v,indent=2))
