#!/usr/bin/env python3
"""Fixed reference correction sequence, own-child resource bounds and immutable outputs."""
import hashlib,json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-obstacle/correction-v1'

def run(stem,args):
 output=DATA/(stem+'.json')
 receipt_path=DATA/(stem+'-receipt.json')
 if receipt_path.exists() and json.loads(receipt_path.read_text())['returncode'] != 0:
  print(json.dumps({'retained_failure':str(receipt_path)}),flush=True);return
 if output.exists():
  print(json.dumps({'retained':str(output)}),flush=True);return
 command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(ROOT/'scripts/cfd_fem_reference3d_scalar.py'),*args,'--output',str(output)]
 start=time.monotonic();peak=0;reason=None
 with (DATA/(stem+'.log')).open('w') as log:
  child=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
  try:
   while child.poll() is None:
    row=subprocess.run(['/bin/ps','-o','rss=','-p',str(child.pid)],capture_output=True,text=True).stdout.strip()
    rss=int(row or '0')*1024;peak=max(peak,rss)
    if rss>1800*1024*1024 or time.monotonic()-start>180:
     reason='reference RSS/time cap';child.terminate();break
    time.sleep(.2)
  finally:
   if child.poll() is None:
    try:child.wait(timeout=5)
    except subprocess.TimeoutExpired:child.kill();child.wait()
 receipt=dict(command=command,returncode=child.returncode,peak_observed_rss_bytes=peak,wall_s=time.monotonic()-start,stop_reason=reason,
              rss_cap_bytes=1800*1024*1024,wall_cap_s=180,mesh_cap=50000,
              source_sha256=hashlib.sha256((ROOT/'scripts/cfd_fem_reference3d_scalar.py').read_bytes()).hexdigest())
 (DATA/(stem+'-receipt.json')).write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt),flush=True)
 assert child.returncode==0,(stem,receipt)

if __name__=='__main__':
 DATA.mkdir(parents=True,exist_ok=True)
 run('reference-same-mesh',['--length','8','--center','4','--levels','10'])
 a=json.loads((DATA/'reference-same-mesh.json').read_text());b=json.loads((ROOT/'build/c3d-obstacle/reference8-L8-cx4-edge-l10.json').read_text())
 errors={}
 for key in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w'):
  aa,bb=a[key],b[key]
  if isinstance(aa,list):aa,bb=aa[0],bb[0]
  errors[key]=abs(aa/bb-1)
 assert max(errors.values())<1e-7,errors
 (DATA/'reference-equivalence.json').write_text(json.dumps({'relative_errors':errors,'passed':True,'old_peak_rss_bytes':b['peak_rss_bytes'],'new_peak_rss_bytes':a['peak_rss_bytes']},indent=2)+'\n')
 run('reference-empty-graded10-tight',['--length','8','--center','4','--graded','10','--gaps','4','--long','6','--empty'])
 run('reference-graded12',['--length','8','--center','4','--graded','12','--gaps','4','--long','6'])

 for count in (8,10,12):
  run(f'reference-graded{count}-gd01',['--length','8','--center','4','--graded',str(count),'--gaps','4','--long','6','--grad-div','.1'])
 run('reference-graded12-gd1',['--length','8','--center','4','--graded','12','--gaps','4','--long','6','--grad-div','1'])

 for count in (8,10,12):
  run(f'reference-graded{count}-mirror',['--length','8','--center','4','--graded',str(count),'--gaps','4','--long','6','--mirror-mesh'])
