#!/usr/bin/env python3
"""Current boxed forcing and reciprocal fixed-support thermal proof."""
import argparse
import json
from pathlib import Path
import subprocess
import numpy as np
from coupling.boxed_face_forcing import report,sha,DEPTH,AREA,RHO_CP
from fire_fluid_heated_face_qualification import exact_response


def qualify(root,probe,output):
    result=report(root)
    thermal={}
    for label,n in [('uniform32',32),('uniform64',64),('box32',32),('box64',64)]:
        arg='uniform' if label.startswith('uniform') else str(root/f'box-{n}/fields.bin')
        rows=[]
        for dt in (.1,.05,.025):
            run=subprocess.run([str(probe),str(n),arg,str(dt),str(DEPTH)],
                check=True,capture_output=True,text=True,timeout=90)
            d=json.loads(run.stdout)
            expected=exact_response(d['initial_gas_k'],n,depth_m=DEPTH)
            surface_error=abs(d['surface_k'][-1]-float(expected[-1]))
            gas_error=float(np.max(np.abs(np.asarray(d['final_gas_k'])-expected[:-1])))
            ledger=abs(d['stored_change_j']-d['gas_to_surface_j'])
            # For common capacity/conductance ratios the mean gas and face obey
            # a two-capacity closed form even with nonuniform initial gas.
            cg=RHO_CP*AREA*DEPTH;cs=100;g=10*AREA
            exact_gain=cs*cg*d['initial_mean_rise_k']/(cg+cs)*(-np.expm1(-g*(1/cg+1/cs)*3.2))
            if (surface_error>1e-6 or gas_error>1e-5 or abs(d['energy_residual_j'])>1e-9
                or ledger>1e-10*max(abs(d['gas_to_surface_j']),1e-10)
                or abs(d['stored_change_j']-exact_gain)>1e-5*max(abs(exact_gain),1e-12)):
                raise ValueError('reciprocal fixed-support response/ledger gate failed')
            (output/f'{label}-dt{dt}.json').write_text(json.dumps(d,indent=2)+'\n')
            rows.append(dict(dt_s=dt,gas_to_surface_j=d['gas_to_surface_j'],
                exact_gain_j=float(exact_gain),surface_error_k=surface_error,gas_error_k=gas_error,
                ledger_error_j=ledger,gas_balance_error_j=d['energy_residual_j'],
                initial_mean_rise_k=d['initial_mean_rise_k'],final_surface_k=d['surface_k'][-1]))
        thermal[label]=rows
    uniform_delta=abs(thermal['uniform32'][-1]['gas_to_surface_j']-thermal['uniform64'][-1]['gas_to_surface_j'])
    if uniform_delta>1e-6:raise ValueError('matched uniform support grid invariance failed')
    result['reciprocal_frozen_response']=thermal
    result['uniform_grid_transfer_difference_j']=uniform_delta
    result['sampling_transfer_contract_qualified']=True
    result['thermal_probe_sha256']=sha(probe)
    (output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--thermal-probe',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    result=qualify(args.root.resolve(),args.thermal_probe.resolve(),args.output)
    print(json.dumps({k:v for k,v in result.items() if k not in ('responses','reciprocal_frozen_response')},indent=2))
