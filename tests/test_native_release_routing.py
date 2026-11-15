"""Exercise the real top-level Make routing without compiler or Apple effects."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SOURCE=Path(__file__).resolve().parents[1]

class NativeReleaseRouting(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);self.root=Path(temp.name)
        shutil.copyfile(SOURCE/'makefile',self.root/'Makefile')
        shutil.copytree(SOURCE/'make',self.root/'make')
        (self.root/'VERSION').write_text('0.6.0\n');(self.root/'WORKER_VERSION').write_text('0.5.0\n')
        subprocess.run(['git','init','-q',str(self.root)],check=True)
        self.bin=self.root/'fake-tools';self.bin.mkdir();self.trace=self.root/'trace'
        python=self.bin/'python3';python.write_text('''#!/bin/sh
case "$*" in
 *--check-inherited*) echo unowned; exit 0;;
esac
printf '%s\\n' "$*" >> "$TRACE"
case "$*" in
 *build_owner.py*|*release_app_stage.py*) exit 99;;
esac
exit 0
''');python.chmod(0o755)
        xcrun=self.bin/'xcrun';xcrun.write_text('#!/bin/sh\necho fake-notary >> "$TRACE"\nprintf "{}\\n"\n');xcrun.chmod(0o755)
        self.release=self.root/'output';self.release.mkdir()
        self.env={**os.environ,'PATH':str(self.bin)+':'+os.environ['PATH'],'TRACE':str(self.trace)}
        for key in list(self.env):
            if key.startswith('PHYSICS_SIM_'):self.env.pop(key)

    def run_make(self,target,*,native=True,skip=None):
        argv=['/usr/bin/make','-C',str(self.root)]
        if skip:argv+=['-o',skip]
        if native:argv+=['PHYSICS_SIM_LAYOUT_BUILD=fixture-owned-layout']
        argv += ['RELEASE_ROOT='+str(self.release),'APPLE_NOTARY_PROFILE=fixture-never-accessed',target]
        return subprocess.run(argv,env=self.env,capture_output=True,text=True)

    def test_notary_continuation_does_not_reenter_signer_or_build_owner(self):
        result=self.run_make('release-native-notarize',skip='release-native-signed')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        trace=self.trace.read_text()
        self.assertIn('release_native.py upload',trace);self.assertIn('fake-notary',trace)
        self.assertNotIn('build_owner.py',trace);self.assertNotIn('release_app_stage.py',trace)

    def test_final_package_does_not_reenter_build_owner(self):
        result=self.run_make('release-artifact',skip='release-staple')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('release_native.py package',self.trace.read_text())
        self.assertNotIn('build_owner.py',self.trace.read_text())

    def test_ordinary_build_retains_ownership(self):
        result=self.run_make('physics_sim_headless',native=False)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('build_owner.py --root',self.trace.read_text())

if __name__=='__main__':unittest.main()
