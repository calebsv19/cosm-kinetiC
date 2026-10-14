#!/usr/bin/env python3
"""Numerical hot-gas foundation and independent manufactured surface checks.

This does not qualify a reacting open-domain plume or reciprocal heat feedback.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import subprocess
from coupling import hotgas
from coupling.methanol_boundary import gauge_height_constraint
from coupling.prescribed_pool import packet
from surface_sources.growth_fire_v1 import sealed

ROOT = Path(__file__).resolve().parents[1]


def qualification(worker, reference):
    spec = importlib.util.spec_from_file_location('hotgas_cases', ROOT/'tests/test_hotgas.py')
    cases = importlib.util.module_from_spec(spec); spec.loader.exec_module(cases)
    rows = cases.convergence()
    errors = [r['density_l1_kg_m3'] for r in rows]
    if not errors[2] < errors[1] < errors[0] or errors[1]/errors[2] <= 1.8:
        raise ValueError('contact refinement failed')
    config = dict(grid=[8, 8, 8], length_m=[.6, .6, .6], gamma=1.4, gas_constant_j_kg_k=287.)
    source_packet = packet(reference, config['grid'], config['length_m'])
    conserved = cases.cell(101325/(287*300))*512
    rate = (1, 100000); dt = float(hotgas.rate(rate)); steps = 1000
    source = [0.]*len(conserved)
    volume = math.prod(config['length_m'])/512
    for q, area in enumerate(source_packet['source_cell_areas_m2']):
        source[5*q+4] = source_packet['complete_combustion_power_w']*dt*area/(source_packet['source_disk_area_m2']*volume)
    initial = hotgas.initial(config, worker, conserved, source, rate)
    final, receipt = hotgas.advance(config, worker, initial, source, steps, rate)
    mid, _ = hotgas.advance(config, worker, initial, source, 333, rate)
    restarted, _ = hotgas.advance(config, worker, json.loads(json.dumps(mid)), source, steps, rate)
    if restarted != final: raise ValueError('prescribed power restart failed')
    injected = source_packet['complete_combustion_power_w']*steps*dt
    if abs(receipt['integrated_source'][4]-injected) > 1e-10*injected:
        raise ValueError('disk supported measured-power budget failed')
    if max(abs(v) for v in final['conserved'][1::5]) <= 0:
        raise ValueError('localized heat did not produce gas momentum')
    assessment = sealed(dict(schema='hotgas_foundation_qualification/v1',
        numerical_foundation_qualified=True, physical_pool_fire_qualified=False,
        model='periodic calorically perfect ideal-gas Euler, first-order Rusanov',
        source_packet_digest=source_packet['digest'], prescribed_heat_probe_power_w=source_packet['complete_combustion_power_w'],
        prescribed_heat_probe_duration_s=steps*dt, prescribed_heat_probe_config=config,
        prescribed_heat_probe_integrated_energy_j=injected, exact_prescribed_probe_restart=True,
        probe_receipt=receipt, contact_comparison=rows,
        contact_refinement_ratios=[errors[0]/errors[1], errors[1]/errors[2]],
        hot_cold_isobaric_density_ratio=300/1290, closed_volume_heating_target_k=1290,
        full_fuel_admission_qualified=False, reciprocal_thermal_exchange_enabled=False,
        independent_surface_implementation_checks='analytic Newton cooling; RK4 continuum radiation; equilibrium; signed reservoir transfer',
        experimental_surface_transfer_identified=False,
        missing_physics=['Open reservoir and solid burner boundaries', 'Gravity/buoyant plume',
            'Temperature-dependent mixture properties', 'Viscosity and conduction',
            'Fuel/species and combustion', 'Gas radiation and surface heat flux',
            'Matched measurement planes and steady experimental statistics'],
        low_mach_accuracy_limit='First-order acoustic Rusanov is diffusive; refinement proof does not establish efficient low-Mach flame resolution.'))
    return {'assessment': assessment, 'prescribed-source': source_packet,
            'heat-probe-initial': initial, 'heat-probe-final': final,
            'heat-probe-source': sealed(dict(config=config, tick_seconds=list(rate), integrated_density_per_substep=source)),
            'surface-plane-constraint': gauge_height_constraint()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('worker', 'reference', 'growth-root', 'thermal-worker', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    os.environ['HOTGAS_WORKER'] = str(args.worker.resolve())
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', GROWTH_THERMAL_WORKER=str(args.thermal_worker.resolve()))
    commands = [[os.sys.executable, str(ROOT/'tests/test_hotgas.py'), '-v'],
                [os.sys.executable, str(args.growth_root/'tests/test_liquid_boundary_independent.py'), '-v']]
    for name, command in zip(('gas-tests', 'surface-tests'), commands):
        result = subprocess.run(command, text=True, capture_output=True, env=env, timeout=60)
        (output/(name+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode: raise RuntimeError(name+' failed; see retained log')
    controls = [ROOT/p for p in ('scripts/coupling/hotgas.py', 'scripts/coupling/methanol_boundary.py',
        'scripts/coupling/prescribed_pool.py', 'scripts/fire_hotgas_qualification.py', 'tests/test_hotgas.py')]
    controls += [args.growth_root/p for p in ('tests/test_liquid_boundary_independent.py', 'scripts/fire_source/thermal.py')]
    controls += [args.worker, args.thermal_worker]
    pins = {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in controls}
    for name, value in qualification(args.worker, args.reference).items():
        (output/(name+'.json')).write_text(json.dumps(value, indent=2)+'\n')
    if pins != {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in controls}:
        raise RuntimeError('qualification control drift')
    (output/'controls.json').write_text(json.dumps(sealed(dict(sha256=pins)), indent=2)+'\n')
    print(output/'assessment.json')
