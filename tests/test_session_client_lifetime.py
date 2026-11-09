"""Contained real-process lifetime proof; no live simulation session is touched."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class Lifetime(unittest.TestCase):
    def test_normal_and_thread_refusal(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.environ.get('PHYSICS_L3_SANITIZE') else []
            common=['/usr/bin/clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),*flags]
            subprocess.run(common+['-Dpthread_create=l3_pthread_create','-Dpthread_attr_init=l3_pthread_attr_init','-Dpthread_attr_setdetachstate=l3_pthread_attr_setdetachstate','-c',str(ROOT/'src/app/session_client_lifetime.c'),'-o',str(root/'owner.o')],check=True)
            subprocess.run(common+[str(ROOT/'tests/session_client_lifetime_test.c'),str(root/'owner.o'),'-o',str(root/'probe')],check=True)
            for mode in ('normal','failure','attr-init','attr-state'):
                result=subprocess.run([str(root/'probe'),mode],text=True,capture_output=True,timeout=5)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                print(result.stdout+result.stderr,end='')
if __name__=='__main__':unittest.main()
