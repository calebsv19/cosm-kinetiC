"""Native cache path/ID holds occur before the existing overwrite sequence."""
import os
from pathlib import Path
import subprocess
import shlex
import tempfile
import unittest
from cache_fixture import write_cache, frame_bytes
ROOT=Path(__file__).resolve().parents[1]
class CacheAdmission(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);src=base/'probe.c'
        src.write_text(r"""
#include "app/scene_project_cache_output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){if(argc<5)return 9;char error[256],out[1024];
if(!strcmp(argv[4],"id"))return scene_project_cache_output_make_run_id(out,sizeof(out),NULL,0)?0:2;
if(!strcmp(argv[4],"default"))return scene_project_cache_output_default_run_root(argv[1],argv[3],out,sizeof(out))?0:2;
SceneProjectCacheOutputResolved project={0};snprintf(project.project_root,sizeof(project.project_root),"%s",argv[1]);
SceneProjectCacheOutputPublishRequest request={.project=&project,.run_id=argv[3],.run_output_root=argv[2],.allow_overwrite=true,.source_frame_count=1,.frame_count=1,.export_stride=1,.export_max_frames=1};
if(argc>5)request.source_frame_count=atoi(argv[5]);
if(argc>6)request.export_start_frame=atoi(argv[6]);
if(argc>7)request.export_stride=atoi(argv[7]);
if(argc>8)request.export_max_frames=atoi(argv[8]);
if(argc>9)request.frame_count=atoi(argv[9]);
int ok=scene_project_cache_output_publish(&request,error,sizeof(error));if(!ok)puts(error);return ok?0:2;}
""")
        cls.binary=base/'probe'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(src),str(ROOT/'src/app/scene_project_cache_output.c'),str(ROOT/'src/app/physics_sim_json_helpers.c'),str(ROOT/'src/app/physics_sim_job_json.c'),*shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','json-c'],text=True)),'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name);self.project=self.base/'project';self.project.mkdir()
        self.run=self.project/'physics_sim/runs/run-good';self.source=self.run/'volume_frames/sample';self.source.mkdir(parents=True)
        write_cache(self.source)
    def invoke(self,id='run-good',mode='publish',project=None,run=None,env=None):
        return subprocess.run([str(self.binary),str(project or self.project),str(run or self.run),id,mode],env=env,capture_output=True,text=True,timeout=5)
    def slots(self):return [self.project/'assets/vf3d/runs/run-good',self.project/'assets/vf3d/active',self.project/'assets/physics/runs/run-good',self.project/'assets/physics/active']
    def retained(self):
        sentinels=[]
        for slot in self.slots():slot.mkdir(parents=True);p=slot/'retained';p.write_bytes(b'previous');sentinels.append(p)
        return sentinels
    def assert_retained(self,paths):self.assertTrue(all(p.read_bytes()==b'previous' for p in paths))
    def test_valid_single_component_publishes_original_layout(self):
        result=self.invoke();self.assertEqual(result.returncode,0,result.stdout)
        self.assertEqual((self.slots()[1]/'frame_000000.vf3d').read_bytes(),frame_bytes())
        self.assertTrue((self.project/'physics_sim/active_cache_manifest.json').is_file())
    def test_invalid_ids_hold_publish_default_and_environment_override(self):
        sentinels=self.retained()
        for id in ('../..','../outside','a/b','a\\b','..','.','-hidden','a\nline','雪','x'*64):
            with self.subTest(id=id):
                self.assertEqual(self.invoke(id).returncode,2);self.assertEqual(self.invoke(id,'default').returncode,2)
                env=os.environ.copy();env['PHYSICS_SIM_PROJECT_CACHE_RUN_ID']=id
                self.assertEqual(self.invoke(id,'id',env=env).returncode,2)
                self.assert_retained(sentinels)
    def test_valid_environment_identifier_is_preserved(self):
        env=os.environ.copy();env['PHYSICS_SIM_PROJECT_CACHE_RUN_ID']='physics-run_20261007.1'
        self.assertEqual(self.invoke(mode='id',env=env).returncode,0)
    def test_linked_project_or_asset_ancestor_never_mutates_target(self):
        sentinels=self.retained();linked=self.base/'linked';linked.symlink_to(self.project)
        self.assertEqual(self.invoke(project=linked).returncode,2);self.assert_retained(sentinels)
        assets=self.project/'assets';assets.rename(self.project/'old-assets');assets.symlink_to(self.project/'old-assets')
        self.assertEqual(self.invoke().returncode,2);self.assert_retained([self.project/'old-assets'/p.relative_to(assets) for p in sentinels])
    def test_late_invalid_target_holds_before_any_prior_slot_removal(self):
        sentinels=self.retained();fifo=self.slots()[-1]/'fifo';os.mkfifo(fifo)
        self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
    def test_linked_selected_source_or_volume_parent_holds(self):
        sentinels=self.retained();foreign=self.base/'foreign';foreign.write_text('secret');(self.source/'frame_000000.vf3d').unlink();(self.source/'frame_000000.vf3d').symlink_to(foreign)
        self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels);self.assertEqual(foreign.read_text(),'secret')
        volume=self.run/'volume_frames';volume.rename(self.run/'old-volume');volume.symlink_to(self.run/'old-volume')
        self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
    def test_all_manifest_destinations_are_admitted_before_removal(self):
        sentinels=self.retained();foreign=self.base/'foreign';foreign.write_text('previous')
        targets=[self.project/'physics_sim/active_cache_manifest.json',self.project/'physics_sim/cache_manifest.json',self.run/'cache_manifest.json']
        for target in targets:
            target.symlink_to(foreign);self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels);target.unlink()
        self.assertEqual(foreign.read_text(),'previous')
    def test_source_inside_replacement_slot_is_held(self):
        sentinels=self.retained();run=self.slots()[1]/'input';source=run/'volume_frames/sample';source.mkdir(parents=True);(source/'frame.vf3d').write_text('frame')
        self.assertEqual(self.invoke(run=run).returncode,2);self.assert_retained(sentinels);self.assertEqual((source/'frame.vf3d').read_text(),'frame')
    def test_depth_bound_and_hardlinked_retained_file_hold(self):
        sentinels=self.retained();deep=self.slots()[-1]
        for i in range(17):deep=deep/str(i);deep.mkdir()
        self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
        # Separate fresh slot subtree avoids relying on the prior depth hold.
        import shutil
        shutil.rmtree(self.slots()[-1]);self.slots()[-1].mkdir();foreign=self.base/'foreign';foreign.write_text('previous');os.link(foreign,self.slots()[-1]/'hard')
        self.assertEqual(self.invoke().returncode,2);self.assertEqual(foreign.read_text(),'previous');self.assert_retained(sentinels[:-1])
    def test_protected_project_root_is_held(self):
        self.assertEqual(self.invoke(project=Path('/usr')).returncode,2)
if __name__=='__main__':unittest.main()
