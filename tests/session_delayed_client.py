"""Local native-lifetime fixture; never connects to a simulation service."""
import argparse,json,os,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path);p.add_argument('--output',type=Path);p.add_argument('tool');p.add_argument('arguments');a=p.parse_args()
with (a.root/'calls').open('a') as f:f.write(json.dumps({'pid':os.getpid(),'output':str(a.output),'tool':a.tool})+'\n')
rows=(a.root/'calls').read_text().splitlines()
(a.root/'client.pid').write_text(str(os.getpid()))
time.sleep(1 if len(rows)==1 else 3)
a.output.write_text(json.dumps({'status':'late-result','scene_id':'retired-scene'}))
