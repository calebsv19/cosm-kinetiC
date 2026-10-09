#!/usr/bin/env python3
"""Qualify two installed CLI payloads, with no imports from source checkouts."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import signal
import time
import subprocess
import tempfile


def run(command, cwd, *, fails=False):
    env = {k:v for k,v in os.environ.items() if not k.startswith(('PYTHON','PHYSICS_SIM_'))}
    env['PYTHONPATH'] = str(cwd/'nonexistent-poison-module')
    value = subprocess.run([str(x) for x in command],cwd=cwd,env=env,text=True,
        stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
    if fails:
        if value.returncode == 0:raise AssertionError('Expected refusal: '+str(command))
        return value.stderr
    if value.returncode:raise AssertionError(value.stderr)
    return json.loads(value.stdout)


def qualify(growth, physics, output):
    output.mkdir(parents=True,exist_ok=False)
    g = growth/'bin/fire-source'; p = physics/'bin/physics_sim_coupling'
    gc = run([g,'capabilities'],output);pc = run([p,'capabilities'],output)
    assert gc['physical_response_qualified'] is False
    assert pc['physical_calibration_qualified'] is False
    initial = run([g,'init',growth/'examples/fire-config.json','--store',output/'source','--request-id','initial'],output)
    first = run([g,'advance',initial['path'],'--ticks',3,'--request-id','first'],output)
    second = run([g,'advance',first['path'],'--ticks',3,'--request-id','second'],output)
    bundles = [json.loads(Path(v['path']).read_text()) for v in (first,second)]
    policy = json.loads((physics/'examples/prescribed-policy.json').read_text())
    policy['producer'] = bundles[0]['frame']['producer'];policy['receiver']['dimensions'][2] = 4
    n = math.prod(policy['receiver']['dimensions'])
    c = {'policy':policy, 'properties':{'density_kg_m3':1,'dynamic_viscosity_pa_s':.02,
        'heat_capacity_j_kg_k':800000,'reference_temperature_k':300,
        'conductivity_w_m_k':.1,'tracer_diffusivity_m2_s':.001},
        'initial_face_velocity_m_s':[.2]*n+[.05]*n+[.1]*n,'momentum_dt_s':.01,
        'worker':str(physics/'bin/physics_sim_atmosphere_worker')}
    config=output/'config.json';config.write_text(json.dumps(c))
    def receive(store,*args,fails=False):
        return run([p,'--runtime',output/'runtime','periodic','--config',config,'--store',store,*args],output,fails=fails)
    store=output/'receiving';receive(store,'init')
    for b in (first,second):receive(store,'admit',b['path'])
    a=receive(store,'step','--revision',0,'--operation-id','first-half','--dt',.05)
    database=store/'coupled.sqlite3';before=database.read_bytes()
    assert receive(store,'step','--revision',0,'--operation-id','first-half','--dt',.05)==a
    assert database.read_bytes()==before
    clone=output/'recovered';clone.mkdir();shutil.copy2(database,clone/database.name)
    final=receive(store,'step','--revision',1,'--operation-id','second-half','--dt',.05)
    assert receive(clone,'step','--revision',1,'--operation-id','second-half','--dt',.05)==final
    cp=receive(store,'inspect');assert receive(clone,'inspect')==cp
    assert cp['result']['state']['data']['steps']==10
    assert math.isclose(cp['time_s'],.1)
    totals={key:math.fsum(b['frame']['totals'][source] for b in bundles)
        for key,source in [('energy_j','energy_transferred_j'),('smoke_kg','smoke_transferred_kg')]}
    for key,total in totals.items():
        assert math.isclose(final['budgets'][key]['stored'],total,rel_tol=1e-10,abs_tol=1e-20)
    before=database.read_bytes()
    receive(store,'step','--revision',0,'--operation-id','stale','--dt',.01,fails=True)
    receive(store,'step','--revision',2,'--operation-id','missing-source','--dt',.01,fails=True)
    assert database.read_bytes()==before
    # Inventory refusal precedes any writable store creation.
    adapter=physics/'scripts/coupled_atmosphere.py';original=adapter.read_bytes()
    try:
        adapter.write_bytes(original+b'\n# altered\n')
        receive(output/'must-not-exist','init',fails=True)
        assert not (output/'must-not-exist').exists()
    finally:adapter.write_bytes(original)
    cp_path=output/'checkpoint.json';result_path=output/'result.json'
    cp_path.write_text(json.dumps(cp));result_path.write_text(json.dumps(cp['result']))
    scene=output/'scene'
    run([growth/'bin/combined-scene','export','--source',second['path'],'--physics-result',result_path,
        '--checkpoint',cp_path,'--output',scene,'--reference-concentration',.01],output)
    run([growth/'bin/combined-scene','validate',scene],output)
    result={'status':'passed','physical_time_s':cp['time_s'],'source_intervals':2,'native_fluid_steps':10,
        'exact_replay':True,'exact_recovery':True,'stale_and_missing_source_rollback':True,
        'payload_tamper_refusal':True,'scene_vf3d_validation':True,'totals':totals,
        'platform':os.uname().sysname,'physical_calibration_qualified':False}
    (output/'result-summary.json').write_text(json.dumps(result,indent=2)+'\n')
    return result



def lifecycle(physics, output):
    """Kill an actual native initialization and recover the same store."""
    original=json.loads((output/'config.json').read_text())
    worker=str(physics/'bin/physics_sim_atmosphere_worker')
    def active():
        rows=subprocess.check_output(['ps','-axo','pid=,command='],text=True).splitlines()
        return [int(row.strip().split(None,1)[0]) for row in rows if len(row.strip().split(None,2))>=2 and row.strip().split(None,2)[1]==worker]
    active()
    results=[]
    for number in (signal.SIGTERM,signal.SIGKILL):
        config=json.loads(json.dumps(original));grid=config['policy']['receiver']
        grid['dimensions']=[32,32,32];grid['fluid_mask']=[1]*(32**2)
        config['initial_face_velocity_m_s']=[.2]*(3*32**3)
        path=output/('cancel-config-'+str(number)+'.json');path.write_text(json.dumps(config))
        store=output/('cancel-store-'+str(number));log=output/('cancel-'+str(number)+'.log')
        command=[str(physics/'bin/physics_sim_coupling'),'--runtime',str(output/'runtime'),
            'periodic','--config',str(path),'--store',str(store),'init']
        with log.open('w') as stream:
            child=subprocess.Popen(command,cwd=output,stdout=stream,stderr=stream)
            deadline=time.monotonic()+20
            while child.poll() is None and not active() and time.monotonic()<deadline:time.sleep(.02)
            if child.poll() is not None or not active():
                if child.poll() is None:child.terminate();child.wait(timeout=10)
                raise AssertionError('Cancellation did not reach the native worker: '+log.read_text()[-1000:])
            child.send_signal(number);child.wait(timeout=10)
        deadline=time.monotonic()+10
        while active() and time.monotonic()<deadline:time.sleep(.05)
        assert not active(), 'Native worker survived parent termination'
        # The failed initialization must not commit a partial receiving head.
        run([physics/'bin/physics_sim_coupling','--runtime',output/'runtime','periodic',
            '--config',output/'config.json','--store',store,'init'],output)
        cp=run([physics/'bin/physics_sim_coupling','--runtime',output/'runtime','periodic',
            '--config',output/'config.json','--store',store,'inspect'],output)
        assert cp['revision']==0
        results.append({'signal':number,'native_child_observed':True,'native_child_stopped':True,'same_store_recovered':True})
    (output/'lifecycle-summary.json').write_text(json.dumps(results,indent=2)+'\n')
    return results


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--growth-package',type=Path,required=True)
    p.add_argument('--physics-package',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();result=qualify(a.growth_package.resolve(),a.physics_package.resolve(),a.output.resolve())
    result['native_parent_death_recovery']=lifecycle(a.physics_package.resolve(),a.output.resolve())
    (a.output/'result-summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
