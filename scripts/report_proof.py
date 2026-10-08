"""Retain a bounded open-atmosphere convergence report in a fresh sealed capsule."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import uuid
from build_owner import worker_execution,execution_descriptors,inherited_descriptors
from cfd_evidence import admitted_path,experiment_root,file_fingerprint,sha,seal_bundle,read_json
from tool_probe import capture


def run(repo,worker,parent,script,sources=()):
    repo=repo.resolve();parent=experiment_root(repo,parent)
    script=admitted_path(script)
    if not script.is_relative_to(repo/'tests') or script.suffix!='.py':
        raise ValueError('Report script requires checkout/tests Python source')
    inputs={admitted_path(p) for p in [script,*sources,Path(__file__)]}
    if any(not p.is_relative_to(repo) for p in inputs):raise ValueError('Report inputs require checkout source')
    python=Path(sys.executable).resolve();python_identity=file_fingerprint(python)
    with worker_execution(repo,worker) as worker:
        identities={p:file_fingerprint(p) for p in inputs};worker_identity=file_fingerprint(worker)
        parent.mkdir(parents=True,exist_ok=True);capsule=parent/('open-atmosphere-convergence-'+uuid.uuid4().hex);capsule.mkdir()
        state={'schema':'physics_sim_retained_report_v1','artifact_class':'retained_evidence','status':'running',
               'worker':str(worker),'worker_sha256':worker_identity['sha256'],'python':str(python),
               'python_sha256':python_identity['sha256'],'source_sha256':{str(p.relative_to(repo)):r['sha256'] for p,r in identities.items()},
               'terminal_processes_verified':True,'complete_dependency_capture':False,'backup_verified':False,'physical_accuracy_certified':False}
        (capsule/'request.json').write_text(json.dumps(state,indent=2)+'\n')
        print('Convergence proof retained: '+str(capsule),flush=True)
        try:
            for p,row in identities.items():
                dest=capsule/'source'/p.relative_to(repo);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
                if sha(dest)!=row['sha256']:raise ValueError('Report input changed during snapshot')
            shutil.copy2(worker,capsule/'worker');
            if sha(capsule/'worker')!=worker_identity['sha256']:raise ValueError('Worker changed during snapshot')
            fds=execution_descriptors();owner=worker.parent
            env=dict(os.environ,PHYSICS_SIM_OPEN_ATMOSPHERE_WORKER=str(worker),PHYSICS_SIM_BUILD_OWNER_ROOT=str(owner),
                     PHYSICS_SIM_BUILD_HIERARCHY_FDS=json.dumps(fds),PHYSICS_SIM_BUILD_GLOBAL_FD=str(fds[0]),PHYSICS_SIM_BUILD_ROOT_FD=str(fds[-1]),PYTHONDONTWRITEBYTECODE='1')
            # A borrowed Make owner can be wider than the selected worker directory.
            inherited_root=os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
            if inherited_root and inherited_descriptors(repo,Path(inherited_root))==fds:
                env['PHYSICS_SIM_BUILD_OWNER_ROOT']=inherited_root
            row=capture([str(python),'-B',str(script),'--report',str(capsule/'metrics.json')],env=env,timeout=120,stdout_limit=1048576,stderr_limit=1048576,pass_fds=fds)
            state['terminal_processes_verified']=row.get('terminal_processes_verified') is True
            state['terminal_verification_scope']=row.get('terminal_verification_scope')
            state['direct_capture_anchor_reaped']=row.get('direct_child_reaped') is True
            state['all_external_descendants_verified_terminal']=False
            for key in ('stdout','stderr'):(capsule/(key+'.log')).write_bytes(row[key])
            if row['status']!='passed':raise ValueError(row.get('reason') or 'Report child exited '+str(row.get('exit_code')))
            metrics=read_json(capsule/'metrics.json',67108864)
            if (not isinstance(metrics,dict) or metrics.get('schema')!='physics_sim_open_convergence/v1'
                    or type(metrics.get('tests_passed')) is not int or metrics['tests_passed']!=3 or not isinstance(metrics.get('metrics'),dict)):
                raise ValueError('Invalid convergence metrics contract')
            if file_fingerprint(worker)!=worker_identity or file_fingerprint(python)!=python_identity or any(file_fingerprint(p)!=r for p,r in identities.items()):
                raise ValueError('Report source, interpreter or worker changed during proof')
            state.update(status='passed',tests_passed=3)
        except (Exception,KeyboardInterrupt) as error:state.update(status='failed',failure=str(error))
        (capsule/'receipt.json').write_text(json.dumps(state,indent=2)+'\n')
        return {'status':state['status'],'capsule':str(capsule),'readback':seal_bundle(capsule) if state['terminal_processes_verified'] else {'status':'held','reason':'Unverified owned process teardown; attempt remains unsealed'},'failure':state.get('failure')}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--worker',type=Path,required=True);p.add_argument('--parent',type=Path,required=True);p.add_argument('--script',type=Path,required=True);p.add_argument('--source',type=Path,action='append',default=[]);a=p.parse_args()
    try:
        r=run(Path.cwd(),a.worker,a.parent,a.script,a.source);print(json.dumps(r));raise SystemExit(0 if r['status']=='passed' else 2)
    except (ValueError,OSError) as error:p.exit(2,'Convergence proof held: '+str(error)+'\n')

if __name__=='__main__':main()
