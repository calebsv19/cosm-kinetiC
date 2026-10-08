"""Actual cache status admission over native-published and adversarial metadata."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import unittest
import test_cache_publish_transaction as native
ROOT=native.ROOT

class CacheStatusAdmission(native.CacheTransaction):
    @classmethod
    def setUpClass(cls):
        super().setUpClass();base=Path(cls.t.name);probe=base/'probe.c';source=probe.read_text()
        source=source.replace('return scene_project_cache_output_status_from_project(argv[1],&status,error,sizeof(error))?0:2;',r'''status.active_cache_ready=true;int ok=scene_project_cache_output_status_from_project(argv[1],&status,error,sizeof(error));printf("{\"ready\":%s,\"frames\":%d}\n",status.active_cache_ready?"true":"false",status.frame_count);return ok?0:2;''')
        probe.write_text(source)
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','json-c'],text=True))
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(probe),str(base/'faults.c'),str(base/'cache.o'),str(ROOT/'src/app/physics_sim_json_helpers.c'),str(ROOT/'src/app/physics_sim_job_json.c'),*flags,'-o',str(cls.binary)],check=True)
    def published(self):
        result=self.invoke();self.assertEqual(result.returncode,0,result.stdout)
        self.active=self.project/'physics_sim/active_cache_manifest.json';self.compat=self.project/'physics_sim/cache_manifest.json'
        self.original=self.active.read_bytes();self.manifest=json.loads(self.original)
    def write_both(self,row):
        text=json.dumps(row);self.active.write_text(text);self.compat.write_text(text)
    def status(self,expected=0,ready=True):
        result=self.invoke(mode='status');self.assertEqual(result.returncode,expected,result.stdout)
        value=json.loads(result.stdout);self.assertEqual(value['ready'],ready)
        if expected:self.assertEqual(value['frames'],0)
        return value
    def test_valid_native_manifest_and_compatibility_only_preserve_status(self):
        self.published();self.assertEqual(self.status()['frames'],1);self.active.unlink();self.status()
    def test_no_cache_and_zero_frame_cache_never_report_ready(self):
        self.status(ready=False);self.published();row=dict(self.manifest);row['frame_count']=0;row['retained_frame_indices']=[];self.write_both(row);self.status(ready=False)
    def test_malformed_trailing_duplicate_nested_decoy_and_nul_hold(self):
        self.published()
        cases=['{',self.original.decode()+' garbage','[]','null',self.original.decode().replace('"frame_count": 1','"frame_count": 1,"frame_count": 2'),json.dumps({'decoy':self.manifest}),self.original.decode().replace('"active_run_id": "run-good"','"active_run_id": "run-good\\u0000bad"')]
        for text in cases:
            with self.subTest(text=text[:50]):self.active.write_text(text);self.compat.write_text(text);self.status(2,False)
    def test_invalid_utf8_depth_and_manifest_size_hold(self):
        self.published()
        for text in (b'{"x":"\xff"}',b'{"x":'+b'['*65+b'0'+b']'*65+b'}',self.original+b' '*(1024*1024)):
            self.active.write_bytes(text);self.compat.write_bytes(text);self.status(2,False)
    def test_wrong_schema_missing_and_typed_frame_fields_hold(self):
        self.published()
        for key in ('schema','active_run_id','frame_count','export_start_frame','export_stride','export_max_frames'):
            row=dict(self.manifest);row.pop(key);self.write_both(row);self.status(2,False)
        for key,value in (('schema','wrong'),('frame_count',True),('frame_count',1.5),('frame_count',2147483648),('frame_count',-1),('export_stride',0),('export_max_frames',-1),('active_run_id','../outside'),('active_run_id','x'*64)):
            row=dict(self.manifest);row[key]=value;self.write_both(row);self.status(2,False)
    def test_absolute_traversal_or_alternate_manifest_paths_hold(self):
        self.published()
        for key in ('vf3d_active_dir','physics_active_dir','scene_bundle'):
            for value in ('../outside','/tmp/outside','assets/other','assets/vf3d/active/../active'):
                row=dict(self.manifest);row[key]=value;self.write_both(row);self.status(2,False)
    def test_conflicting_active_and_compatibility_metadata_hold(self):
        self.published();row=dict(self.manifest);row['active_run_id']='different';self.compat.write_text(json.dumps(row));self.status(2,False)
    def test_invalid_retained_indices_and_escaped_valid_keys(self):
        self.published()
        for indices in ([1],[1,0],[True],[-1],['0'],[],[2147483648]):
            row=dict(self.manifest);row['retained_frame_indices']=indices;self.write_both(row);self.status(2,False)
        text=self.original.decode().replace('"frame_count"','"frame\\u005fcount"');self.active.write_text(text);self.compat.write_text(text);self.status()
    def test_linked_manifests_artifacts_and_source_inputs_hold(self):
        self.published();foreign=self.base/'foreign';foreign.write_bytes(self.original)
        for target in (self.active,self.compat,self.project/'assets/physics/active/scene_bundle.json',self.project/'scene_runtime.json'):
            original=target.read_bytes();target.unlink();target.symlink_to(foreign);self.status(2,False);target.unlink();target.write_bytes(original)
        nested=self.project/'assets/vf3d/active/linked.vf3d';nested.symlink_to(foreign);self.status(2,False)
    def test_special_and_hardlinked_cache_entries_hold(self):
        self.published();foreign=self.base/'foreign';foreign.write_bytes(b'bytes');selected=self.project/'assets/vf3d/active/extra.pack'
        os.mkfifo(selected);self.status(2,False);selected.unlink();os.link(foreign,selected);self.status(2,False)

if __name__=='__main__':unittest.main()
