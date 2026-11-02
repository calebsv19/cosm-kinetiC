#!/usr/bin/env python3
"""Independent linear-system oracle for a non-burning frozen-plume face.

Reads hash-bound retained boxed-plume fields. Never resumes or rewrites them.
The gas transport is frozen during this separate 3.2 s thermal experiment.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def exact_response(initial_gas, n, duration=3.2, depth_m=None):
    """Symmetric capacity-weighted matrix exponential, independent of splitting."""
    gas=np.asarray(initial_gas, dtype=float)
    capacities=np.r_[np.full(gas.size, 1206/n**3), 100.]
    layers=1 if depth_m is None else depth_m*n
    if not np.isfinite(layers) or layers<1 or abs(layers-round(layers))>1e-10:
        raise ValueError('grid-aligned fixed physical depth required')
    k=10/n**2/layers
    matrix=np.zeros((gas.size+1, gas.size+1))
    matrix[np.arange(gas.size), np.arange(gas.size)]=-k/capacities[:-1]
    matrix[-1,-1]=-gas.size*k/capacities[-1]
    matrix[:-1,-1]=matrix[-1,:-1]=k/np.sqrt(capacities[:-1]*capacities[-1])
    eigenvalues,basis=np.linalg.eigh(matrix)
    initial=np.r_[gas-300., 0.]*np.sqrt(capacities)
    final=basis@(np.exp(eigenvalues*duration)*(basis.T@initial))
    return 300.+final/np.sqrt(capacities)


def report(probe, accuracy, surface_evidence, output):
    # The earlier independent sampler bound these exact field bytes.
    old=json.loads(surface_evidence.read_text())
    bound={str(Path(row['path']).resolve()):row['sha256'] for row in old['inputs']}
    cases={};inputs=[]
    for label,n in [('prescribed',32),('32',32),('64',64)]:
        field=accuracy/f'box-{n}-csr/fields.bin'
        sha=digest(field)
        if bound.get(str(field.resolve()))!=sha:
            raise ValueError('retained boxed field is not the previously verified input')
        metadata=json.loads((field.parent/'summary.json').read_text())
        if metadata['grid']!=n or metadata['time_s']!=3.2 or not metadata['box']:
            raise ValueError('boxed plume identity mismatch')
        if label!='prescribed':inputs.append(dict(path=str(field.resolve()),sha256=sha))
        rows=[]
        for dt in (.1,.05,.025):
            result=subprocess.run([str(probe),str(n),'uniform' if label=='prescribed' else str(field),str(dt)],check=True,
                                  text=True,capture_output=True,timeout=60)
            row=json.loads(result.stdout)
            (output/f'box-{label}-dt{dt}.json').write_text(json.dumps(row,indent=2)+'\n')
            oracle=exact_response(row['initial_gas_k'],n)
            row['surface_error_k']=abs(row['surface_k'][-1]-float(oracle[-1]))
            row['gas_error_max_k']=float(np.max(np.abs(row['final_gas_k']-oracle[:-1])))
            row['stored_ledger_error_j']=abs(row['stored_change_j']-row['gas_to_surface_j'])
            if (row['surface_error_k']>1e-6 or row['gas_error_max_k']>1e-5 or
                abs(row['energy_residual_j'])>1e-9 or
                row['stored_ledger_error_j']>1e-10*max(abs(row['gas_to_surface_j']),1e-10)):
                raise ValueError('independent response or reciprocal energy gate failed')
            rows.append({k:row[k] for k in ('dt_s','surface_error_k','gas_error_max_k',
                'stored_ledger_error_j','energy_residual_j','gas_to_surface_j','stored_change_j')}
                | dict(final_surface_k=row['surface_k'][-1],
                       initial_mean_gas_k=float(np.mean(row['initial_gas_k']))))
        cases[label]=rows
    transfer=[cases[str(n)][-1]['gas_to_surface_j'] for n in (32,64)]
    relative=abs(transfer[0]-transfer[1])/abs(transfer[1]) if transfer[1] else None
    summary=dict(schema='fire_fluid_heated_face_qualification/v1',inputs=inputs,
        probe_sha256=digest(probe),cases=cases,heat_threshold_relative=.1,
        transfer_grid_sensitivity_relative=relative,
        # Adjacent-cell bath thickness changes with mesh; this is a diagnostic,
        # not a matched-volume convergence experiment or a physical error norm.
        matched_spatial_convergence_qualified=False,
        numerical_exchange_qualified=True,live_plume_exchange_qualified=False,
        physical_surface_flux_qualified=False,burning_enabled=False,
        limits=['Synthetic constant h=10 W/m2/K, whole-face C=100 J/K.',
                'One lower face of a 0.25 m inert box; other faces insulated.',
                'Retained plume is initial gas data; transport frozen thereafter.',
                'Adjacent gas layer thickness is one cell and changes with grid.',
                'No-box directional passes do not qualify boxed surface forcing.',
                'Live coupling needs matched physical sampling/transfer support and box-face accuracy.'])
    (output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    return summary


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--accuracy-root',type=Path,required=True)
    parser.add_argument('--surface-evidence',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    summary=report(args.probe.resolve(),args.accuracy_root.resolve(),
                   args.surface_evidence.resolve(),args.output)
    print(json.dumps(summary,indent=2))
