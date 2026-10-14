import argparse,json
from pathlib import Path
from coupling.horizontal_reference_report import report
p=argparse.ArgumentParser();p.add_argument('root');a=p.parse_args();r=Path(a.root)
v=report(r);(r/'horizontal-verdict.json').write_text(json.dumps(v,indent=2)+'\n');print(json.dumps(v,indent=2))
