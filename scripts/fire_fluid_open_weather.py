"""Finite local live Fire–Fluid open-weather comparison; no background retries."""
import argparse,hashlib,json,os,sys,time,traceback
from pathlib import Path
from passive_atmosphere import ROOT
from surface_sources.growth_fire_v1 import require
from coupling.open_weather_study import CASES,run_case
from coupling.live_gust import write


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('workers','qualification','output','budget-root'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();workspace=ROOT.parent.parent if ROOT.parent.name=='_worktrees' else ROOT.parent
    helper=workspace/'bin/verification_retention.py';require(helper.is_file(),'weather campaign needs consumer helper before native admission');sys.path.insert(0,str(helper.parent))
    qualified=json.loads(a.qualification.read_text());require(qualified['status']=='passed','qualified native weather worker required')
    selected=qualified['binaries']['worker'];worker=Path(selected['path'])
    require(hashlib.sha256(worker.read_bytes()).hexdigest()==selected['sha256'],'weather worker unchanged')
    w=json.loads(a.workers.read_text())
    workers=(w['growth_root'],w['binaries']['fire']['path'],str(worker),w['binaries']['clock']['path'])
    for role in ('fire','clock'):
        require(hashlib.sha256(Path(w['binaries'][role]['path']).read_bytes()).hexdigest()==w['binaries'][role]['sha256'],'retained '+role+' identity')
    sources=[ROOT/'scripts'/name for name in ('fire_fluid_open_weather.py','coupling/open_weather_study.py','coupling/open_weather_cases.py','coupling/open_weather_diagnostics.py','coupling/live_gust.py')]
    pins={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    a.output.mkdir(parents=True,exist_ok=False)
    write(a.output/'process.json',{'pid':os.getpid(),'script':str(Path(__file__).resolve()),'started':time.time(),'cases':CASES,'source_pins':pins,'worker':selected})
    def progress(name,windows,phase):
        write(a.output/'progress.json',{'status':'running','case':name,'completed_windows':windows,'windows':32,'phase':phase})
        if phase=='published':print(name,windows,flush=True)
    results={}
    for profile,grid in CASES:
        require(pins=={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'weather producer controls changed')
        name=profile+'-'+str(grid)
        try:results[name]=run_case(workers,a.output/'cases'/name,profile,grid,budget_root=a.budget_root,progress=progress)
        except Exception as error:
            failure={'status':'failed-preserved','error':str(error),'traceback':traceback.format_exc(),'action':'Inspect exact native attempt and process terminality before repair/retry; no root reuse or automatic retry'}
            write(a.output/'error.json',{'case':name,**failure});write(a.output/'cases'/name/'error.json',failure)
            results[name]=failure;write(a.output/'results.json',results);raise
        write(a.output/'results.json',results)
    write(a.output/'terminal.json',{'status':'numerical-complete-pending-audit','cases':results})
    write(a.output/'progress.json',{'status':'terminal-pending-audit','completed_cases':len(results)})

if __name__=='__main__':main()
