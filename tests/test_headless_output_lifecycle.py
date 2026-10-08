"""Native output overwrite preserves predecessors and holds uncertain roots."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class NativeOutput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler=tempfile.TemporaryDirectory();cls.bin=Path(cls.compiler.name)/'probe'
        harness=Path(cls.compiler.name)/'probe.c'
        harness.write_text(r"""
#include "app/physics_sim_headless_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char **argv){
 PhysicsSimHeadlessOutputOwner owner;char error[256];if(argc<3)return 3;
 if(!physics_sim_headless_output_prepare(argv[1],argc>3?argv[3]:NULL,strcmp(argv[2],"overwrite")==0,&owner,error,sizeof(error))){puts(error);return 2;}
 if(argc>4){
  char marker[2048],outside[2048],sentinel[2048],bytes[256];
  snprintf(marker,sizeof(marker),"%s/.physics-sim-headless-owner",argv[1]);
  snprintf(outside,sizeof(outside),"%s/outside-marker",argv[1]);
  snprintf(sentinel,sizeof(sentinel),"%s/sentinel",argv[1]);
  FILE *input=fopen(marker,"rb");if(!input)return 4;size_t n=fread(bytes,1,sizeof(bytes),input);fclose(input);
  FILE *evidence=fopen(sentinel,"wb");if(!evidence)return 4;fputs("evidence",evidence);fclose(evidence);
  if(!strcmp(argv[4],"hardlink")){if(link(marker,outside))return 4;}
  else if(!strcmp(argv[4],"unlink")){if(unlink(marker))return 4;}
  else if(!strcmp(argv[4],"symlink")){
   FILE *foreign=fopen(outside,"wb");if(!foreign)return 4;fputs("foreign",foreign);fclose(foreign);
   if(unlink(marker)||symlink(outside,marker))return 4;
  }else{
   if(!strcmp(argv[4],"replace") && rename(marker,outside))return 4;
   FILE *changed=fopen(marker,"wb");if(!changed)return 4;
   if(!strcmp(argv[4],"changed"))fputs("changed marker",changed);else fwrite(bytes,1,n,changed);
   fclose(changed);
  }
 }
 int ok=strcmp(argv[2],"running")==0||physics_sim_headless_output_finish(&owner);
 physics_sim_headless_output_close(&owner);return ok?0:1;
}
""")
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(harness),str(ROOT/'src/app/physics_sim_headless_output.c'),'-o',str(cls.bin)],check=True,capture_output=True)
        wrapper=Path(cls.compiler.name)/'fault.c'
        wrapper.write_text(r"""
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
int output_test_openat(int directory,const char *name,int flags,...){
 int descriptor;
 if(flags&O_CREAT){va_list args;va_start(args,flags);int mode=va_arg(args,int);va_end(args);descriptor=openat(directory,name,flags,mode);}
 else descriptor=openat(directory,name,flags);
 if(descriptor>=0 && getenv("OUTPUT_TEST_COMPLETION_STAGE_DRIFT") && !strncmp(name,".headless-owner-",16)){
  int marker=openat(directory,".physics-sim-headless-owner",O_WRONLY|O_TRUNC);
  if(marker<0){close(descriptor);return -1;}
  const char *changed="changed while completion staged";
  int valid=write(marker,changed,strlen(changed))==(ssize_t)strlen(changed);
  if(close(marker))valid=0;
  if(!valid){close(descriptor);return -1;}
 }
 return descriptor;
}
char *output_test_mkdtemp(char *path){
 char *result=mkdtemp(path);const char *kind=getenv("OUTPUT_TEST_MARKER_DRIFT");
 if(result && kind){
  char root[4096],marker[4096],buffer[256];const char *suffix=strstr(path,".retained-");
  if(!suffix)return NULL;snprintf(root,sizeof(root),"%.*s",(int)(suffix-path),path);
  snprintf(marker,sizeof(marker),"%s/.physics-sim-headless-owner",root);
  FILE *input=fopen(marker,"rb");if(!input)return NULL;size_t n=fread(buffer,1,sizeof(buffer)-1,input);fclose(input);buffer[n]=0;
  if(!strcmp(kind,"running")){
   char *word=strstr(buffer,"completed");if(!word)return NULL;
   memmove(word+7,word+9,strlen(word+9)+1);memcpy(word,"running",7);n-=2;
  }
  char replacement[4096];snprintf(replacement,sizeof(replacement),"%s/.owner-test-replacement",root);
  FILE *out=fopen(!strcmp(kind,"replace")?replacement:marker,"wb");if(!out)return NULL;
  int valid=fwrite(buffer,1,n,out)==n;if(fclose(out))valid=0;
  if(!valid)return NULL;
  if(!strcmp(kind,"replace") && rename(replacement,marker))return NULL;
 }
 return result;
}
""")
        obj=Path(cls.compiler.name)/'output-fault.o';cls.fault_bin=Path(cls.compiler.name)/'fault-probe'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-Dmkdtemp=output_test_mkdtemp','-Dopenat=output_test_openat','-c',str(ROOT/'src/app/physics_sim_headless_output.c'),'-o',str(obj)],check=True,capture_output=True)
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(harness),str(wrapper),str(obj),'-o',str(cls.fault_bin)],check=True,capture_output=True)
    @classmethod
    def tearDownClass(cls):cls.compiler.cleanup()
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
    def invoke(self,path,mode='fresh',input=None):
        return subprocess.run([str(self.bin),str(path),mode,*([str(input)] if input else [])],capture_output=True,text=True,timeout=5)
    def test_completed_overwrite_preserves_every_predecessor_byte(self):
        out=self.root/'out';self.assertEqual(self.invoke(out).returncode,0)
        (out/'sentinel').write_bytes(b'evidence');before=(out/'.physics-sim-headless-owner').read_bytes()
        result=self.invoke(out,'overwrite');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        previous=list(self.root.glob('out.retained-*/previous'));self.assertEqual(len(previous),1)
        self.assertEqual((previous[0]/'sentinel').read_bytes(),b'evidence');self.assertEqual((previous[0]/'.physics-sim-headless-owner').read_bytes(),before)
        self.assertFalse((out/'sentinel').exists());self.assertIn('retained predecessor',result.stderr)
    def test_unknown_and_running_roots_are_held(self):
        unknown=self.root/'unknown';unknown.mkdir();(unknown/'sentinel').write_bytes(b'held')
        self.assertNotEqual(self.invoke(unknown,'overwrite').returncode,0);self.assertEqual((unknown/'sentinel').read_bytes(),b'held')
        running=self.root/'running';self.assertEqual(self.invoke(running,'running').returncode,0)
        self.assertNotEqual(self.invoke(running,'overwrite').returncode,0)
        self.assertEqual(list(self.root.glob('*.retained-*')),[])
    def test_nonempty_without_overwrite_preserves_root(self):
        out=self.root/'out';self.assertEqual(self.invoke(out).returncode,0)
        self.assertNotEqual(self.invoke(out).returncode,0);self.assertEqual(list(self.root.glob('*.retained-*')),[])
    def test_symlink_components_and_protected_paths_fail_without_effects(self):
        target=self.root/'target';target.mkdir();link=self.root/'link';link.symlink_to(target)
        for path in (link,link/'new',self.root/'../escape',Path('/usr/physics-output-test'),Path('/private/tmp'),self.root/'Demo.app'/'output'):
            with self.subTest(path=path):self.assertNotEqual(self.invoke(path,'overwrite').returncode,0)
        self.assertEqual(list(target.iterdir()),[])
    def test_checkout_storage_and_input_overlap(self):
        repo=self.root/'repo';repo.mkdir();(repo/'.git').write_text('gitdir: fixture')
        for path in (repo,repo/'src/output',repo/'tests/output',repo/'build'):
            self.assertNotEqual(self.invoke(path,'overwrite').returncode,0)
        self.assertEqual(self.invoke(repo/'build/run').returncode,0)
        out=self.root/'input-output';out.mkdir();scene=out/'scene.json';scene.write_bytes(b'input')
        self.assertNotEqual(self.invoke(out,'overwrite',scene).returncode,0);self.assertEqual(scene.read_bytes(),b'input')
    def test_copied_marker_cannot_claim_a_different_directory(self):
        first=self.root/'first';self.assertEqual(self.invoke(first).returncode,0)
        second=self.root/'second';second.mkdir();(second/'.physics-sim-headless-owner').write_bytes((first/'.physics-sim-headless-owner').read_bytes())
        self.assertNotEqual(self.invoke(second,'overwrite').returncode,0)
    def test_hardlinked_completed_marker_is_held(self):
        out=self.root/'out';self.assertEqual(self.invoke(out).returncode,0)
        marker=out/'.physics-sim-headless-owner';expected=marker.read_bytes()
        outside=self.root/'outside-marker';os.link(marker,outside)
        (out/'sentinel').write_bytes(b'held')
        self.assertEqual(self.invoke(out,'overwrite').returncode,2)
        self.assertEqual(marker.read_bytes(),expected);self.assertEqual(outside.read_bytes(),expected)
        self.assertEqual((out/'sentinel').read_bytes(),b'held');self.assertEqual(list(self.root.glob('out.retained-*')),[])

    def test_noncanonical_completed_markers_are_held(self):
        for kind in ('whitespace','plus','zero','missing-newline','trailing-space'):
            with self.subTest(kind=kind):
                out=self.root/kind;self.assertEqual(self.invoke(out).returncode,0)
                marker=out/'.physics-sim-headless-owner';data=marker.read_bytes()
                if kind=='whitespace':data=data.replace(b'completed ',b'completed  ')
                if kind=='plus':data=data.replace(b'completed ',b'completed +')
                if kind=='zero':data=data.replace(b'completed ',b'completed 0')
                if kind=='missing-newline':data=data.rstrip(b'\n')
                if kind=='trailing-space':data=data.rstrip(b'\n')+b' \n'
                marker.write_bytes(data);(out/'sentinel').write_bytes(b'held')
                self.assertEqual(self.invoke(out,'overwrite').returncode,2)
                self.assertEqual(marker.read_bytes(),data);self.assertEqual((out/'sentinel').read_bytes(),b'held')
                self.assertEqual(list(self.root.glob(kind+'.retained-*')),[])

    def test_marker_drift_after_validation_holds_before_retirement(self):
        for kind in ('running','rewrite','replace'):
            with self.subTest(kind=kind):
                out=self.root/kind;self.assertEqual(self.invoke(out).returncode,0)
                marker=out/'.physics-sim-headless-owner';before=marker.read_bytes();inode=marker.stat().st_ino
                (out/'sentinel').write_bytes(b'held');env=os.environ.copy();env['OUTPUT_TEST_MARKER_DRIFT']=kind
                result=subprocess.run([str(self.fault_bin),str(out),'overwrite'],env=env,capture_output=True,text=True,timeout=5)
                self.assertEqual(result.returncode,2,result.stdout);self.assertEqual((out/'sentinel').read_bytes(),b'held')
                self.assertEqual(list(self.root.glob(kind+'.retained-*/previous')),[])
                if kind=='running':self.assertIn(b' running ',marker.read_bytes())
                else:self.assertEqual(marker.read_bytes(),before)
                if kind=='replace':self.assertNotEqual(marker.stat().st_ino,inode)

    def test_changed_running_marker_holds_completion_without_replacing_it(self):
        for kind in ('hardlink','unlink','symlink','replace','rewrite','changed'):
            with self.subTest(kind=kind):
                out=self.root/kind
                result=subprocess.run([str(self.bin),str(out),'fresh','',kind],capture_output=True,text=True,timeout=5)
                self.assertEqual(result.returncode,1,result.stdout+result.stderr)
                self.assertEqual((out/'sentinel').read_bytes(),b'evidence')
                marker=out/'.physics-sim-headless-owner';outside=out/'outside-marker'
                if kind=='unlink':self.assertFalse(marker.exists())
                elif kind=='symlink':self.assertTrue(marker.is_symlink());self.assertEqual(outside.read_bytes(),b'foreign')
                elif kind=='changed':self.assertEqual(marker.read_bytes(),b'changed marker')
                else:self.assertIn(b' running ',marker.read_bytes())
                self.assertEqual(list(out.glob('.headless-owner-*.pending')),[])

    def test_running_marker_drift_during_completion_staging_is_retained(self):
        out=self.root/'late-completion';env=os.environ.copy();env['OUTPUT_TEST_COMPLETION_STAGE_DRIFT']='1'
        result=subprocess.run([str(self.fault_bin),str(out),'fresh'],env=env,capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,1,result.stdout+result.stderr)
        self.assertEqual((out/'.physics-sim-headless-owner').read_bytes(),b'changed while completion staged')
        pending=list(out.glob('.headless-owner-*.pending'));self.assertEqual(len(pending),1)
        self.assertIn(b' completed ',pending[0].read_bytes())

    def test_linked_and_oversized_markers_are_held(self):
        outside=self.root/'marker';outside.write_text('held')
        for name in ('linked','oversized'):
            out=self.root/name;out.mkdir();marker=out/'.physics-sim-headless-owner'
            if name=='linked':marker.symlink_to(outside)
            else:marker.write_bytes(b'x'*1024)
            self.assertNotEqual(self.invoke(out,'overwrite').returncode,0)
        self.assertEqual(outside.read_text(),'held')

if __name__=='__main__':unittest.main()
