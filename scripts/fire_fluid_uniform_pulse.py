"""Bounded 4096-cell uniform source-pulse control: diffusion, then advection."""
import argparse
from decimal import Decimal
import hashlib
import json
from pathlib import Path
import subprocess
import time
import numpy as np
from fire_fluid_uniform_reference import continuous, spectrum
from fire_fluid_sharp_transport import relative
from fire_fluid_thermal_transport import execute


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return [json.loads(line) for line in path.read_text().splitlines()]
def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--reuse-native',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[1]
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=a.reuse_native)
    build=a.build.resolve()
    inputs=[root/'src/tools/physics_sim_uniform_pulse_probe.c',root/'src/tools/physics_sim_local_matrix.h',
            root/'src/app/cfd_memory.c',root/'include/app/cfd_memory.h',Path(__file__),
            Path(__file__).with_name('fire_fluid_uniform_reference.py'),root/'tests/cfd_local_matrix_test.c',
            root/'src/tools/physics_sim_scalar_subcell.h']
    limiter_paths=[root/'src/tools'/name for name in ['physics_sim_scalar_fct.h','physics_sim_scalar_logquadratic.h','physics_sim_scalar_front_target.h','physics_sim_scalar_exponential.h']]
    if a.reuse_native:
        identity=json.loads((out/'identity.json').read_text())
        assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
        assert all(sha(Path(path))==digest for path,digest in identity['limiter'].items())
        assert all(sha(out/path)==digest for path,digest in json.loads((out/'native-sha256.json').read_text()).items())
    else:
        build.mkdir(parents=True,exist_ok=False)
        identity=dict(inputs={str(path):sha(path) for path in inputs},limiter={str(path):sha(path) for path in limiter_paths},commands=[])
        flags=['clang','-std=c11','-Wall','-Wextra','-Werror','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'src/tools')]
        for name,extra in [('probe',['-O2']),('probe-sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
            cmd=flags+extra+[str(inputs[0]),str(inputs[2]),'-lm','-o',str(build/name)]
            subprocess.run(cmd,check=True,timeout=60);identity['commands'].append(cmd)
        cmd=flags+['-O2',str(root/'tests/cfd_local_matrix_test.c'),str(inputs[2]),'-lm','-o',str(build/'matrix-test')]
        subprocess.run(cmd,check=True,timeout=60);identity['commands'].append(cmd)
        identity['binaries']={name:sha(build/name) for name in ['probe','probe-sanitize','matrix-test']}
        assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
        write(out/'identity.json',identity)
    cases=[]
    for name,u in [('diffusion',0),('advection',.025)]:
        if a.reuse_native:
            rows=read(out/(name+'.jsonl'));san=read(out/(name+'-sanitize.jsonl'))
        else:
            rows=execute([build/'probe',u],out/(name+'.jsonl'),120)
            san=execute([build/'probe-sanitize',u],out/(name+'-sanitize.jsonl'),120)
        assert rows==san,'strict/sanitizer mismatch'
        terminal=rows.pop();assert terminal['steps']==12800 and terminal['cells']==4096
        assert terminal['memory_live_bytes']==0 and terminal['minimum_k']>=0
        assert terminal['inventory_error_k_m']<1e-11
        assert [r['step'] for r in rows]==list(range(0,12801,640))
        if a.reuse_native:
            refs=json.loads((out/(name+'-continuous.json')).read_text())
            spectra={digits:json.loads((out/(name+f'-spectrum{digits}.json')).read_text()) for digits in [60,90]}
        else:
            refs={}
            for points in [64,128,256]:
                refs[str(points)]=[{key:continuous(i*.025,u,lo,hi,points) for key,lo,hi in [('transport_k',1/16,3/32),('tail_k',3/32,1/8)]} for i in range(21)]
            write(out/(name+'-continuous.json'),refs)
            spectra={}
            for digits in [60,90]:
                start=time.monotonic();spectra[digits]=spectrum(u,digits)
                write(out/(name+f'-spectrum{digits}.json'),spectra[digits])
                print(f'{name}: {digits}-digit spatial/BE spectrum completed in {time.monotonic()-start:.2f}s',flush=True)
        qualification={key:max(relative([r[key] for r in refs[lo]],[r[key] for r in refs[hi]]) for lo,hi in [('64','128'),('128','256')]) for key in ['transport_k','tail_k']}
        assert all(x<1e-8 for x in qualification.values()),qualification
        metrics={key:relative([r[key] for r in rows],[r[key] for r in refs['256']]) for key in ['transport_k','tail_k']}
        splits=[]
        for low,high in zip(spectra[60],spectra[90]):
            i=round(high['time_s']/.025);parts={}
            for key in ['transport_k','tail_k']:
                for method in ['semi','be']:
                    x,y=Decimal(low[method][key]),Decimal(high[method][key])
                    assert y>0 and abs(x-y)<abs(y)*Decimal('1e-30'),(name,method,key,high['time_s'])
                exact=refs['256'][i][key];semi=float(high['semi'][key]);be=float(high['be'][key]);native=rows[i][key]
                assert abs(native/be-1)<1e-8,(name,key,native,be)
                if 'continuous512' in high:
                    c256=float(high['continuous256'][key]);c512=float(high['continuous512'][key])
                    assert abs(c256/c512-1)<1e-10 and abs(exact/c512-1)<1e-9,(name,key,'continuous spectral check',exact,c256,c512)
                parts[key]=dict(physical_k=exact,semidiscrete_k=semi,be_spectral_k=be,native_k=native,
                                spatial_relative=semi/exact-1,time_relative=be/semi-1,total_relative=native/exact-1,
                                native_vs_be_relative=native/be-1)
            splits.append(dict(time_s=high['time_s'],parts=parts))
        point_checks={}
        for key in ['transport_k','tail_k']:
            changes=[abs(x[key]/y[key]-1) for x,y in zip(refs['128'][1:],refs['256'][1:])]
            assert max(changes)<1e-8,(name,key,'pointwise quadrature qualification')
            failures=[i for i in range(1,21) if abs(rows[i][key]/refs['256'][i][key]-1)>.1]
            point_checks[key]=dict(max_reference_quadrature_change=max(changes),failed_times_s=[i*.025 for i in failures],
                first_native_k=rows[1][key],first_reference_k=refs['256'][1][key],first_ratio=rows[1][key]/refs['256'][1][key])
        case=dict(point_checks=point_checks,name=name,velocity_m_s=u,history_errors=metrics,gates={k:v<=.1 for k,v in metrics.items()},
                  continuous_quadrature_changes=qualification,operator_split=splits,terminal=terminal)
        cases.append(case);print(json.dumps(dict(name=name,history_errors=metrics,gates=case['gates'],final_tail_split=splits[-1]['parts']['tail_k'])),flush=True)
    if not a.reuse_native:
        result=subprocess.run([build/'matrix-test'],capture_output=True,text=True,check=True,timeout=60)
        (out/'matrix-test.log').write_text(result.stdout+result.stderr)
        artifacts={str(path.relative_to(out)):sha(path) for path in out.iterdir() if path.name!='identity.json'}
        write(out/'native-sha256.json',artifacts)
    assert all(sha(Path(path))==digest for path,digest in identity['inputs'].items())
    assert all(sha(Path(path))==digest for path,digest in identity['limiter'].items())
    write(out/'report.json',dict(threshold=.1,interval_s=[0,.5],source_x_m=[1/32,1/16],source_end_s=.4,
          dt_s=1/25600,dx_m=1/16384,cells=4096,alpha_m2_s=.026/1206,source_k_s=1,
          cases=cases,feedback_run=False,coupled_qualification=False,limiter_unchanged=True,
          reference_layers='positive continuous kernel; 60/90-digit exact spatial semigroup and exact BE/pulse spectrum; late continuous Fourier256/512 cross-check'))


if __name__=='__main__':main()
