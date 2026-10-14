"""Native usable-workflow qualification; outputs are owned by each test."""
import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from coupling.workbench import Workbench, DEFAULTS, settings_valid


class WorkbenchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fire-fluid-workbench-')
        self.addCleanup(self.temp.cleanup); self.root = Path(self.temp.name)
        self.old = os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT')
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(self.root / 'evidence')
        self.addCleanup(self.restore)
        self.workers = tuple(os.environ[key] for key in ('FIRE_FLUID_GROWTH_ROOT', 'FIRE_FLUID_FIRE_WORKER', 'FIRE_FLUID_OPEN_WORKER', 'FIRE_FLUID_CLOCK_WORKER'))
        self.w = Workbench(self.root / 'panel', self.workers); self.addCleanup(self.w.close)
        self.settings = dict(DEFAULTS, duration_s=.3)

    def restore(self):
        if self.old is None: os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT', None)
        else: os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = self.old

    def test_author_step_retry_seek_reopen_export_and_preserve(self):
        initial = self.w.create(self.settings); run_id = initial['run_id']
        receipt = self.w.step(0, 'first')
        self.assertEqual(receipt, self.w.step(0, 'first'))
        with self.assertRaisesRegex(ValueError, 'stale'): self.w.step(0, 'different')
        for revision in (1, 2): self.w.step(revision, 'step-' + str(revision))
        state = self.w.state(); self.assertTrue(state['complete']); self.assertEqual(len(state['frames']), 4)
        for before, after in zip(state['frames'], state['frames'][1:]):
            self.assertEqual(after['feedback']['prior_checkpoint_digest'], before['checkpoint_digest'])
        reopened = Workbench(self.root / 'panel', self.workers); self.addCleanup(reopened.close)
        self.assertEqual(state, reopened.state()); self.assertFalse(reopened.playing)
        exported = reopened.export()
        self.assertEqual(len(exported['manifest']['frames']), 3)
        self.assertEqual(exported['manifest']['head_checkpoint_digest'], state['frames'][-1]['checkpoint_digest'])
        from combined_scene import validate
        for row in exported['manifest']['frames']:
            directory = Path(exported['output']) / Path(row['request']).parent
            self.assertEqual(validate(directory)['snapshot']['time_s'], row['time_s'])
            scene = json.loads((directory / 'playback_scene_runtime.json').read_text())
            self.assertEqual(len(scene['objects']), 65)
            self.assertFalse(any(obj['object_id'].startswith('fire_emitter_') for obj in scene['objects']))
        reopened.create(dict(self.settings, feedback=False))
        self.assertIn(run_id, reopened.state()['saved_runs'])
        reopened.resume(run_id); self.assertEqual(state, reopened.state() | {'saved_runs': state['saved_runs']})

    def test_reproducible_feedback_changes_native_state_and_controls(self):
        self.w.create(self.settings)
        for revision in range(3): self.w.step(revision, 'step-' + str(revision))
        enabled = self.w.state()['frames'][-1]
        self.w.create(self.settings)
        for revision in range(3): self.w.step(revision, 'step-' + str(revision))
        self.assertEqual(enabled, self.w.state()['frames'][-1])
        self.w.create(dict(self.settings, feedback=False))
        for revision in range(3): self.w.step(revision, 'step-' + str(revision))
        disabled = self.w.state()['frames'][-1]
        self.assertIsNone(disabled['feedback'])
        self.assertNotEqual(enabled['surface']['fields']['heat'], disabled['surface']['fields']['heat'])
        self.assertGreater(abs(enabled['metrics']['energy_j'] - disabled['metrics']['energy_j']), 1e-7)
        self.w.create(dict(self.settings, source_scale=2., sample_height_m=.125, exchange_s=.04, duration_s=.2))
        self.w.step(0, 'settings'); f = self.w.state()['frames'][-1]
        self.assertEqual(f['feedback']['sample_height_m'], .125)
        self.assertEqual(f['feedback']['window_ticks'], [0, 8])

    def test_pause_play_completion_and_invalid_authoring(self):
        for change in ({'exchange_s': .05}, {'duration_s': 2., 'exchange_s': .02}, {'sample_height_m': .5}, {'source_scale': float('nan')}):
            with self.assertRaises(ValueError): settings_valid(dict(self.settings, **change))
        self.w.create(self.settings)
        thread = threading.Thread(target=self.w.run_loop); thread.start()
        self.addCleanup(lambda: (self.w.close(), thread.join(65)))
        self.w.play(); self.w.pause()
        revision = self.w.state()['revision']; time.sleep(.25)
        self.assertEqual(self.w.state()['revision'], revision)
        self.w.play()
        deadline = time.monotonic() + 30
        while self.w.playing and time.monotonic() < deadline: time.sleep(.1)
        self.assertTrue(self.w.state()['complete']); self.assertIsNone(self.w.error)
        with self.assertRaisesRegex(ValueError, 'completed'): self.w.play()

    def test_warm_plume_profile_preserves_baseline_and_exposes_native_height(self):
        self.w.create(self.settings)
        baseline = self.w.host.config
        self.assertEqual(baseline['fluid']['grid'], [8, 8, 4])
        self.w.create(dict(self.settings, profile='warm_plume', wind_scale=1., source_scale=3.))
        self.assertEqual(self.w.host.config['fluid']['grid'], [8, 8, 8])
        self.assertEqual(self.w.host.config['source']['calibration']['energy_transfer_fraction'], .75)
        self.w.step(0, 'plume')
        state = self.w.state(); f = state['frames'][-1]['fluid']
        self.assertEqual(len(f['smoke_side_kg_m3']), 64)
        self.assertGreater(f['smoke_above_first_layer_fraction'], 0)
        self.assertLess(f['smoke_above_first_layer_fraction'], 1)
        self.assertTrue(state['frames'][-1]['feedback']['applied_to_fire'])
        with self.assertRaises(ValueError): settings_valid(dict(self.settings, profile='unknown'))

    def test_failed_authoring_and_invalid_resume_preserve_current_scene(self):
        before = self.w.create(self.settings)
        with patch.object(self.w, '_native_command', side_effect=ValueError('native refusal')):
            with self.assertRaisesRegex(ValueError, 'native refusal'): self.w.create(self.settings)
        self.assertEqual(before, self.w.state())
        with self.assertRaises(ValueError): self.w.resume('invalid-run')
        self.assertEqual(before, self.w.state())


if __name__ == '__main__': unittest.main()
