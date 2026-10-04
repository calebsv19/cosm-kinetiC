#!/usr/bin/env python3
"""Keep numerical execution success distinct from force-accuracy acceptance."""
import json
import re
import subprocess
from pathlib import Path
root=Path('build/s3-cfd-force-control-volume');root.mkdir(parents=True,exist_ok=True)
run=subprocess.run(['build/cfd_mac2d_force_accuracy_test'],capture_output=True,text=True)
(root/'run.log').write_text(run.stdout+run.stderr)
rows=[{k:float(v) for k,v in re.findall(r'(\w+)=([^ ]+)',line)} for line in run.stdout.splitlines() if line.startswith('n=')]
# An engineering control-volume consistency gate, not experimental validation.
report={'schema':'physics_sim_obstacle_force_cv_v1','execution_passed':run.returncode==0,'relative_mismatch_limit':.02,'runs':rows,'accuracy_passed':bool(run.returncode==0 and len(rows)==3 and rows[-1]['relative_mismatch']<.02),'physical_drag_qualified':False}
(root/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(0 if report['accuracy_passed'] else 1)
