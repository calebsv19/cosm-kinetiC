"""Directional independent-flow reference; no three-axis convergence claim."""
import json, math
from pathlib import Path
import numpy as np
from .matched_heat_report import digest
from .flow_time_report import pair
from .flow_spatial_report import load as cubic_load, validate as cubic_validate
from .flow_spatial_fields import pack, restrict_extensive, restrict_faces
from .isolated_qualification import PROBES
from .open_boundaries import face

SOURCE = '6db4841abd9c7418d56f100203d2727aca6ac8fdd8384feab7392dddb2172f30'

def validate(s):
    if (s['qualification_profile'] != 'flow_vertical_fixed_scalar_v1' or s['grid'] != [128,128,256]
        or s['dt_s'] != .005 or s['time_s'] != 3.2 or s['scalar_dt_s'] != .00125
        or s['flow_steps'] != 640 or s['scalar_cycles'] != 2560
        or s['scalar_work_cells'] != 2560*128*128*256
        or s['max_scalar_work_cells'] != 12000000000
        or s['scalar_transport_scheme'] != 'muscl_minmod'):
        raise ValueError('directional spatial/timing/work identity')
    if (not 0 <= s['maximum_divergence'] < 1e-8 or not 0 <= s['maximum_pressure_residual'] < 1e-11
        or not abs(s['heat_balance_j']) < 1e-9 or not abs(s['smoke_balance_kg']) < 1e-15
        or not 300 <= s['peak_temperature_k'] <= 330 or s['memory_limit_bytes'] != 4294967296
        or not 0 < s['memory_peak_bytes'] <= s['memory_limit_bytes']):
        raise ValueError('directional numerical/resource invariants')

def load(path, grid):
    x,y,z=grid;c=x*y*z;v=np.fromfile(path,dtype='f8')
    if len(v)!=5*c+x*y+x*z+y*z or not np.isfinite(v).all() or (v[:2*c]<0).any():
        raise ValueError('native finite nonnegative anisotropic fields')
    return dict(energy_j=v[:c],smoke_kg=v[c:2*c],face_velocity_m_s=v[2*c:])

def dense(v, grid):
    x,y,z=grid;c=x*y*z;p=x*y
    return [np.concatenate((v[:c].reshape(z,y,x),v[3*c+p:3*c+p+y*z].reshape(z,y,1)),axis=2),
            np.concatenate((v[c:2*c].reshape(z,y,x),v[3*c+p+y*z:].reshape(z,1,x)),axis=1),
            np.concatenate((v[2*c:3*c].reshape(z,y,x),v[3*c:3*c+p].reshape(1,y,x)),axis=0)]

def restrict_vertical(a, n):
    f=dense(a['face_velocity_m_s'],[n,n,2*n])
    return dict(energy_j=a['energy_j'].reshape(n,2,n,n).sum(axis=1).ravel(),
                smoke_kg=a['smoke_kg'].reshape(n,2,n,n).sum(axis=1).ravel(),
                face_velocity_m_s=pack([f[0].reshape(n,2,n,n+1).mean(axis=1),
                                       f[1].reshape(n,2,n+1,n).mean(axis=1),f[2][::2,:,:]]))

def winds(a, grid):
    # Values have already passed one whole-array finite check. Native MAC
    # sampling matches the retained shared observation convention exactly.
    v=a['face_velocity_m_s'];result={}
    import itertools
    for name,point in PROBES.items():
        out=[]
        for component in range(3):
            c=[p*n-(axis!=component)*.5 for axis,(p,n) in enumerate(zip(point,grid))]
            lo=[math.floor(p) for p in c];weights=[p-i for p,i in zip(c,lo)]
            values=[]
            for corner in itertools.product((0,1),repeat=3):
                ijk=[lo[i]+corner[i] for i in range(3)];sign=1
                for i in (0,1):ijk[i]=max(0,min(ijk[i],grid[i]-(i!=component)))
                k=ijk[2]
                if component==2:
                    if k<0:k=-k;sign=-1
                    if k>grid[2]:k=2*grid[2]-k
                else:
                    if k<0:k=-k-1;sign=-1
                    k=min(k,grid[2]-1)
                ijk[2]=k
                weight=math.prod(t if b else 1-t for t,b in zip(weights,corner))
                values.append(sign*v[face(grid,component,*ijk,True)]*weight)
            out.append(math.fsum(values))
        result[name]=out
    return result

