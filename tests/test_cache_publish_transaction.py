"""Actual seven-slot native publication with controlled syscall faults."""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import shlex
import unittest
import test_cache_publish_admission as admission
ROOT=admission.ROOT

class CacheTransaction(admission.CacheAdmission):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        base=Path(cls.t.name)
        wrapper=base/'faults.c'
        wrapper.write_text(r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include "export/volume_frame_vf3d_contract.h"
int cache_test_fclose(FILE *stream) {
 const char *target=getenv("CACHE_TEST_MANIFEST_COPY");char path[4096]={0};int named=0;
 if(target){
#ifdef __APPLE__
  named=fcntl(fileno(stream),F_GETPATH,path)==0;
#else
  char proc[64];snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fileno(stream));
  named=readlink(proc,path,sizeof(path)-1)>0;
#endif
 }
 int result=fclose(stream);
 if(result==0 && target && named && (strstr(path,target) || (!strcmp(target,"all") && (strstr(path,"/new-4") || strstr(path,"/new-5") || strstr(path,"/new-6"))))){
  FILE *input=fopen(path,"rb");if(!input)return EOF;
  char data[4096];size_t n=fread(data,1,sizeof(data)-1,input);data[n]=0;fclose(input);
  const char *kind=getenv("CACHE_TEST_MANIFEST_KIND");const char *from="run-good",*to="run-evil";
  if(kind && !strcmp(kind,"time")){from="created_at\": \"2";to="created_at\": \"3";}
  if(kind && !strcmp(kind,"count")){from="frame_count\": 1";to="frame_count\": 2";}
  if(kind && !strcmp(kind,"extra")){from="\n}";to=",\"unexpected\":true\n}";}
  char *where=strstr(data,from);if(!where)return EOF;
  char output[8192];int length=snprintf(output,sizeof(output),"%.*s%s%s",(int)(where-data),data,to,where+strlen(from));
  if(length<0 || (size_t)length>=sizeof(output))return EOF;
  FILE *changed=fopen(path,"wb");if(!changed)return EOF;
  int valid=fwrite(output,1,(size_t)length,changed)==(size_t)length;
  if(fclose(changed))valid=0;
  if(!valid)return EOF;
 }
 return result;
}
static int payload_clocks;
static int interrupted_payload;
int cache_test_clock_gettime(clockid_t id,struct timespec *value) {
 int result=clock_gettime(id,value);
 if(result==0 && getenv("CACHE_TEST_PAYLOAD_EXPIRE") && ++payload_clocks>1)value->tv_sec+=121;
 return result;
}
static int stage_mutated;
ssize_t cache_test_pread(int fd,void *buffer,size_t bytes,off_t offset) {
 const char *target=getenv("CACHE_TEST_STAGE_MUTATE");
 if(target && !stage_mutated){
  char path[4096]={0};
#ifdef __APPLE__
  int named=fcntl(fd,F_GETPATH,path)==0;
#else
  char proc[64];snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fd);
  ssize_t length=readlink(proc,path,sizeof(path)-1);int named=length>0;
#endif
  char *later=named?strstr(path,"/new-2/manifest.json"):NULL;
  if(later){
   stage_mutated=1;*later=0;char changed[4096];
   snprintf(changed,sizeof(changed),"%s/%s",path,target);
   FILE *stream=fopen(changed,"r+b");if(!stream){errno=EIO;return -1;}
   const char *kind=getenv("CACHE_TEST_STAGE_MUTATION_KIND");
   int valid;
   if(kind && !strcmp(kind,"rewrite")){
    int byte=fgetc(stream);valid=byte!=EOF && fseek(stream,0,SEEK_SET)==0 && fputc(byte,stream)!=EOF;
   }else if(strstr(changed,".vf3d")){
    float finite=0.25f;valid=fseek(stream,(long)sizeof(VolumeFrameHeaderVf3dV1),SEEK_SET)==0 && fwrite(&finite,sizeof(finite),1,stream)==1;
   }else {valid=fseek(stream,0,SEEK_END)==0 && fputc(' ',stream)!=EOF;}
   if(fclose(stream))valid=0;
   if(!valid){errno=EIO;return -1;}
  }
 }
 if(offset>=(off_t)sizeof(VolumeFrameHeaderVf3dV1) && getenv("CACHE_TEST_PAYLOAD_READ_FAIL")){errno=EIO;return -1;}
 if(offset>=(off_t)sizeof(VolumeFrameHeaderVf3dV1) && getenv("CACHE_TEST_PAYLOAD_SHORT_READ")){
  if(!interrupted_payload++){errno=EINTR;return -1;}
  if(bytes>7)bytes=7;
 }
 return pread(fd,buffer,bytes,offset);
}
static int calls;
static FILE *corrupt_output;
static FILE *finite_output;
static FILE *byte_output;
static int source_mutated;
size_t cache_test_fwrite(const void *p,size_t s,size_t n,FILE *stream) {
 if(getenv("CACHE_TEST_BYTE_COPY") && stream==byte_output && ftell(stream)==0 && s==1 && n){
  unsigned char *changed=malloc(n);if(!changed)return 0;
  memcpy(changed,p,n);changed[0]=9;
  size_t result=fwrite(changed,s,n,stream);free(changed);return result;
 }
 if(getenv("CACHE_TEST_FINITE_COPY") && stream==finite_output && ftell(stream)==0 && s==1 && n>sizeof(VolumeFrameHeaderVf3dV1)){
  unsigned char *changed=malloc(n);if(!changed)return 0;
  memcpy(changed,p,n);changed[sizeof(VolumeFrameHeaderVf3dV1)]^=1;
  size_t result=fwrite(changed,s,n,stream);free(changed);return result;
 }
 if(getenv("CACHE_TEST_CORRUPT_COPY") && stream==corrupt_output){fwrite(p,s,n/2,stream);return n;}
 return fwrite(p,s,n,stream);
}
int cache_test_rename(const char *a,const char *b) {
 const char *fail=getenv("CACHE_TEST_RENAME_FAILURE");
 const char *death=getenv("CACHE_TEST_RENAME_DEATH");
 ++calls;
 if(getenv("CACHE_TEST_STAGE_RENAME_MUTATE") && calls==1){
  const char *prior=strstr(b,"/prior-0");if(!prior){errno=EIO;return -1;}
  char path[4096];snprintf(path,sizeof(path),"%.*s/new-1/frame_000000.vf3d",(int)(prior-b),b);
  FILE *changed=fopen(path,"r+b");if(!changed){errno=EIO;return -1;}
  float finite=0.5f;int valid=fseek(changed,(long)sizeof(VolumeFrameHeaderVf3dV1),SEEK_SET)==0 && fwrite(&finite,sizeof(finite),1,changed)==1;
  if(fclose(changed))valid=0;
  if(!valid){errno=EIO;return -1;}
 }
 if(death && calls==atoi(death))_exit(77);
 if(fail && calls==atoi(fail)){errno=EIO;return -1;} return rename(a,b);
}
#ifdef __APPLE__
FILE *cache_test_fopen(const char *p,const char *m) __asm("_cache_test_fopen$DARWIN_EXTSN");
#endif
FILE *cache_test_fopen(const char *p,const char *m) {
 if(getenv("CACHE_TEST_COPY_FAILURE") && strstr(p,"/new-1/")){errno=EIO;return NULL;}
 const char *source_target=getenv("CACHE_TEST_SOURCE_MUTATE");
 if(source_target && !source_mutated && strstr(p,"/volume_frames/") && strstr(p,source_target) && strchr(m,'r')){
  source_mutated=1;
  FILE *changed=fopen(p,"r+b");if(!changed)return NULL;
  const char *kind=getenv("CACHE_TEST_SOURCE_MUTATION_KIND");
  if(kind && !strcmp(kind,"rewrite")){
   int byte=fgetc(changed);if(byte==EOF || fseek(changed,0,SEEK_SET) || fputc(byte,changed)==EOF){fclose(changed);return NULL;}
  }else if(kind && !strcmp(kind,"add")){
   char extra[1024];snprintf(extra,sizeof(extra),"%.*s/water_surface_added.json",(int)(strrchr(p,'/')-p),p);
   FILE *added=fopen(extra,"wb");if(!added){fclose(changed);return NULL;}
   if(fputs("{}",added)==EOF || fclose(added)){fclose(changed);return NULL;}
  }else if(strstr(p,".vf3d")){
   float finite=0.125f;if(fseek(changed,(long)sizeof(VolumeFrameHeaderVf3dV1),SEEK_SET) || fwrite(&finite,sizeof(finite),1,changed)!=1){fclose(changed);return NULL;}
  }else {if(fputc(9,changed)==EOF){fclose(changed);return NULL;}}
  if(fclose(changed))return NULL;
 }
 finite_output=NULL;byte_output=NULL;
 FILE *result=fopen(p,m);
 if(getenv("CACHE_TEST_CORRUPT_COPY") && strstr(p,"/new-0/") && strstr(p,".vf3d") && strchr(m,'w'))corrupt_output=result;
 if(getenv("CACHE_TEST_FINITE_COPY") && strstr(p,"/new-0/") && strstr(p,".vf3d") && strchr(m,'w'))finite_output=result;
const char *byte_target=getenv("CACHE_TEST_BYTE_COPY");
 if(byte_target && strstr(p,byte_target) && strchr(m,'w'))byte_output=result;
 return result;
}
""")
        obj=base/'cache.o'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),*shlex.split(subprocess.check_output(['pkg-config','--cflags','json-c'],text=True)),'-Drename=cache_test_rename','-Dfopen=cache_test_fopen','-Dfwrite=cache_test_fwrite','-Dfclose=cache_test_fclose','-Dclock_gettime=cache_test_clock_gettime','-Dpread=cache_test_pread','-c',str(ROOT/'src/app/scene_project_cache_output.c'),'-o',str(obj)],check=True)
        probe=base/'probe.c'
        source=probe.read_text();source=source.replace('SceneProjectCacheOutputResolved project={0};', 'if(!strcmp(argv[4],"status")){SceneProjectCacheOutputStatus status={0};return scene_project_cache_output_status_from_project(argv[1],&status,error,sizeof(error))?0:2;}\nSceneProjectCacheOutputResolved project={0};')
        probe.write_text(source)
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(probe),str(wrapper),str(obj),str(ROOT/'src/app/physics_sim_json_helpers.c'),str(ROOT/'src/app/physics_sim_job_json.c'),*shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','json-c'],text=True)),'-o',str(cls.binary)],check=True)
    def setUp(self):
        super().setUp()
        for name in ("scene_project.json","scene_runtime.json","scene_authoring.json"):(self.project/name).write_text("{}")
    def attempts(self):return sorted((self.project/'physics_sim').glob('.cache-publication-attempt-*'))
    def targets(self):return self.slots()+[self.project/'physics_sim/active_cache_manifest.json',self.project/'physics_sim/cache_manifest.json',self.run/'cache_manifest.json']
    def predecessors(self):
        self.retained()
        for p in self.targets()[4:]:p.write_bytes(b'previous-manifest')
    def test_success_retains_all_seven_predecessors_and_repeated_attempts(self):
        self.predecessors();result=self.invoke();self.assertEqual(result.returncode,0,result.stdout)
        attempt=self.attempts()[0];plan=json.loads((attempt/'plan.json').read_text());self.assertEqual(len(plan['targets']),7)
        for i in range(4):self.assertEqual((attempt/f'prior-{i}'/'retained').read_bytes(),b'previous')
        for i in range(4,7):self.assertEqual((attempt/f'prior-{i}').read_bytes(),b'previous-manifest')
        self.assertEqual(self.invoke(mode='status').returncode,0);self.assertTrue((attempt/'complete').is_file());self.assertFalse((self.project/'physics_sim/.cache-publication.pending').exists())
        self.assertEqual(self.invoke().returncode,0);self.assertEqual(len(self.attempts()),2)
    def test_copy_failure_preserves_every_visible_predecessor_and_holds_retry(self):
        self.predecessors();env=os.environ.copy();env['CACHE_TEST_COPY_FAILURE']='1'
        self.assertEqual(self.invoke(env=env).returncode,2)
        self.assert_retained([p/'retained' for p in self.slots()])
        for p in self.targets()[4:]:self.assertEqual(p.read_bytes(),b'previous-manifest')
        self.assertEqual(self.invoke().returncode,2);self.assertEqual(self.invoke(mode='status').returncode,2)
        self.assertEqual(len(self.attempts()),1)
    def test_every_rename_failure_retains_each_original_and_status_holds(self):
        # Two renames per existing slot: every boundary is injected in an independent fixture.
        for failure in range(1,15):
            with self.subTest(failure=failure):
                self.setUp();self.predecessors();env=os.environ.copy();env['CACHE_TEST_RENAME_FAILURE']=str(failure)
                self.assertEqual(self.invoke(env=env).returncode,2)
                attempt=self.attempts()[0]
                for i,target in enumerate(self.targets()):
                    original=attempt/f'prior-{i}'
                    if not original.exists():original=target
                    self.assertEqual((original/'retained').read_bytes() if i<4 else original.read_bytes(),b'previous' if i<4 else b'previous-manifest')
                self.assertEqual(self.invoke(mode='status').returncode,2);self.assertEqual(self.invoke().returncode,2)
    def test_process_death_preserves_originals_and_durable_hold_after_lock_release(self):
        self.predecessors();env=os.environ.copy();env['CACHE_TEST_RENAME_DEATH']='6'
        self.assertEqual(self.invoke(env=env).returncode,77)
        attempt=self.attempts()[0]
        for i,target in enumerate(self.targets()):
            original=attempt/f'prior-{i}'
            if not original.exists():original=target
            self.assertEqual((original/'retained').read_bytes() if i<4 else original.read_bytes(),b'previous' if i<4 else b'previous-manifest')
        lock=self.project/'physics_sim/.cache-publication.lock'
        with lock.open('rb') as f:fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        self.assertEqual(self.invoke(mode='status').returncode,2);self.assertEqual(self.invoke().returncode,2)
        self.assertEqual(len(self.attempts()),1)
    def test_failed_first_publication_keeps_final_slots_absent(self):
        env=os.environ.copy();env['CACHE_TEST_COPY_FAILURE']='1'
        self.assertEqual(self.invoke(env=env).returncode,2)
        self.assertTrue(all(not target.exists() for target in self.targets()))
        self.assertTrue((self.attempts()[0]/'new-0'/'frame_000000.vf3d').exists())
        self.assertEqual(self.invoke().returncode,2)
    def test_busy_owner_and_invalid_pending_hold_without_attempt(self):
        sentinels=self.retained();lock=self.project/'physics_sim/.cache-publication.lock';lock.touch()
        with lock.open('rb') as f:
            fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
            self.assertEqual(self.invoke().returncode,2);self.assertEqual(self.invoke(mode='status').returncode,2)
        self.assert_retained(sentinels);self.assertEqual(self.attempts(),[])
        (self.project/'physics_sim/.cache-publication.pending').symlink_to(self.base/'absent')
        self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels);self.assertEqual(self.attempts(),[])

if __name__=='__main__':unittest.main()
