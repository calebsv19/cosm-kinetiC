"""Atomic native sidecar generations preserve readable predecessors on failure."""
import json
import os
from pathlib import Path
import selectors
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AtomicSidecars(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);cls.binary=base/'probe'
        source=base/'probe.c'
        source.write_text(r'''#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_headless_output.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <signal.h>
int main(int argc,char **argv){if(argc!=3)return 3;PhysicsSimHeadlessSidecar sidecar;
if(!physics_sim_headless_sidecar_claim(argv[1],&sidecar))return 4;
FILE *stream=physics_sim_headless_sidecar_stream(&sidecar);if(!stream)return 5;
fputs("{\"version\":1}\n",stream);if(!physics_sim_headless_sidecar_publish(&sidecar,stream))return 6;
if(strcmp(argv[2],"repeat")==0){puts("READY");fflush(stdout);for(int n=2;n<=80;n++){stream=physics_sim_headless_sidecar_stream(&sidecar);if(!stream)return 7;fprintf(stream,"{\"version\":%d}\n",n);if(!physics_sim_headless_sidecar_publish(&sidecar,stream))return 8;}physics_sim_headless_sidecar_close(&sidecar);return 0;}
stream=physics_sim_headless_sidecar_stream(&sidecar);if(!stream)return 9;
fputs("{\"version\":",stream);fflush(stream);fsync(fileno(stream));
if(strcmp(argv[2],"pause")==0){puts("READY");fflush(stdout);sleep(20);return 10;}
if(strcmp(argv[2],"replace")==0){char previous[1200];snprintf(previous,sizeof(previous),"%s.previous",argv[1]);rename(argv[1],previous);FILE *foreign=fopen(argv[1],"wb");fputs("foreign",foreign);fclose(foreign);}
if(strcmp(argv[2],"stage-link")==0){char staged[1200],old[1200];snprintf(staged,sizeof(staged),"%s/%s",sidecar.parent,sidecar.pending_name);snprintf(old,sizeof(old),"%s.old",staged);rename(staged,old);symlink("foreign",staged);}
if(strcmp(argv[2],"io-error")==0){struct rlimit limit;if(getrlimit(RLIMIT_FSIZE,&limit))return 12;limit.rlim_cur=16;signal(SIGXFSZ,SIG_IGN);if(setrlimit(RLIMIT_FSIZE,&limit))return 13;setvbuf(stream,NULL,_IONBF,0);fputs("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",stream);if(!ferror(stream))return 14;}
if(strcmp(argv[2],"close-error")==0)close(fileno(stream));
int published=physics_sim_headless_sidecar_publish(&sidecar,stream);int retry=physics_sim_headless_sidecar_stream(&sidecar)!=NULL;
physics_sim_headless_sidecar_close(&sidecar);return !published&&!retry?0:11;}
''')
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),str(ROOT/'src/app/physics_sim_headless_output.c'),'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name).resolve();self.path=self.base/'progress.json'
    def invoke(self,mode):
        return subprocess.run([str(self.binary),str(self.path),mode],capture_output=True,text=True,timeout=5)
    def child(self,mode):
        process=subprocess.Popen([str(self.binary),str(self.path),mode],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        def cleanup():
            if process.poll() is None:process.kill();process.wait(timeout=5)
            process.stdout.close();process.stderr.close()
        self.addCleanup(cleanup)
        selector=selectors.DefaultSelector();selector.register(process.stdout,selectors.EVENT_READ)
        try:self.assertTrue(selector.select(5),'No staged readiness signal')
        finally:selector.close()
        self.assertEqual(process.stdout.readline().strip(),'READY');return process
    def test_forced_death_retains_valid_predecessor_and_partial_stage(self):
        process=self.child('pause');self.assertEqual(json.loads(self.path.read_text()),{'version':1})
        pending=list(self.base.glob('.headless-sidecar-*.pending'));self.assertEqual(len(pending),1)
        self.assertEqual(pending[0].read_bytes(),b'{"version":')
        process.kill();process.wait(timeout=5)
        self.assertEqual(json.loads(self.path.read_text()),{'version':1});self.assertTrue(pending[0].exists())
    def test_readers_observe_only_complete_generations(self):
        process=self.child('repeat');observed=[]
        while process.poll() is None:observed.append(json.loads(self.path.read_text())['version'])
        self.assertEqual(process.wait(timeout=5),0,process.stderr.read())
        self.assertEqual(json.loads(self.path.read_text()),{'version':80});self.assertTrue(all(1<=n<=80 for n in observed))
        self.assertEqual(list(self.base.glob('.headless-sidecar-*.pending')),[])
    def test_foreign_replacement_holds_publication_and_retains_stage(self):
        result=self.invoke('replace');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(self.path.read_bytes(),b'foreign');self.assertEqual(json.loads((self.base/'progress.json.previous').read_text()),{'version':1})
        self.assertEqual(len(list(self.base.glob('.headless-sidecar-*.pending'))),1)
    def test_staged_symlink_holds_and_preserves_foreign_target(self):
        foreign=self.base/'foreign';foreign.write_bytes(b'held')
        result=self.invoke('stage-link');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(json.loads(self.path.read_text()),{'version':1});self.assertEqual(foreign.read_bytes(),b'held')
        self.assertEqual(len(list(self.base.glob('*.old'))),1)
    def test_close_failure_preserves_predecessor_and_blocks_retry(self):
        result=self.invoke('close-error');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(json.loads(self.path.read_text()),{'version':1});self.assertEqual(len(list(self.base.glob('.headless-sidecar-*.pending'))),1)
    def test_stream_error_cannot_publish_partial_payload(self):
        result=self.invoke('io-error');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(json.loads(self.path.read_text()),{'version':1});self.assertEqual(len(list(self.base.glob('.headless-sidecar-*.pending'))),1)
if __name__=='__main__':unittest.main()
