#!/usr/bin/env python3
"""Bounded mixed Stokes verification from native hybrid matrices.

Requires the isolated CFD reference Python environment (NumPy/SciPy). This is
an algebra/physics oracle for correcting the native split prototype, not a
worker backend, performance qualification, or a new independent mesh reference.
"""
import argparse
import json
from pathlib import Path
import numpy as np
from scipy import sparse
from scipy.sparse.linalg import splu


def exact(x, y, t):
    a, b = np.pi / 4, np.pi / 2
    s, q, e = np.sin(a*x), np.cos(a*x), .01*np.exp(-t)
    X, X1 = s**4, 4*a*s**3*q
    X2 = 4*a*a*(3*s*s*q*q-X)
    X3 = 4*a**3*(6*s*q**3-10*s**3*q)
    Y, Y1 = np.sin(b*y)**2, b*np.sin(2*b*y)
    Y2, Y3 = 2*b*b*np.cos(2*b*y), -4*b**3*np.sin(2*b*y)
    up, vp = e*X*Y1, -e*X1*Y
    u, v = 6*.02*(y/2)*(1-y/2)+up, vp
    # Continuous unsteady Stokes forcing; no numerical stencil in this answer.
    fx = -up-.1*e*(X2*Y1+X*Y3)
    fy = -vp+.1*e*(X3*Y+X1*Y2)
    return u, v, fx, fy


def load(path):
    data = json.loads(Path(path).read_text())
    cells, faces, entries = map(np.asarray, (data['cells'], data['faces'], data['entries']))
    nc, nf = len(cells), len(faces)
    nv = nc+nf
    K = sparse.coo_matrix((entries[:, 2], (entries[:, 0].astype(int), entries[:, 1].astype(int))), shape=(nv,nv)).tocsc()
    rows, cols, vals = [], [], []
    for fi, (lo,hi,axis,x,y,area) in enumerate(faces):
        for cell, sign in ((lo,1),(hi,-1)):
            if cell >= 0:
                rows.append(int(cell)); cols.append(int(axis)*nv+nc+fi); vals.append(sign*area)
    B = sparse.coo_matrix((vals,(rows,cols)),shape=(nc,2*nv)).tocsc()
    exterior = (faces[:,0]<0)|(faces[:,1]<0)
    outlet = (faces[:,2]==0)&(faces[:,1]<0)&(np.abs(faces[:,3]-4)<1e-10)
    fixed_faces = np.flatnonzero(exterior&~outlet)
    fixed = np.r_[nc+fixed_faces,nv+nc+fixed_faces]
    known = np.zeros(2*nv+nc)
    y = faces[fixed_faces,4]
    known[nc+fixed_faces] = 6*.02*(y/2)*(1-y/2)
    free = np.setdiff1d(np.arange(2*nv+nc),fixed)
    return cells, faces, K, B, known, free


def run(path, dt, transient):
    cells, faces, K, B, known, free = load(path)
    nc,nf = len(cells),len(faces); nv=nc+nf
    mass = sparse.diags(np.r_[cells[:,2],np.zeros(nf)],format='csc')
    factors = {}
    def system(coefficient):
        if coefficient not in factors:
            H = coefficient*mass+.1*K
            V = sparse.block_diag((H,H),format='csc')
            A = sparse.bmat([[V,-B.T],[-B,None]],format='csc')
            factors[coefficient] = (splu(A[free,:][:,free].tocsc()), A@known)
        return factors[coefficient]
    x,y,vol = cells.T
    u0,v0,_,_ = exact(x,y,0)
    state = known.copy()
    state[:nc] = u0 if transient else 6*.02*(y/2)*(1-y/2)
    state[nv:nv+nc] = v0 if transient else 0
    old = state.copy()
    steps = round(.4/dt) if transient else 1
    max_div = 0
    for step in range(steps):
        coefficient = (1 if step==0 else 1.5)/dt if transient else 0
        lu,lift = system(coefficient)
        rhs = np.zeros_like(state)
        if transient:
            _,_,fx,fy = exact(x,y,(step+1)*dt)
            temporal = state/dt if step==0 else (2*state-.5*old)/dt
            rhs[:nc] = vol*(temporal[:nc]+fx)
            rhs[nv:nv+nc] = vol*(temporal[nv:nv+nc]+fy)
        solution = known.copy()
        solution[free] = lu.solve((rhs-lift)[free])
        max_div = max(max_div,float(np.max(np.abs(B@solution[:2*nv])/vol)))
        old,state = state,solution
    u,v,_,_ = exact(x,y,.4)
    if not transient:
        u=6*.02*(y/2)*(1-y/2);v=np.zeros(nc)
    error = np.sqrt(np.sum(vol*((state[:nc]-u)**2+(state[nv:nv+nc]-v)**2))/8)
    pressure_error = np.sqrt(np.sum(vol*(state[2*nv:]-.006*(4-x))**2)/8)
    result = dict(cells=nc,dt=dt,transient=transient,velocity_l2=float(error),pressure_l2=float(pressure_error),max_divergence=max_div)
    native_path=Path(path).with_name(Path(path).name.replace('matrix-', 'native-mixed-'))
    if not transient and native_path.exists():
        native=json.loads(native_path.read_text())
        difference=max(float(np.max(np.abs(state[:nc]-native['u']))),
                       float(np.max(np.abs(state[nv:nv+nc]-native['v']))),
                       float(np.max(np.abs(state[2*nv:]-native['p']))))
        result['native_max_field_difference']=difference
        assert difference<2.4e-10
    print(json.dumps(result),flush=True)
    assert max_div<1e-9 and np.isfinite(error)
    velocity = np.r_[state[:nc]*np.sqrt(vol/8),state[nv:nv+nc]*np.sqrt(vol/8)]
    return result,velocity


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,required=True)
    args=parser.parse_args(); rows=[]
    for n in (8,16,32):
        row,_=run(args.root/f'matrix-{n}.json',.01,False);row['n']=n;rows.append(row)
    for key in ('velocity_l2','pressure_l2'):
        assert rows[0][key]/rows[1][key]>3.5 and rows[1][key]/rows[2][key]>3.5
    assert rows[2]['velocity_l2']<.01*.02 and rows[2]['pressure_l2']<.01*.024
    temporal=[]
    for dt in (.04,.02,.01,.005,.0025):
        row,field=run(args.root/'matrix-16.json',dt,True);rows.append(row);temporal.append(field)
    differences=[float(np.linalg.norm(a-b)) for a,b in zip(temporal,temporal[1:])]
    ratios=[a/b for a,b in zip(differences,differences[1:])]
    report=dict(schema='physics_sim_refined_coupling_probe_v1',rows=rows,time_differences=differences,time_ratios=ratios,runtime_backend=False)
    print(json.dumps(dict(time_differences=differences,time_ratios=ratios)),flush=True)
    (args.root/'mixed-coupling.json').write_text(json.dumps(report,indent=2)+'\n')
    assert all(r>3.5 for r in ratios)


if __name__=='__main__':
    main()
