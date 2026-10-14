"""Rectangular native-grid comparisons on the unchanged source-region gate."""
import hashlib,json,math
from pathlib import Path
from fire_fluid_accuracy import dimensions,fields
from .convergence import comparison,restrict
from .surface_observations import observe,relative_l1
from .isolated_qualification import PROBES
from .velocity import sample_mac
from .solid_box import topology,blocked_faces

def read(directory,*,full_grid=False):
    directory=Path(directory);s=json.loads((directory/'summary.json').read_text())
    grid=dimensions(s['grid'],full_grid=full_grid);path=directory/'fields.bin'
    sha=hashlib.sha256(path.read_bytes()).hexdigest()
    if s['fields_sha256']!=sha:raise ValueError('field digest mismatch')
    if s['time_s']!=3.2 or s['dt_s']!=.005 or s.get('transport_scheme','muscl_minmod')!='muscl_minmod':raise ValueError('matched native run required')
    if abs(s['heat_balance_j'])>=1e-9 or abs(s['smoke_balance_kg'])>=1e-15 or s['divergence']>=1e-8 or s['pressure_residual']>=1e-11 or not 300<=s['peak_temperature_k']<=330:raise ValueError('native invariants')
    if full_grid and (s.get('qualification_profile')!='full_grid_128_v1' or s.get('scalar_transport_scheme')!='muscl_minmod' or s.get('memory_limit_bytes')!=2147483648 or not 0<s.get('memory_peak_bytes',0)<=2147483648 or not 0<s.get('scalar_work_cells',0)<=s.get('max_scalar_work_cells',0)==8000000000):raise ValueError('full-grid native profile/resources')
    f=fields(path,s['grid'],full_grid=full_grid)
    if any(not math.isfinite(v) for values in f.values() for v in values) or any(v<0 for key in ('energy_j','smoke_kg') for v in f[key]):raise ValueError('finite nonnegative fields')
    if s['box']:
        config={'grid':grid,'length_m':[1.]*3,'boundary_policy':{'vertical':'solid_bottom_open_top'},
                'solid_box':{'schema':'physics_sim_stationary_box/v1','lower_m':[.375]*3,'upper_m':[.625]*3,'velocity_boundary':'no_slip','thermal_boundary':'insulated'}}
        mask=topology(config)
        if any(solid and (f['energy_j'][q]!=0 or f['smoke_kg'][q]!=0) for q,solid in enumerate(mask)) or any(f['face_velocity_m_s'][q]!=0 for q in blocked_faces(config,mask)):
            raise ValueError('insulated solid exclusion')
    return s,f,grid

def region_difference(a,b,grid):
    nx,ny,nz=grid
    indices=[(k*ny+j)*nx+i for k in range(3*nz//8) for j in range(ny//4,3*ny//4) for i in range(nx//4,3*nx//4)]
    return relative_l1([a[q] for q in indices],[b[q] for q in indices])

def pair(coarse,fine,observation_grid=None,*,full_grid=False):
    sa,a,ga=read(coarse,full_grid=full_grid);sb,b,gb=read(fine,full_grid=full_grid)
    return compare_runs(sa,a,ga,sb,b,gb,observation_grid)

def compare_runs(sa,a,ga,sb,b,gb,observation_grid=None):
    """Compare already verified arrays without reopening large field files."""
    if sa['source_frames_sha256']!=sb['source_frames_sha256'] or sa['box']!=sb['box'] or sa['probe_sha256']!=sb['probe_sha256']:
        raise ValueError('matched source, geometry and executable required')
    restricted=restrict(b['energy_j'],gb,ga)
    target=ga if observation_grid is None else observation_grid
    common=[{key:restrict(f[key],g,target) for key in ('energy_j','smoke_kg')} for f,g in ((a,ga),(b,gb))]
    oa=observe(a['energy_j'],ga);ob=observe(b['energy_j'],gb)
    wind=[{name:sample_mac(f['face_velocity_m_s'],g,[1.]*3,point,solid_floor=True,lateral_open=True) for name,point in PROBES.items()} for f,g in ((a,ga),(b,gb))]
    flux_a=[v['diagnostic_robin_flux_w_m2'] for v in oa['faces'].values()]
    flux_b=[v['diagnostic_robin_flux_w_m2'] for v in ob['faces'].values()]
    return dict(grids=[ga,gb],full_fields=comparison(a,b,ga,gb),
        common_observation_grid=target,common_fields=comparison(common[0],common[1],target,target),
        common_source_region_heat_relative_l1=region_difference(common[0]['energy_j'],common[1]['energy_j'],target),
        source_region_heat_relative_l1=region_difference(a['energy_j'],restricted,ga),
        height_profile_relative_l1=relative_l1(oa['source_footprint_temperature_rise_profile_k'],ob['source_footprint_temperature_rise_profile_k']),
        diagnostic_flux_relative_l1=relative_l1(flux_a,flux_b),
        diagnostic_flux_max_absolute_difference_w_m2=max(abs(x-y) for x,y in zip(flux_a,flux_b)),
        wind_probes_m_s=wind,wind_probe_difference_l2_m_s=math.sqrt(math.fsum((x-y)**2 for name in PROBES for x,y in zip(wind[0][name],wind[1][name]))),
        actual_insulated_heat_exchange_j=0.,burning_faces_enabled=False)