def l2(a,b):
    delta=float(np.linalg.norm(np.asarray(a)-np.asarray(b)));scale=float(np.linalg.norm(b))
    return dict(absolute_l2=delta,fine_l2=scale,relative_l2=delta/scale if scale else None)

def report(root, prior):
    root,prior=Path(root),Path(prior)
    ident=json.loads((root/'run-identity.json').read_text())
    if (ident['grid'] != [128,128,256] or ident['flow_dt_s'] != .005 or ident['scalar_dt_s'] != .00125
        or ident['windows'] != 32 or ident['window_s'] != .1 or ident['source_sha256'] != SOURCE
        or digest(ident['source']) != SOURCE or digest(root/'vertical-flow-probe') != ident['probe_sha256']
        or json.loads((root/'run-terminal.json').read_text())['returncode'] != 0
        or 'memory_live_bytes=0 rejected_allocations=0' not in (root/'native.log').read_text()):
        raise ValueError('terminal source/probe/grid identity')
    fine=json.loads((root/'native.json').read_text());validate(fine)
    if digest(root/'fields.bin') != json.loads((root/'field-identity.json').read_text())['sha256']:
        raise ValueError('final field identity')
    a=load(root/'fields.bin',[128,128,256]);b=restrict_vertical(a,128)
    for key,limit in [('energy_j',1e-9),('smoke_kg',1e-15)]:
        if abs(math.fsum(a[key])-fine[key])>=limit or abs(math.fsum(a[key])-math.fsum(b[key]))>=limit:
            raise ValueError('final totals or conservative restriction')
    old=[];summaries=[]
    for n in (64,128):
        run=prior/f'grid-{n}';s=json.loads((run/'summary.json').read_text());cubic_validate(s,n)
        if (digest(run/'fields.bin') != s['fields_sha256'] or
            json.loads((run/'terminal.json').read_text())['returncode'] != 0):raise ValueError('accepted baseline identity')
        old.append(cubic_load(run/'fields.bin',n));summaries.append(s)
    if summaries[1]['native_input_sha256'] != SOURCE or summaries[1]['fields_sha256'] != ident['baseline_fields_sha256']:
        raise ValueError('original fine source/baseline identity')
    directional=pair(old[1],b,summaries[1],fine,n=128)
    directional.pop('temporal_screens_passed');directional.pop('flow_dt_s')
    native_winds=[winds(old[1],[128]*3),winds(a,[128,128,256])]
    norm=l2([v for name in PROBES for v in native_winds[0][name]], [v for name in PROBES for v in native_winds[1][name]])
    directional['wind_probes_m_s']=native_winds;directional['wind_probe_norm']=norm
    values=[directional['heat']['local_heat_relative_l1'],directional['heat']['height_profile_relative_l1'],
            directional['full_mac_velocity']['relative_l2'],norm['relative_l2'],directional['full_smoke_relative_l1'],directional['peak_rise_relative_difference']]
    restricted64={k:restrict_extensive(b[k],64) for k in ('energy_j','smoke_kg')}
    restricted64['face_velocity_m_s']=pack(restrict_faces(b['face_velocity_m_s'],64))
    coarse=pair(old[0],restricted64,summaries[0],fine,n=64)
    coarse.pop('temporal_screens_passed');coarse.pop('flow_dt_s')
    coarse.pop('wind_probes_m_s');coarse.pop('wind_probe_norm')
    return dict(schema='vertical_flow_reference_v1',directional_screens_passed=all(v is not None and math.isfinite(v) and v<=.1 for v in values),
                directional_heat_gate_passed=directional['heat']['local_heat_gate_passed'],
                heat_threshold_relative=.1,domain_m=[1,1,1],heat_region_m=[[.25,.75],[.25,.75],[0,.375]],
                identity=ident,runs=[summaries[1],fine],directional_comparison=directional,coarse64_to_directional_reference=coarse,
                limitations=['Independent evolving directional reference, not prescribed/prolonged velocity.',
                            'Horizontal resolution and original horizontal source shape remain fixed at 128.',
                            'Only vertical resolution is doubled; this does not establish full three-axis convergence or a formal order.',
                            'Fixed flow 5 ms and scalar 1.25 ms; no new directional-grid temporal qualification.',
                            'No physical calibration, surface response, burning objects or packaged capability claim.'])
