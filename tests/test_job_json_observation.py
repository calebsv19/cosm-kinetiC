"""Live generation observation retries only drift, with explicit read/time bounds."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class JsonObservation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);cls.binary=base/'probe';source=base/'probe.c'
        source.write_text(r"""#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_json.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static const char *selected,*mode;static int calls;
ssize_t read(int fd,void *buffer,size_t size){off_t offset=lseek(fd,0,SEEK_CUR);ssize_t count=pread(fd,buffer,size,offset);if(count>0)lseek(fd,count,SEEK_CUR);calls++;
if(strcmp(mode,"always")==0||strcmp(mode,"slow")==0||(strcmp(mode,"once")==0&&calls==1)){
if(strcmp(mode,"slow")==0){struct timespec pause={0,120000000};nanosleep(&pause,0);}
int writer=open(selected,O_WRONLY);if(writer>=0){const char *text=calls%2?"{\"v\":2}":"{\"v\":1}";pwrite(writer,text,7,0);fsync(writer);close(writer);}}
return count;}
int main(int argc,char **argv){if(argc!=3)return 3;selected=argv[1];mode=argv[2];json_object *object=physics_sim_job_json_observe(selected),*v=0;int number=-1;
if(object&&json_object_object_get_ex(object,"v",&v))number=json_object_get_int(v);printf("%d %d %d\n",object!=0,calls,number);if(object)json_object_put(object);return 0;}
""")
        flags=shlex.split(subprocess.check_output([os.environ.get('PKG_CONFIG','pkg-config'),'--cflags','--libs','json-c'],text=True))
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),str(ROOT/'src/app/physics_sim_job_json.c'),*flags,'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def invoke(self,mode,text='{"v":1}'):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory).resolve()/'progress.json';path.write_text(text)
            result=subprocess.run([str(self.binary),str(path),mode],capture_output=True,text=True,timeout=3)
            self.assertEqual(result.returncode,0,result.stderr);return [int(v) for v in result.stdout.split()]
    def test_one_changed_generation_reobserves_stable_new_bytes(self):self.assertEqual(self.invoke('once'),[1,2,2])
    def test_continuous_drift_stops_at_eight_reads(self):self.assertEqual(self.invoke('always'),[0,8,-1])
    def test_malformed_json_is_not_retried(self):self.assertEqual(self.invoke('none','{"v":NaN}'),[0,1,-1])
    def test_cooperative_deadline_stops_after_slow_first_read(self):self.assertEqual(self.invoke('slow'),[0,1,-1])
if __name__=='__main__':unittest.main()
