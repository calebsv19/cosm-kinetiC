import argparse,json
from pathlib import Path
from coupling.reconstructed_flow_report import transfer_report,timing_report
p=argparse.ArgumentParser();p.add_argument('root');p.add_argument('--transfer-only',action='store_true');a=p.parse_args()
r=Path(a.root);v=transfer_report(r);(r/'transfer-verdict.json').write_text(json.dumps(v,indent=2)+'\n')
if not a.transfer_only:(r/'timing-verdict.json').write_text(json.dumps(timing_report(r),indent=2)+'\n')
print('strict reports written')
