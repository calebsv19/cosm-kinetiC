"""Native inert-box integration and independently rejected incompatible state."""
import copy
import os
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from coupling.box_qualification import configuration, checks
from coupling.framework import Framework
from coupling.solid_box import topology, blocked_faces, point_outside
from coupling.playback_export import box_surfaces
from open_atmosphere import validate, run, SCHEMA


class BoxTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fire-fluid-box-test-'); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name); old = os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT')
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(self.root / 'evidence')
        def restore():
            if old is None: os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT', None)
            else: os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = old
        self.addCleanup(restore)
        self.workers = tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT', 'FIRE_FLUID_FIRE_WORKER', 'FIRE_FLUID_OPEN_WORKER', 'FIRE_FLUID_CLOCK_WORKER'))
        self.config = configuration(self.workers, duration=.3)

    def test_native_solid_exclusion_feedback_and_immutable_replay(self):
        host = Framework(self.config, self.root / 'store'); host.initialize()
        for revision in range(3): host.step(revision, 'step-' + str(revision))
        cp = host.inspect(); result = checks(self.config, cp['fluid'])
        self.assertEqual(result['solid_cells'], 8)
        self.assertGreater(result['blocked_faces'], 0)
        self.assertEqual(result['max_blocked_normal_velocity_m_s'], 0)
        self.assertEqual(cp['fluid']['model'], 'xy_periodic_ground_box_open_top_projection3d_v1')
        self.assertTrue(cp['evidence']['held_return']['applied_to_fire'])
        from coupling.playback_export import export_playback
        from combined_scene import HEADER,validate as validate_scene
        exported = export_playback(host,self.root/'export',revisions=[3])
        directory = Path(exported['output'])/'revision-003'
        validate_scene(directory)
        scene = __import__('json').loads((directory/'playback_scene_runtime.json').read_text())
        self.assertEqual(len(scene['objects']),71)
        self.assertEqual((directory/'frame_000000.vf3d').read_bytes()[-512:],bytes(topology(self.config['fluid'])))
        before = host.path.read_bytes(); host.step(2, 'step-2'); self.assertEqual(before, host.path.read_bytes())

    def test_alignment_initial_flow_sources_and_probe_admission(self):
        request = dict(self.config['fluid'], schema=SCHEMA, state=None, steps=[])
        validate(request)
        mask = topology(request); q = mask.index(1)
        changed = copy.deepcopy(request); changed['solid_box']['lower_m'][0] += .01
        with self.assertRaisesRegex(ValueError, 'align'): validate(changed)
        changed = copy.deepcopy(request); changed['initial_energy_j'][q] = 1
        with self.assertRaisesRegex(ValueError, 'solid'): validate(changed)
        changed = copy.deepcopy(request); changed['initial_face_velocity_m_s'][blocked_faces(request, mask)[0]] = .1
        with self.assertRaisesRegex(ValueError, 'solid'): validate(changed)
        changed = copy.deepcopy(request); changed['steps'] = [{'energy_j': [float(v) for v in mask], 'smoke_kg': [0.] * len(mask)}]
        with self.assertRaisesRegex(ValueError, 'source intersects'): run(changed, self.workers[2])
        with self.assertRaisesRegex(ValueError, 'inside'): point_outside(request, [.25, .25, .5])

    def test_render_faces_match_native_box_extent(self):
        from coupling.framework import load_fire
        load_fire(self.workers[0])
        from fire_render_diagnostic import plane
        objects = box_surfaces(self.config, plane)
        self.assertEqual(len(objects), 6)
        # Each outward face lies on one exact physical box plane.
        box = self.config['fluid']['solid_box']
        for obj in objects:
            frame = obj['primitive']['frame']; normal = list(frame['normal'].values())
            axis = next(a for a, value in enumerate(normal) if value)
            position = list(frame['origin'].values())
            self.assertEqual(position[axis], box['lower_m' if normal[axis] < 0 else 'upper_m'][axis])
            self.assertEqual(obj['primitive']['width'], .25)
            self.assertEqual(obj['primitive']['height'], .25)


if __name__ == '__main__': unittest.main()
