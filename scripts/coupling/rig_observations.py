"""Accepted rig source/transport metrics without inventing a common source pulse."""
import base64,hashlib,math
from numeric_digest_stream import digest
from .rig_geometry import probes
from surface_sources.growth_fire_v1 import require


def observation(cp,config,fire):
    fluid=cp['fluid'];d=fluid['state']['data'];budget=fluid['budgets']
    heat=math.fsum([math.fsum(d['energy_j']),budget['energy_j']['outflow'],-budget['energy_j']['inflow'],-d['initial_energy_j'],-d['input_energy_j']])
    smoke=math.fsum([math.fsum(d['smoke_kg']),budget['smoke_kg']['outflow'],-budget['smoke_kg']['inflow'],-d['initial_smoke_kg'],-d['input_smoke_kg']])
    kinetic=math.fsum([*d['kinetic_ledger_j'],d['open_domain_forcing_state']['external_work_j'],-d['kinetic_stored_j']])
    require(abs(heat)<1e-9 and abs(smoke)<1e-15 and abs(kinetic)<1e-9,'independent rig budgets')
    raw=base64.b64decode(cp['source']['checkpoint']['native_state_b64']);native=fire.state_decode(raw)
    n=config['fluid']['grid'][0];smoke_total=math.fsum(d['smoke_kg'])
    downstream=math.fsum(v for q,v in enumerate(d['smoke_kg']) if q%n>=5*n//8)
    centroid=[math.fsum(v*(((q//stride)%n+.5)/n) for q,v in enumerate(d['smoke_kg']))/smoke_total if smoke_total else None for stride in (1,n,n*n)]
    return dict(revision=cp['revision'],time_s=d['time_s'],gas_steps=d['steps'],fire_tick=native['tick'],checkpoint_digest=cp['digest'],
                physical_data_digest=digest(d),native_fire_sha256=hashlib.sha256(raw).hexdigest(),
                gas_energy_j=math.fsum(d['energy_j']),gas_smoke_kg=math.fsum(d['smoke_kg']),input_energy_j=d['input_energy_j'],input_smoke_kg=d['input_smoke_kg'],
                heat_defect_j=heat,smoke_defect_kg=smoke,kinetic_defect_j=kinetic,
                pressure_residual=fluid['fields']['projection_relative_residual'],divergence_s_inv=fluid['fields']['max_divergence_s_inv'],
                native_scalar_work_cells=d['scalar_work_cells'],downstream_smoke_kg=downstream,smoke_centroid_m=centroid,
                downstream_fraction_of_delivered_smoke=downstream/d['input_smoke_kg'] if d['input_smoke_kg'] else None,external_work_j=d['open_domain_forcing_state']['external_work_j'],**probes(config['fluid'],d))
