import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts/agent_session'))
from service import Service, SessionError, TERMINAL
from protocol import call


def wait(service, run, terminal=False):
    end = time.monotonic()+90
    while time.monotonic() < end:
        row = service.run_inspect(run, history=True)
        if (row['state'] in TERMINAL if terminal else row['state'] != 'starting'):
            if row['state'] in TERMINAL and run in service.children:
                service.children[run].wait(timeout=10)
            return row
        time.sleep(.02)
    raise AssertionError('worker did not reach expected state')


def regions(n, depth=7):
    result = [{'bounds_m': [1, .5, 3, 1.5], 'level': 1}]
    band = 4*(2/n/8)
    for b in ([1.5-band, .75-band, 2.5+band, .75+band],
              [1.5-band, 1.25-band, 2.5+band, 1.25+band],
              [1.5-band, .75-band, 1.5+band, 1.25+band],
              [2.5-band, .75-band, 2.5+band, 1.25+band]):
        result.append({'bounds_m': b, 'level': 3})
    for level in range(4, depth+1):
        radius = 8*(2/n)/2**level
        for x in (1.5, 2.5):
            for y in (.75, 1.25):
                result.append({'bounds_m': [x-radius, y-radius, x+radius, y+radius], 'level': level})
    return result


class RefinedTests(unittest.TestCase):
    def test_background_transient_controls_sampling_and_artifact(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            scene = call(s, 'scene_create', {'scene_id': 'channel', 'template': 'cfd_refined_channel_2d'})
            options = dict(scene_id='channel', scene_revision=scene['scene_revision'], grid=[16, 8, 1],
                           dt=.005, fluid={'density_kg_m3': 1, 'dynamic_viscosity_pa_s': .1},
                           numerical_memory_limit_mib=32)
            self.assertTrue(call(s, 'scene_validate', options)['valid'])
            self.assertEqual(s.request(**options)[0]['channel']['refinement_regions'], [])
            call(s, 'run_start', dict(options, request_id='transient', steps=4))
            row = wait(s, 'transient'); self.assertEqual(row['state'], 'paused')
            self.assertEqual(row['model'], 'incompressible_refined2d_v1')
            self.assertEqual(row['health']['projection_status'], 'not_solved')
            try:
                call(s, 'run_control', dict(run_id='transient', command_id='step', action='step', scene_revision=scene['scene_revision'], wait_ms=5000))
                row = s.run_inspect('transient', history=True)
                self.assertEqual(row['tick'], 1); self.assertAlmostEqual(row['simulation_time'], .005)
                sample = s.run_sample('transient', 'sample', field='pressure_pa', resolution=8,
                                      points=[[0, .5, .5], [1, .5, .5]], wait_ms=5000)['preview']
                self.assertEqual(len(sample['samples']), 64); self.assertEqual(len(sample['probes']), 2)
                self.assertIsNotNone(sample['probes'][0]['values'][9])
                self.assertEqual(s.run_inspect('transient')['tick'], 1)
                self.assertEqual(s.run_assess('transient')['numerical_status'], 'passed')
                self.assertEqual(s.run_assess('transient')['reference_accuracy']['status'], 'not_applicable')
                s.run_control('transient', 'go', 'continue', scene['scene_revision'], 5000)
                row = wait(s, 'transient', True); self.assertEqual(row['state'], 'completed', row)
                artifact = next(a for a in s.run_result('transient')['artifacts'] if a['path'].endswith('channel_fields.json'))
                data = Path(artifact['path']).read_bytes(); self.assertEqual(hashlib.sha256(data).hexdigest(), artifact['sha256'])
                fields = json.loads(data); self.assertEqual(fields['schema'], 'physics_sim_refined2d_fields_v1')
                self.assertEqual(len(fields['refined_fields']['leaves']), row['health']['fluid_leaf_cells'])
                self.assertGreater(row['health']['numerical_memory']['peak_bytes'], 0)
                self.assertEqual(sum(row['health']['fluid_cells_per_level']), row['health']['fluid_leaf_cells'])
                self.assertEqual(row['health']['fluid_cells_per_level'][0], 128)
                self.assertGreater(row['health']['phase_cost']['mixed_solve_cpu_ms'], 0)
                self.assertGreater(row['runtime_cost']['final_export_wall_ms'], 0)
                self.assertGreater(row['runtime_cost']['previous_publication_wall_ms'], 0)
                self.assertFalse(row['qualification']['physical_accuracy_certified'])
            finally:
                if s.run_inspect('transient')['state'] not in TERMINAL:
                    s.run_control('transient', 'cleanup', 'cancel', scene['scene_revision'], 5000)

    def test_budget_failure_and_input_boundaries(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            scene = s.scene_create('body', 'cfd_refined_obstacle_2d', channel={'solve_mode': 'steady_stokes'})
            options = dict(scene_id='body', scene_revision=scene['scene_revision'], grid=[128,64,1],
                           steps=1, dt=.005, fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1}, numerical_memory_limit_mib=1)
            s.run_start('limited', **options)
            row = wait(s, 'limited', True)
            self.assertEqual(row['state'], 'failed'); self.assertEqual(row['error'], 'numerical_memory_budget_exceeded')
            self.assertGreater(row['health']['numerical_memory']['rejected_allocations'], 0)
            self.assertEqual(s.run_assess('limited')['numerical_status'], 'failed')
            with self.assertRaises(SessionError): s.request(**dict(options, steps=2))
            with self.assertRaises(SessionError): s.scene_create('bad', 'cfd_refined_channel_2d', channel={'refinement_regions':[{'bounds_m':[0,0,1,1], 'level':True}]})

    def test_refinement_comparison_and_assessment(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            scene = s.scene_create('compare', 'cfd_refined_obstacle_2d',
                                   channel={'solve_mode': 'steady_stokes'})
            options = dict(scene_id='compare', scene_revision=scene['scene_revision'],
                           steps=1, dt=.005, start_paused=False,
                           fluid={'density_kg_m3':1, 'dynamic_viscosity_pa_s':.1})
            for name,grid in [('coarse',[16,8,1]), ('fine',[32,16,1])]:
                s.run_start(name, grid=grid, **options)
                self.assertEqual(wait(s, name, True)['state'], 'completed')
            report = s.run_compare(['coarse','fine'])
            changes = report['comparisons'][0]['changes']
            self.assertGreater(changes['pressure_force_n_x']['absolute_difference'], 0)
            self.assertIn('viscous_force_n_x', changes)
            self.assertIn('physical_strain_dissipation_w', changes)
            assessment = s.run_assess('fine', spatial_coarse_run='coarse')
            self.assertEqual(assessment['refinement_comparisons']['spatial'], report)
            self.assertFalse(assessment['physical_accuracy_certified'])
            self.assertEqual(assessment['reference_accuracy']['status'], 'failed')
            with self.assertRaises(SessionError): s.run_compare(['coarse','fine'], 'temporal')
            with self.assertRaises(SessionError): s.run_compare(['fine','coarse'])

    def test_transient_time_comparison(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            scene = s.scene_create('time', 'cfd_refined_channel_2d')
            options = dict(scene_id='time', scene_revision=scene['scene_revision'],
                           grid=[16,8,1], start_paused=False,
                           fluid={'density_kg_m3':1, 'dynamic_viscosity_pa_s':.1})
            for name,dt,steps in [('coarse',.01,2), ('fine',.005,4), ('unmatched',.005,3)]:
                s.run_start(name, dt=dt, steps=steps, **options)
                self.assertEqual(wait(s, name, True)['state'], 'completed')
            report = s.run_compare(['coarse','fine'], 'temporal')
            self.assertIn('kinetic_energy_j', report['comparisons'][0]['changes'])
            self.assertEqual(s.run_assess('fine', temporal_coarse_run='coarse')
                             ['refinement_comparisons']['temporal'], report)
            with self.assertRaises(SessionError): s.run_compare(['coarse','unmatched'], 'temporal')

    def test_qualified_stationary_case_through_agent(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            scene = call(s, 'scene_create', dict(scene_id='reference', template='cfd_refined_obstacle_2d',
                        channel={'solve_mode':'steady_stokes', 'refinement_regions':regions(64)}))
            options = dict(scene_id='reference', scene_revision=scene['scene_revision'], grid=[128,64,1],
                           steps=1, dt=.005, fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},
                           numerical_memory_limit_mib=192, start_paused=False)
            started = time.monotonic(); s.run_start('reference', **options)
            self.assertLess(time.monotonic()-started, 5)
            row = wait(s, 'reference', True)
            self.assertEqual(row['state'], 'completed', row)
            self.assertEqual(row['tick'], 1); self.assertEqual(row['simulation_time'], 0)
            self.assertTrue(row['qualification']['fixed_case_reference_gate']['passed'], row)
            self.assertLessEqual(row['health']['numerical_memory']['peak_bytes'], 192*1024*1024)
            self.assertEqual(s.run_assess('reference')['steady_state'], 'stationary_equations_solved')
            self.assertEqual(s.run_assess('reference')['reference_accuracy']['status'], 'passed')
            self.assertEqual(s.run_start('reference', **options)['state'], 'completed')
            result = s.run_result('reference')
            self.assertEqual(result['provenance']['model'], 'incompressible_refined2d_v1')


if __name__ == '__main__': unittest.main()
