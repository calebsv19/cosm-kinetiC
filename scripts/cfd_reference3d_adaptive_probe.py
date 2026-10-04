#!/usr/bin/env python3
"""Test one equilibrium-score refinement with the unchanged reference solver."""
import argparse
import json
from pathlib import Path
from cfd_reference3d_adaptive_mesh import adaptive_mesh
from cfd_reference3d_spatial_probe import run


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--diagnostic',type=Path,required=True)
    ap.add_argument('--marks',type=int,choices=(8,16,32,64),default=32)
    ap.add_argument('--snapshot',type=Path);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();assert not a.output.exists()
    data,metadata=adaptive_mesh(a.diagnostic,a.marks)
    print(json.dumps({'phase':'adaptive_mesh','metadata':metadata}),flush=True)
    row=run(kind='factor',length=float(data[3][0][-1]),count=6,normal_spacing=.03,insert_normal=True,
            snapshot=a.snapshot,chunk_size=1024,matrix_free=True,mesh_data=data)
    row['adaptive_refinement']=metadata
    a.output.write_text(json.dumps(row,indent=2)+'\n')
    print(json.dumps({'numerically_accepted':row['numerically_accepted'],'iterations':row['iterations'],
        'final_residual':row['final_residual'],'tetrahedra':row['tetrahedra']}),flush=True)
    raise SystemExit(0 if row['numerically_accepted'] else 2)
