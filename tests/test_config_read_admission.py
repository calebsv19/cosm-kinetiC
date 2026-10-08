"""Native config FD admission and exact missing fallback with disposable inputs."""
import os
from pathlib import Path
import subprocess
import shlex
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ConfigReads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name)
        source=base/'probe.c'
        source.write_text(r"""
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "config/config_loader.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
const char *selected;int fault=0;int stat_count=0;
int read_fstat(int fd,struct stat *st){int result=fstat(fd,st);if(result==0&&fault==3&&++stat_count==2)st->st_size=2*1024*1024;return result;}
ssize_t config_read(int fd,void *bytes,size_t size){
    if(fault==1){fault=0;char old[4096];snprintf(old,sizeof(old),"%s.old",selected);if(rename(selected,old))return -1;FILE *f=fopen(selected,"wb");if(!f)return -1;fputs("{}",f);fclose(f);}
    else if(fault==2){fault=0;int other=open(selected,O_WRONLY|O_TRUNC);if(other<0)return -1;write(other,"{}",2);close(other);}
    return read(fd,bytes,size);
}
int main(int argc,char **argv){if(argc<4)return 9;selected=argv[1];fault=atoi(argv[3]);AppConfig cfg={0};ConfigLoadOptions opts={.path=argv[1],.allow_missing=atoi(argv[2])!=0};
int ok=config_loader_load(&cfg,&opts);printf("%d\n",cfg.grid_w);return ok?0:2;}
""")
        objects=[]
        for rel in ('src/config/config_loader.c','src/app/physics_sim_job_json.c','src/app/physics_sim_persistence.c','src/app/physics_sim_headless_output.c','src/app/app_config.c','src/app/data_paths.c'):
            obj=base/(Path(rel).stem+'.o');objects.append(str(obj))
            command=['clang','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-I'+str(ROOT/'src')]
            if rel=='src/config/config_loader.c':command+=['-Dread=config_read','-Dfstat=read_fstat']
            command += shlex.split(subprocess.check_output(['pkg-config','--cflags','json-c'],text=True))
            subprocess.run(command+['-c',str(ROOT/rel),'-o',str(obj)],capture_output=True,check=True)
        cls.binary=base/'probe'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),*objects,*shlex.split(subprocess.check_output(['pkg-config','--libs','json-c'],text=True)),'-lm','-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name);self.path=self.base/'config.json'
    def invoke(self,path=None,allow=False,fault=0,cwd=None):return subprocess.run([str(self.binary),str(path or self.path),'1' if allow else '0',str(fault)],cwd=cwd,capture_output=True,text=True,timeout=5)
    def test_regular_source_config_and_relative_path_are_supported(self):
        self.path.write_text('{"grid":{"width":37}}')
        self.assertEqual(self.invoke().returncode,0);self.assertEqual(self.invoke().stdout.strip(),'37')
        self.assertEqual(self.invoke(Path('config.json'),cwd=self.base).returncode,0)
        (self.base/'.git').write_text('marker');source=self.base/'config';source.mkdir();path=source/'app.json';path.write_text('{}')
        self.assertEqual(self.invoke(path).returncode,0)
    def test_missing_only_fallback_and_no_allocation(self):
        self.assertEqual(self.invoke(allow=True).returncode,0);self.assertEqual(self.invoke().returncode,2)
        self.assertEqual(list(self.base.iterdir()),[])
    def test_linked_file_parent_and_dangling_link_hold_even_with_fallback(self):
        target=self.base/'target';target.write_text('{}');self.path.symlink_to(target)
        self.assertEqual(self.invoke(allow=True).returncode,2);self.assertEqual(target.read_text(),'{}')
        linked=self.base/'linked';linked.symlink_to(self.base)
        self.assertEqual(self.invoke(linked/'target',allow=True).returncode,2)
        self.path.unlink();self.path.symlink_to(self.base/'missing');self.assertEqual(self.invoke(allow=True).returncode,2)
    def test_fifo_directory_and_hardlink_hold_without_blocking(self):
        os.mkfifo(self.path);self.assertEqual(self.invoke(allow=True).returncode,2)
        self.path.unlink();self.path.mkdir();self.assertEqual(self.invoke(allow=True).returncode,2)
        self.path.rmdir();target=self.base/'target';target.write_text('{}');os.link(target,self.path)
        self.assertEqual(self.invoke(allow=True).returncode,2);self.assertEqual(target.read_text(),'{}')
    def test_oversized_empty_and_embedded_nul_hold_even_with_fallback(self):
        for data in (b'',b'{}\0trailing'):
            self.path.write_bytes(data)
            self.assertEqual(self.invoke(allow=True).returncode,2)
        with self.path.open('wb') as f:f.truncate(1024*1024+1)
        self.assertEqual(self.invoke(allow=True).returncode,2)
    def test_changed_leaf_and_inplace_bytes_hold(self):
        for fault in (1,2):
            self.path.write_text('{"grid":{"width":37}}')
            self.assertEqual(self.invoke(allow=True,fault=fault).returncode,2)
            self.assertEqual(self.path.read_text(),'{}')
    def test_size_change_before_allocation_is_rejected(self):
        self.path.write_text('{}');self.assertEqual(self.invoke(allow=True,fault=3).returncode,2)
        self.assertEqual(self.path.read_text(),'{}')
    def test_parent_traversal_and_private_components_hold(self):
        self.path.write_text('{}')
        self.assertEqual(self.invoke(self.base/'child/../config.json',allow=True).returncode,2)
        private=self.base/'.ssh';private.mkdir();path=private/'config';path.write_text('{}')
        self.assertEqual(self.invoke(path,allow=True).returncode,2)
if __name__=='__main__':unittest.main()
