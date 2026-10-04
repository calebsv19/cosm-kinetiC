"""Read-only assessment of independent unforced periodic 3D physical tests."""
import json
import math
from pathlib import Path
import sys

def assess(path):
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    assert all(all(math.isfinite(v) for k,v in r.items() if k != 'kind') for r in rows)
    spatial = [r for r in rows if r['kind'] == 'spatial']
    temporal = [r for r in rows if r['kind'] == 'temporal']
    assert [r['n'] for r in spatial] == [8, 16, 32]
    assert [r['dt_s'] for r in temporal] == [.02, .01, .005]
    assert all(r['maximum_true_residual'] <= 1e-11 and
               r['maximum_divergence'] < 1e-8 and
               r['forcing_power_w'] == 0 and
               abs(r['advective_work_w']) < 1e-10 and
               r['peak_owned_bytes'] <= 256*1024**2 for r in rows)
    ratios = {}
    for key in ('velocity_relative_error', 'pressure_relative_error',
                'energy_relative_error', 'dissipation_relative_error'):
        ratios[key] = [a[key]/b[key] for a,b in zip(spatial,spatial[1:])]
        assert min(ratios[key]) >= 3
    temporal_ratios = [a['temporal_velocity_relative_error']/b['temporal_velocity_relative_error']
                       for a,b in zip(temporal,temporal[1:])]
    assert min(temporal_ratios) >= 3
    assert spatial[-1]['velocity_relative_error'] < .01
    assert spatial[-1]['pressure_relative_error'] < .05
    result = dict(status='passed_unforced_periodic_physical_refinement',
                  spatial_ratios=ratios, temporal_ratios=temporal_ratios,
                  finest=spatial[-1], wall_open_obstacle_certification=False,
                  scope='unforced ABC decay with raw continuous pressure, velocity, energy and strain diagnostics; temporal velocity uses independent discrete Fourier decay to separate space error')
    print(json.dumps(result,indent=2))
    return result

if __name__ == '__main__':
    assess(Path(sys.argv[1]))
