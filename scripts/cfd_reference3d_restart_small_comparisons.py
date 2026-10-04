#!/usr/bin/env python3
"""Immutable readback of complete restart controls and optional matched force field."""
import json,hashlib
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,ElementDG
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_quartic_pair import quartic_quadrature,CubicPressureMass
from audit_cfd_3d_spatial import verify_receipt,force_comparison
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-restart-small'; P=R/'build/c3d-restart'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(stem,root=D):
 p=next((root/'runs').glob('*/'+stem+'-receipt.json'));r,a=verify_receipt(p);assert r['returncode']==0 and a['final_residual']['true_residual']<1e-10;return p,r,a

def calibration():
 out=D/'calibration-comparisons.json';assert not out.exists();cal={};inputs={}
 for count,group in ((2,'original'),(6,'normal')):
  base=f'L4-body{count}-{group}-r60';bp,br,b=load(base,P);bx=Path(br['command'][br['command'].index('--snapshot')+1]);inputs.update({str(bp):sha(bp),str(bx):sha(bx)})
  with np.load(bx,allow_pickle=False) as x:
   mesh=MeshTet(x['vertices_m'],x['tetrahedra']);p=x['pressure_coefficients'];u=x['velocity_coefficients'];mu=float(x['mu']);pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=quartic_quadrature(),elements=np.array([0]));mass=CubicPressureMass(pb,mu)
  for restart in (8,4):
   stem=f'L4-body{count}-{group}-r{restart}';path=next((D/'runs').glob('*/'+stem+'-receipt.json')); r,a=verify_receipt(path); inputs[str(path)]=sha(path)
   if r['returncode']!=0:
    cal[stem]=dict(receipt=str(path),receipt_sha256=sha(path),rejected=True,numerical_failure_reasons=a['numerical_failure_reasons'],requested_full_residual=a['final_residual'],force_comparison=None,field_published=False);continue
   assert a['final_residual']['true_residual']<1e-10
   snap=Path(r['command'][r['command'].index('--snapshot')+1]);inputs.update({str(path):sha(path),str(snap):sha(snap)});assert a['identity']==b['identity']
   with np.load(snap,allow_pickle=False) as x:
    np.testing.assert_array_equal(mesh.p,x['vertices_m']);np.testing.assert_array_equal(mesh.t,x['tetrahedra']);q=x['pressure_coefficients'];v=x['velocity_coefficients'];delta=q-p
    forward=dict(velocity_coefficient_relative_difference=float(np.linalg.norm(v-u)/np.linalg.norm(u)),pressure_coefficient_relative_difference=float(np.linalg.norm(delta)/np.linalg.norm(p)),pressure_mass_relative_difference=mass.norm(delta)/mass.norm(p),pressure_global_mean_difference_pa=float(np.ones_like(p)@mass.apply(delta))/(mass.determinant.sum()/6),forward_accuracy_certified=False)
   cal[stem]=dict(receipt=str(path),receipt_sha256=sha(path),restart=restart,cost_ratios=dict(wall_s=r['wall_s']/br['wall_s'],owned_peak_rss=a['peak_rss_bytes']/b['peak_rss_bytes'],basis_reservation=a['preconditioner']['flexible_iteration']['basis_reservation_bytes']/b['preconditioner']['flexible_iteration']['basis_reservation_bytes']),force_comparison=force_comparison(a,b),pressure_and_velocity_readback=forward,requested_target_met=True)
 for p,h in inputs.items():assert sha(Path(p))==h
 out.write_text(json.dumps(dict(calibration=cal,input_sha256=inputs,physical_accuracy_certified=False,default_restart_changed=False),indent=2)+'\n');print(json.dumps(cal,indent=2))
if __name__=='__main__':calibration()
