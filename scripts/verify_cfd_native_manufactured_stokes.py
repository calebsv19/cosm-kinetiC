"""Verify physical forcing factors and measured native velocity/pressure refinement."""
import math,json,hashlib,subprocess,shutil
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-native-manufactured-stokes'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def derivative(t,d):
    result=0.
    for j in range(d+1):
        k=d-j;result+=math.comb(d,j)*math.factorial(4)/math.factorial(4-j)*t**(4-j)*(-1)**k*math.factorial(4)/math.factorial(4-k)*(1-t)**(4-k)
    return 256*result

def main():
    out=D/'checkpoint-audit.json';assert not out.exists();contract=json.loads((D/'source-contract.json').read_text())
    for q,h in contract['source_sha256'].items():assert sha(R/q)==h==sha(D/'frozen'/q)
    assert sha(D/'n8.json')==contract['coarse_n8_sha256']
    X,W=np.polynomial.legendre.leggauss(6);low=[.25,.25,.25];high=[1.25,1.75,1.75];maximum=0.;integrals=json.loads((D/'integrals.json').read_text());assert len(integrals)==84
    for axis,a,b,d,value in integrals:
        lower=max(a,low[axis]);upper=min(b,high[axis]);expected=0
        if upper>lower:
            x=(upper+lower)/2+(upper-lower)/2*X;t=(x-low[axis])/(high[axis]-low[axis]);expected=float(np.dot(W,derivative(t,d))/(high[axis]-low[axis])**d*(upper-lower)/(2*(b-a)))
        maximum=max(maximum,abs(expected-value));assert abs(expected-value)<2e-8
    rows={n:json.loads((D/f'n{n}.json').read_text()) for n in contract['resolutions']};orders={}
    for n,row in rows.items():assert row['momentum_residual']<1e-11 and row['maximum_divergence']<1e-8 and row['wall_s']<contract['wall_cap_s'] and row['peak_owned_bytes']<contract['owned_cap_bytes'] and 0<row['velocity_relative_error'] and 0<row['pressure_relative_error']
    for n in (16,32):
        orders[n]={}
        for key in ('velocity_relative_error','pressure_relative_error'):
            ratio=rows[n][key]/rows[n//2][key];assert ratio<=contract['maximum_error_ratio_each_refinement'];orders[n][key]=math.log2(1/ratio)
    assert rows[32]['velocity_relative_error']<=contract['velocity_n32_max'] and rows[32]['pressure_relative_error']<=contract['pressure_n32_max']
    source={**contract['source_sha256'],'tests/cfd_obstacle3d_manufactured_integral_probe.c':sha(R/'tests/cfd_obstacle3d_manufactured_integral_probe.c'),'scripts/verify_cfd_native_manufactured_stokes.py':sha(Path(__file__))}
    for q in ('tests/cfd_obstacle3d_manufactured_integral_probe.c','scripts/verify_cfd_native_manufactured_stokes.py'):
        p=D/'verification-frozen'/q;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(R/q,p)
    result=dict(status='NATIVE MANUFACTURED STOKES CALIBRATION PASSES; SPATIAL ERROR MEASURED',source_sha256=source,source_contract_sha256=sha(D/'source-contract.json'),artifact_sha256={p.name:sha(p) for p in D.iterdir() if p.is_file()},integral_samples=84,maximum_independent_integral_error=maximum,controls=rows,observed_orders=orders,compiler=subprocess.check_output(['clang','--version'],text=True).splitlines()[0],native_physical_operator_changed=False,new_native_manufactured_solutions=3,physical_cube_force_qualification=False,persistent_goal_complete=False,scope='independent compact physical forcing, known divergence-free face-area velocity averages and cell-volume pressure means on the native steady cube solver; new smooth-case diagnostic acceptance, not cube force, general-object or transient certification')
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(status=result['status'],sha256=sha(out),orders=orders)))
if __name__=='__main__':main()
