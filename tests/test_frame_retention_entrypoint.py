"""Legacy frame command must preserve evidence and bind its checkout, not caller cwd."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class FrameRetention(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.repo = self.root / 'checkout with spaces'
        self.repo.mkdir()
        (self.repo / 'scripts').mkdir()
        (self.repo / 'config').mkdir()
        shutil.copy2(ROOT / 'rm_frames', self.repo / 'rm_frames')
        for name in ('retention_audit.py', 'cfd_evidence.py', 'check_clean_root.py'):
            shutil.copy2(ROOT / 'scripts' / name, self.repo / 'scripts' / name)
        shutil.copy2(ROOT / 'config/artifact_retention_policy.json', self.repo / 'config/artifact_retention_policy.json')
        self.frames = self.repo / 'export/render_frames'
        self.frames.mkdir(parents=True)
        (self.frames / 'frame_000001.bmp').write_bytes(b'original frame evidence')
        (self.frames / 'notes.txt').write_text('retain me')
        self.caller = self.root / 'unrelated caller'
        (self.caller / 'export/render_frames').mkdir(parents=True)
        (self.caller / 'export/render_frames/frame_000001.bmp').write_bytes(b'caller evidence')

    def snapshot(self):
        return {str(p.relative_to(self.root)): (p.stat().st_ino, p.stat().st_mtime_ns, p.read_bytes())
                for p in self.root.rglob('*') if p.is_file() and not p.is_symlink()}

    def run_entry(self, *args):
        before = self.snapshot()
        result = subprocess.run(['bash', str(self.repo / 'rm_frames'), *args], cwd=self.caller,
                                capture_output=True, text=True, timeout=5)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse(any(self.repo.rglob('__pycache__')))
        return result

    def test_default_keeps_frames_and_selects_its_own_checkout(self):
        result = self.run_entry()
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertFalse(report['pruning_authorized'])
        self.assertEqual(report['mutations_performed'], [])
        row = report['inventory'][0]
        self.assertEqual(row['path'], str(self.frames))
        self.assertEqual(row['regular_files'], 2)
        self.assertFalse(row['eligible_for_pruning'])
        self.assertEqual(row['observed_classes'], ['unclassified'])

    def test_plan_preserves_even_retained_marked_frames(self):
        (self.frames / 'receipt.json').write_text(json.dumps({'artifact_class': 'retained_evidence'}))
        result = self.run_entry('--plan')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['inventory'][0]['observed_classes'], ['retained_evidence'])

    def test_apply_and_unknown_or_extra_arguments_refuse(self):
        for args in (('--apply',), ('--force',), ('--plan', 'other-root')):
            with self.subTest(args=args):
                result = self.run_entry(*args)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, '')

    def test_linked_frame_root_refuses(self):
        shutil.rmtree(self.frames)
        self.frames.symlink_to(self.caller / 'export/render_frames', target_is_directory=True)
        result = self.run_entry()
        self.assertEqual(result.returncode, 2)
        self.assertIn('symlink', result.stderr)

    def test_absent_frames_do_not_allocate_output(self):
        shutil.rmtree(self.frames)
        result = self.run_entry()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['inventory'][0]['status'], 'absent')
        self.assertFalse(self.frames.exists())

    def test_help_has_no_output_side_effects(self):
        result = self.run_entry('--help')
        self.assertEqual(result.returncode, 0)
        self.assertIn('Read-only', result.stdout)

if __name__ == '__main__':
    unittest.main()
