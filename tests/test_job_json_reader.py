"""Native bounded metadata reads reject ambiguous and unsafe input."""
import json
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class JobJson(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name);cls.binary=base/'probe'
        source=base/'probe.c'
        source.write_text(r'''#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
ssize_t read(int fd,void *buffer,size_t length){static int changed=0;const char *target=getenv("MUTATE");if(target&&!changed){changed=1;struct timespec delay={0,10000000};nanosleep(&delay,NULL);int writer=open(target,O_WRONLY);if(writer>=0){write(writer,"{\"changed\":true }",17);fsync(writer);close(writer);}}off_t offset=lseek(fd,0,SEEK_CUR);ssize_t count=pread(fd,buffer,length,offset);if(count>0)lseek(fd,count,SEEK_CUR);return count;}
int main(int argc,char **argv){if(argc!=2)return 3;json_object *object=physics_sim_job_json_read(argv[1]);if(!object)return 2;puts(json_object_to_json_string_ext(object,JSON_C_TO_STRING_PLAIN));json_object_put(object);return 0;}
''')
        flags=shlex.split(subprocess.check_output([os.environ.get('PKG_CONFIG','pkg-config'),'--cflags','--libs','json-c'],text=True))
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),str(ROOT/'src/app/physics_sim_job_json.c'),*flags,'-o',str(cls.binary)],check=True,capture_output=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name).resolve();self.path=self.root/'input.json'
    def invoke(self,payload=None,path=None,env=None):
        if payload is not None:self.path.write_bytes(payload if isinstance(payload,bytes) else payload.encode())
        return subprocess.run([str(self.binary),str(path or self.path)],capture_output=True,text=True,timeout=10,env=env)
    def test_valid_objects_and_escaped_names(self):
        row={'schema':'example','fields':[1,True,None,-2.5,{'quoted"key':'Unicode é'}]}
        result=self.invoke(json.dumps(row,ensure_ascii=False));self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(json.loads(result.stdout),row)
        self.assertEqual(self.invoke('{"\\u0070id":12}').returncode,0)
    def test_duplicate_keys_including_escaped_and_nested_keys_are_held(self):
        for data in ('{"pid":1,"pid":2}','{"pid":1,"\\u0070id":2}','{"nested":{"x":1,"x":2}}','{"arr":[{"x":1,"x":2}]}'):
            with self.subTest(data=data):self.assertNotEqual(self.invoke(data).returncode,0)
    def test_nonfinite_comments_trailing_and_invalid_roots_are_held(self):
        for data in ('{"x":NaN}','{"x":Infinity}','{"x":-Infinity}','{"x":1e9999}','{"x":1,}','{"x":1} trailing','{"x":/*comment*/1}','[1,2]','null','1','{"x":01}'):
            with self.subTest(data=data):self.assertNotEqual(self.invoke(data).returncode,0)
    def test_nul_invalid_utf8_and_unterminated_strings_are_held(self):
        for data in (b'{"x":"\xff"}',b'{"x":1}\x00',r'{"x":"\u0000"}',r'{"\u0000":1}','{"x":"unfinished}'):
            self.assertNotEqual(self.invoke(data).returncode,0)
    def test_depth_value_key_and_string_budgets_hold(self):
        cases=('{"x":'+ '['*70+'0'+']'*70+'}',json.dumps({'x':[0]*100001}),json.dumps({'k'*4097:1}),json.dumps({'x':'s'*1048577}))
        for data in cases:self.assertNotEqual(self.invoke(data).returncode,0)
    def test_file_size_bound_and_empty_file_hold(self):
        with self.path.open('wb') as stream:stream.truncate(16*1024*1024+1)
        self.assertNotEqual(self.invoke().returncode,0);self.assertNotEqual(self.invoke(b'').returncode,0)
    def test_links_special_and_protected_components_hold(self):
        self.path.write_text('{"x":1}');link=self.root/'link';link.symlink_to(self.path);fifo=self.root/'fifo';os.mkfifo(fifo)
        linked_dir=self.root/'linked-dir';linked_dir.symlink_to(self.root)
        protected=self.root/'.aws';protected.mkdir();(protected/'input.json').write_text('{}')
        for path in (link,fifo,linked_dir/'input.json',protected/'input.json',self.root/'../missing'):
            self.assertNotEqual(self.invoke(path=path).returncode,0)
    def test_same_size_in_place_change_during_read_is_held(self):
        self.path.write_bytes(b'{"changed":false}')
        env=dict(os.environ,MUTATE=str(self.path));self.assertNotEqual(self.invoke(env=env).returncode,0)
    def test_deterministic_roundtrip_corpus(self):
        generator=random.Random(1707)
        for _ in range(40):
            row={'values':[generator.randint(-10000,10000),generator.random(),None,True,{'key\\quoted"':str(generator.randrange(1000))}]}
            result=self.invoke(json.dumps(row));self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(json.loads(result.stdout),row)
if __name__=='__main__':unittest.main()
