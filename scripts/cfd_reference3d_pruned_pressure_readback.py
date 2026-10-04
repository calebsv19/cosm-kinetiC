#!/usr/bin/env python3
"""Read accepted immutable paired fields; coefficient/residual norms are distinct."""
import sys,time,resource,hashlib,json,shutil
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,ElementDG
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_quartic_pair import quartic_quadrature,CubicPressureMass
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pruned-graph'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(stem,folder):
    p=next((D/folder).glob('*/'+stem+'-receipt.json'));r,row=verify_receipt(p)
    assert r['returncode']==0 and row['final_residual']['true_residual']<1e-10
    snapshot=Path(r['command'][r['command'].index('--snapshot')+1]);return p,r,row,snapshot

def run():
    out=D/'pressure-readback.json';assert not out.exists();begin=time.monotonic();pairs=[];inputs={}
    choices=(('L4-body2-original-pruned0','runs','L4-body2-original-pruned1e3','runs'),('L4-body6-normal-pruned0','runs','L4-body6-normal-pruned1e3','runs'),('L4-body6-normal-complete-fresh','control-runs','L4-body6-normal-pruned0','runs'))
    for a,af,b,bf in choices:
        ap,ar,aa,ax=load(a,af);bp,br,bb,bx=load(b,bf)
        assert aa['identity']==bb['identity'];inputs.update({str(ap):sha(ap),str(bp):sha(bp),str(ax):sha(ax),str(bx):sha(bx)})
        with np.load(ax,allow_pickle=False) as x,np.load(bx,allow_pickle=False) as y:
            np.testing.assert_array_equal(x['vertices_m'],y['vertices_m']);np.testing.assert_array_equal(x['tetrahedra'],y['tetrahedra'])
            mesh=MeshTet(x['vertices_m'],x['tetrahedra']);p=x['pressure_coefficients'];q=y['pressure_coefficients'];mu=float(x['mu'])
        pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=quartic_quadrature(),elements=np.array([0]));mass=CubicPressureMass(pb,mu)
        delta=q-p;volume=float(mass.determinant.sum()/6);one=np.ones_like(p);integral=float(one@mass.apply(delta))
        def macro_mean(v):return v.reshape(-1,20).mean(axis=1).reshape(4,-1).mean(axis=0)
        means=macro_mean(q)-macro_mean(p)
        pairs.append(dict(base=a,comparison=b,velocity_or_pressure_forward_accuracy_certified=False,coefficient_relative_difference=float(np.linalg.norm(delta)/np.linalg.norm(p)),pressure_l2_relative_difference=mass.norm(delta)/mass.norm(p),pressure_l2_absolute_difference=mass.norm(delta),global_pressure_mean_difference_pa=integral/volume,retained_macro_mean_relative_difference=float(np.linalg.norm(means)/np.linalg.norm(macro_mean(p))),maximum_retained_macro_mean_difference_pa=float(np.abs(means).max()),original_full_residuals=dict(base=aa['final_residual'],comparison=bb['final_residual']),scope='observed coefficient and physical pressure-mass/global-mean differences on accepted paired fields; residual target is not a forward-error bound'))
    for p,h in inputs.items():assert sha(Path(p))==h
    peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss;wall=time.monotonic()-begin;assert peak<1800*1024**2 and wall<180
    sources=('scripts/cfd_reference3d_pruned_pressure_readback.py','scripts/cfd_reference3d_quartic_pair.py','scripts/cfd_reference3d_p3.py','scripts/audit_cfd_3d_spatial.py')
    frozen=D/'pressure-readback-source';frozen.mkdir()
    for name in sources:shutil.copy2(R/name,frozen/Path(name).name)
    row=dict(diagnostic_accepted=True,physical_accuracy_certified=False,pairs=pairs,input_sha256=inputs,source_sha256={name:sha(R/name) for name in sources},peak_rss_bytes=peak,wall_s=wall)
    out.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(pairs,indent=2))
if __name__=='__main__':run()
