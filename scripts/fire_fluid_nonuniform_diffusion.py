"""Bounded interface diffusion repair: face tests, then fixed source histories."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from fire_fluid_sharp_transport import relative
from fire_fluid_thermal_transport import execute


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return [json.loads(x) for x in p.read_text().splitlines()]
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser()
    for key in ['build','output','previous','references']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--reuse-native',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=a.reuse_native)
    build=a.build.resolve()
    old_identity=json.loads((a.previous/'runs/identity.json').read_text())
    preserved={path:digest for path,digest in old_identity['inputs'].items() if Path(path).name in ['physics_sim_nonuniform_advection.h','physics_sim_local_matrix.h','physics_sim_scalar_fct.h','physics_sim_scalar_logquadratic.h','physics_sim_scalar_front_target.h','physics_sim_scalar_exponential.h']}
    assert len(preserved)==6 and all(sha(Path(path))==digest for path,digest in preserved.items())
    inputs=[root/'src/tools/physics_sim_nonuniform_diffusion.h',root/'src/tools/physics_sim_nonuniform_diffusion_probe.c',
            root/'src/app/cfd_memory.c',root/'include/app/cfd_memory.h',root/'src/app/cfd_scalar_edge_candidate.h',
            root/'src/tools/physics_sim_scalar_subcell.h',root/'tests/cfd_nonuniform_diffusion_test.c',
            root/'tests/cfd_nonuniform_advection_test.c',root/'tests/cfd_scalar_fct_test.c',root/'tests/cfd_local_matrix_test.c',Path(__file__)]+[Path(path) for path in preserved]
    ref_paths={name:a.references/'runs'/f'{name}-continuous.json' for name in ['diffusion','advection']}
    prior_ref_manifest=json.loads((a.references/'runs/native-sha256.json').read_text())
    assert all(sha(path)==prior_ref_manifest[path.name] for path in ref_paths.values())
    if a.reuse_native:
        identity=json.loads((out/'identity.json').read_text())
        assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
        assert all(sha(out/path)==digest for path,digest in json.loads((out/'native-sha256.json').read_text()).items())
    else:
        build.mkdir(parents=True,exist_ok=False)
        identity=dict(inputs={str(path):sha(path) for path in inputs},preserved=preserved,commands=[])
        flags=['clang','-std=c11','-Wall','-Wextra','-Werror','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'src/tools')]
        targets=[('probe',root/'src/tools/physics_sim_nonuniform_diffusion_probe.c',['-O2']),
          ('probe-sanitize',root/'src/tools/physics_sim_nonuniform_diffusion_probe.c',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),
          ('face-test',root/'tests/cfd_nonuniform_diffusion_test.c',['-O2']),
          ('face-test-sanitize',root/'tests/cfd_nonuniform_diffusion_test.c',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']),
          ('advection-test',root/'tests/cfd_nonuniform_advection_test.c',['-O2']),
          ('limiter-test',root/'tests/cfd_scalar_fct_test.c',['-O2']),('matrix-test',root/'tests/cfd_local_matrix_test.c',['-O2'])]
        for name,source,extra in targets:
            cmd=flags+extra+[str(source),str(root/'src/app/cfd_memory.c'),'-lm','-o',str(build/name)]
            subprocess.run(cmd,check=True,timeout=60);identity['commands'].append(cmd)
        identity['binaries']={name:sha(build/name) for name,_,_ in targets}
        assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
        write(out/'identity.json',identity)
        for name in ['face-test','face-test-sanitize','advection-test','limiter-test','matrix-test']:
            result=subprocess.run([build/name],capture_output=True,text=True,timeout=60)
            (out/(name+'.log')).write_text(result.stdout+result.stderr)
            assert result.returncode==0,(name,result.stdout,result.stderr)
        assert (out/'face-test.log').read_bytes()==(out/'face-test-sanitize.log').read_bytes()
    refs={key:json.loads(path.read_text())['256'] for key,path in ref_paths.items()}
    cases={};raw={}
    # Begin with diffusion only; advection follows only after successful execution.
    for name,uniform,u,repair,prior in [('diffusion-baseline',0,0,0,'mixed-diffusion'),
       ('diffusion-repaired',0,0,1,None),('uniform-diffusion-recovery',1,0,1,None),
       ('advection-baseline',0,.025,0,'mixed-candidate'),('advection-repaired',0,.025,1,None)]:
        if a.reuse_native:rows=read(out/(name+'.jsonl'));san=read(out/(name+'-sanitize.jsonl'))
        else:
            rows=execute([build/'probe',uniform,u,2,repair],out/(name+'.jsonl'),120)
            san=execute([build/'probe-sanitize',uniform,u,2,repair],out/(name+'-sanitize.jsonl'),120)
        assert rows==san,'strict/sanitizer mismatch'
        terminal=rows.pop();assert terminal['steps']==12800 and terminal['memory_live_bytes']==0
        assert terminal['minimum_k']>=0 and terminal['maximum_k']<=.4+1e-10
        assert terminal['inventory_error_k_m']<1e-11
        assert terminal['diffusion_steps']==(12800 if repair and not uniform else 0)
        assert [r['step'] for r in rows]==list(range(0,12801,640))
        assert all(np.isfinite(v).all() for r in rows for v in r.values())
        reference=refs['advection' if u else 'diffusion']
        if prior:assert rows==read(a.previous/'runs'/f'{prior}.jsonl')[:-1],'baseline changed'
        if uniform:
            original=read(a.references/'runs/diffusion.jsonl')[:-1]
            assert all(all(r[key]==o[key] for key in ['transport_k','tail_k','inventory_k_m','source_k_m']) for r,o in zip(rows,original))
        metrics={key:relative([r[key] for r in rows],[r[key] for r in reference]) for key in ['transport_k','tail_k']}
        points={key:dict(failed_times_s=[i*.025 for i in range(1,21) if abs(rows[i][key]/reference[i][key]-1)>.1],first_native_k=rows[1][key],first_reference_k=reference[1][key],final_relative=rows[-1][key]/reference[-1][key]-1) for key in ['transport_k','tail_k']}
        cases[name]=dict(history_errors=metrics,gates={key:value<=.1 for key,value in metrics.items()},points=points,terminal=terminal,final=rows[-1]);raw[name]=rows
        print(json.dumps(dict(case=name,metrics=metrics,points=points['tail_k'])),flush=True)
    if not a.reuse_native:write(out/'native-sha256.json',{str(path.relative_to(out)):sha(path) for path in out.iterdir() if path.name!='identity.json'})
    assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
    write(out/'report.json',dict(threshold=.1,interval_s=[0,.5],interface_m=3/32,dt_s=1/25600,cases=cases,preserved=preserved,
       reference_inputs={str(path.resolve()):sha(path) for path in ref_paths.values()},feedback_run=False,coupled_adoption=False))


if __name__=='__main__':main()
