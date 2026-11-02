"""Bounded higher-order advection candidate; unchanged paired FCT limiter."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from fire_fluid_sharp_transport import relative
from fire_fluid_thermal_transport import execute


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return [json.loads(line) for line in path.read_text().splitlines()]
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n')


def specialize(text,root):
    # Only workspace capacity, admission capacity, and include routing change.
    assert text.count('[256]')==4 and text.count('n>256')==1
    return text.replace('[256]','[4096]').replace('n>256','n>4096').replace(
        '"../app/cfd_scalar_edge_candidate.h"','"'+str(root/'src/app/cfd_scalar_edge_candidate.h')+'"')


def main():
    p=argparse.ArgumentParser()
    for name in ['build','output','previous']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--reuse-native',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.output.resolve();build=a.build.resolve()
    out.mkdir(parents=True,exist_ok=a.reuse_native)
    names=['physics_sim_scalar_fct.h','physics_sim_scalar_logquadratic.h','physics_sim_scalar_front_target.h','physics_sim_scalar_exponential.h','physics_sim_local_matrix.h']
    preserved={str(root/'src/tools'/name):sha(root/'src/tools'/name) for name in names}
    old=json.loads((a.previous/'preserved-operators.json').read_text())
    assert all(preserved[str(root/'src/tools'/name)]==digest for name,digest in old.items())
    inputs=[root/'src/tools/physics_sim_uniform_advection_probe.c',root/'src/tools/physics_sim_uniform_advection.h',
            root/'src/app/cfd_memory.c',root/'include/app/cfd_memory.h',root/'src/app/cfd_scalar_edge_candidate.h',
            root/'tests/cfd_uniform_advection_test.c',root/'tests/cfd_local_matrix_test.c',root/'tests/cfd_scalar_fct_test.c',
            root/'src/tools/physics_sim_scalar_subcell.h',Path(__file__)]+[Path(x) for x in preserved]
    generated=specialize((root/'src/tools/physics_sim_scalar_fct.h').read_text(),root)
    if a.reuse_native:
        identity=json.loads((out/'identity.json').read_text())
        assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
        assert hashlib.sha256(generated.encode()).hexdigest()==identity['specialized_header_sha256']
        assert all(sha(out/path)==digest for path,digest in json.loads((out/'native-sha256.json').read_text()).items())
    else:
        build.mkdir(parents=True,exist_ok=False)
        (build/'physics_sim_scalar_fct_4096.h').write_text(generated)
        (out/'capacity-specialization.patch').write_text(''.join(difflib.unified_diff((root/'src/tools/physics_sim_scalar_fct.h').read_text().splitlines(True),generated.splitlines(True),fromfile='original-fct',tofile='capacity4096-fct')))
        identity=dict(inputs={str(path):sha(path) for path in inputs},preserved=preserved,
                      specialized_header_sha256=sha(build/'physics_sim_scalar_fct_4096.h'),commands=[])
        flags=['clang','-std=c11','-Wall','-Wextra','-Werror','-ffp-contract=off','-I'+str(build),'-I'+str(root/'include'),'-I'+str(root/'src/tools')]
        targets=[('probe',root/'src/tools/physics_sim_uniform_advection_probe.c',['-O2']),
                 ('probe-sanitize',root/'src/tools/physics_sim_uniform_advection_probe.c',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),
                 ('candidate-test',root/'tests/cfd_uniform_advection_test.c',['-O2']),
                 ('candidate-test-sanitize',root/'tests/cfd_uniform_advection_test.c',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),
                 ('matrix-test',root/'tests/cfd_local_matrix_test.c',['-O2']),
                 ('limiter-test',root/'tests/cfd_scalar_fct_test.c',['-O2'])]
        for name,source,extra in targets:
            cmd=flags+extra+[str(source),str(root/'src/app/cfd_memory.c'),'-lm','-o',str(build/name)]
            subprocess.run(cmd,check=True,timeout=60);identity['commands'].append(cmd)
        identity['binaries']={name:sha(build/name) for name,_,_ in targets}
        assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
        write(out/'identity.json',identity)
        for name in ['candidate-test','candidate-test-sanitize','matrix-test','limiter-test']:
            result=subprocess.run([build/name],capture_output=True,text=True,timeout=60)
            (out/(name+'.log')).write_text(result.stdout+result.stderr)
            assert result.returncode==0,(name,result.stdout,result.stderr)
        assert (out/'candidate-test.log').read_bytes()==(out/'candidate-test-sanitize.log').read_bytes()
    references={name:json.loads((a.previous/'runs'/f'{name}-continuous.json').read_text())['256'] for name in ['diffusion','advection']}
    cases={};raw={}
    for name,u,method in [('baseline-diffusion',0,0),('baseline-advection',.025,0),('split-donor',.025,1),('candidate-diffusion',0,2),('candidate-advection',.025,2)]:
        if a.reuse_native:rows=read(out/(name+'.jsonl'));san=read(out/(name+'-sanitize.jsonl'))
        else:
            rows=execute([build/'probe',u,method],out/(name+'.jsonl'),120)
            san=execute([build/'probe-sanitize',u,method],out/(name+'-sanitize.jsonl'),120)
        assert rows==san,'strict/sanitizer mismatch'
        terminal=rows.pop();assert terminal['steps']==12800 and terminal['memory_live_bytes']==0
        assert terminal['minimum_k']>=0 and terminal['maximum_k']<=.4+1e-10
        assert terminal['inventory_error_k_m']<1e-11
        assert terminal['advection_stage_checks']==(38400 if u and method else 0)
        assert [r['step'] for r in rows]==list(range(0,12801,640))
        assert all(np.isfinite(v).all() for r in rows for v in r.values())
        reference=references['advection' if u else 'diffusion']
        if method==0:
            previous=read(a.previous/'runs'/('advection.jsonl' if u else 'diffusion.jsonl'))
            assert rows==previous[:-1],'baseline changed'
        if name=='candidate-diffusion':assert rows==raw['baseline-diffusion'],'diffusion operator changed'
        metrics={key:relative([r[key] for r in rows],[r[key] for r in reference]) for key in ['transport_k','tail_k']}
        points={key:dict(failed_times_s=[i*.025 for i in range(1,21) if abs(rows[i][key]/reference[i][key]-1)>.1],
                        first_native_k=rows[1][key],first_reference_k=reference[1][key],
                        final_relative=rows[-1][key]/reference[-1][key]-1) for key in ['transport_k','tail_k']}
        cases[name]=dict(velocity_m_s=u,method=method,history_errors=metrics,gates={k:v<=.1 for k,v in metrics.items()},points=points,terminal=terminal)
        raw[name]=rows
        print(json.dumps(dict(case=name,metrics=metrics,tail_points=points['tail_k'])),flush=True)
    refs={str((a.previous/'runs'/f'{name}-continuous.json').resolve()):sha(a.previous/'runs'/f'{name}-continuous.json') for name in references}
    if not a.reuse_native:
        write(out/'native-sha256.json',{str(path.relative_to(out)):sha(path) for path in out.iterdir() if path.name!='identity.json'})
    assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
    write(out/'report.json',dict(threshold=.1,cells=4096,dx_m=1/16384,dt_s=1/25600,interval_s=[0,.5],
          cases=cases,reference_inputs=refs,limiter_algorithm_unchanged=True,cell_bounds_unchanged=True,
          capacity_specialization='Only n admission/workspace 256→4096 and include routing; original header untouched',
          feedback_run=False,coupled_adoption=False))


if __name__=='__main__':main()
