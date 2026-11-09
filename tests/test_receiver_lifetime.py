"""Actual receiver/unchanged Thermal adapter, contained normal and sanitizer proof."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
WORKSPACE=ROOT.parent.parent
class ReceiverLifetime(unittest.TestCase):
 def test_process_lifetimes(self):
  thermal=WORKSPACE/'thermal_sim';shared=WORKSPACE/'shared'
  includes=[ROOT/'include',ROOT/'src/tools',thermal/'include',thermal/'src/coupling',shared/'core/core_sim/include',shared/'core/core_thermal/include']
  sources=[thermal/'src/experiment/participant.c',thermal/'src/coupling/inert.c',thermal/'src/coupling/receiver.c']
  sources += [shared/f'core/core_thermal/src/{name}.c' for name in ('core_thermal','core_thermal_rod','core_thermal_material','core_thermal_material_rod')]
  sources += [shared/f'core/core_sim/src/{name}.c' for name in ('core_sim_time','core_sim_schedule','core_sim_exchange')]
  flags=['/usr/bin/clang','-std=c11','-D_DARWIN_C_SOURCE=','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-Wl,-dead_strip',*['-I'+str(p) for p in includes]]
  flags += [f'-D{name}="'+('0'*64)+'"' for name in ('COUPLED_FIXTURE_ID','COUPLED_SOURCE_ID','COUPLED_SUPPORT_ID')]
  if os.environ.get('PHYSICS_L3_SANITIZE'):flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  with tempfile.TemporaryDirectory() as temp:
   base=Path(temp);worker=base/'worker';probe=base/'probe'
   for output,inputs in [(worker,[ROOT/'src/tools/physics_sim_receiver_worker.c']), (probe,[ROOT/'tests/receiver_lifetime_test.c',ROOT/'src/tools/physics_sim_receiver_client.c'])]:
    result=subprocess.run(flags+list(map(str,inputs+sources))+['-o',str(output)],text=True,capture_output=True)
    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   for mode in ('normal','ignore','descendant','eof','malformed','wire','admission'):
    with self.subTest(mode=mode):
     result=subprocess.run([str(probe),mode,str(worker),str(base/mode)],text=True,capture_output=True,timeout=8)
     self.assertEqual(result.returncode,0,result.stdout+result.stderr);print(mode,result.stdout.strip())
if __name__=='__main__':unittest.main()
