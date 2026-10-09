"""Package lifetime admission preserves unknown, failed and authenticated artifacts."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
sys.path.insert(0,str(ROOT/'scripts'))
from package_outputs import plan,declare
from check_clean_root import check

class PackageOutputs(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();self.root=self.repo/'build/attempt'

    def test_complete_set_existing_archive_refuses_before_directory_creation(self):
        self.root.mkdir(parents=True);archive=self.root/'worker.tar.gz';archive.write_bytes(b'predecessor')
        stage=self.root/'worker';validate=lambda:plan(self.repo,self.root,[stage],[archive])
        before=list(self.root.iterdir())
        with self.assertRaises(ValueError):declare(validate(),validate)
        self.assertFalse(stage.exists());self.assertEqual(archive.read_bytes(),b'predecessor')
        self.assertEqual(list(self.root.iterdir()),before)

    def test_signed_failed_and_unknown_predecessors_are_all_retained(self):
        for content in ('{"signed":true}','{"status":"failed"}','unknown old contents'):
            with self.subTest(content=content):
                stage=self.root/'worker';stage.mkdir(parents=True,exist_ok=True);p=stage/'proof';p.write_text(content)
                validate=lambda:plan(self.repo,self.root,[stage],[])
                with self.assertRaises(ValueError):declare(validate(),validate)
                self.assertEqual(p.read_text(),content)

    def test_fresh_stage_and_file_reservations_protect_custom_build_namespace(self):
        stage=self.root/'worker';archive=self.root/'worker.tar.gz'
        validate=lambda:plan(self.repo,self.root,[stage],[archive])
        declared=declare(validate(),validate)
        self.assertEqual(declared['status'],'fresh_outputs_admitted')
        self.assertEqual(list(stage.iterdir()),[])
        self.assertTrue(list((self.root/'.package-reservations').glob('*.json')))
        with self.assertRaises(ValueError):check(self.repo,stage,[])
        with self.assertRaises(ValueError):check(self.repo,self.root,[])
        with self.assertRaises(ValueError):declare(validate(),validate)
        other=self.root/'other.zip';other_validate=lambda:plan(self.repo,self.root,[],[other])
        declare(other_validate(),other_validate)
        self.assertFalse(other.exists())
        self.assertFalse(other_validate()['fresh'])

    def test_escape_symlink_and_source_roots_refused_without_writes(self):
        (self.repo/'build').mkdir();(self.repo/'build/alias').symlink_to(self.repo/'src')
        for root,output in [(self.repo/'src',self.repo/'src/item'),(self.repo/'build',self.repo/'build/item'),(self.repo/'build/bin',self.repo/'build/bin/item'),(self.repo/'build/alias',self.repo/'build/alias/item'),(self.root,self.repo/'data/experiments')]:
            with self.assertRaises(ValueError):plan(self.repo,root,[output],[])
        self.assertFalse((self.repo/'src').exists());self.assertFalse(self.root.exists())

    def test_changed_plan_refuses_before_reserving_any_output(self):
        stage=self.root/'worker';validate=lambda:plan(self.repo,self.root,[stage],[])
        prepared=validate();stage.mkdir(parents=True);(stage/'changed').write_bytes(b'preserved')
        with self.assertRaises(ValueError):declare(prepared,validate)
        self.assertEqual((stage/'changed').read_bytes(),b'preserved')
        self.assertFalse((self.root/'.package-reservations').exists())

    def test_unique_comparison_directories_do_not_touch_candidate(self):
        (self.repo/'scripts').mkdir()
        for name in ('package_outputs.py','package_paths.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        self.root.mkdir(parents=True);candidate=self.root/'accepted.tar.gz';candidate.write_bytes(b'accepted')
        allocated=[]
        for _ in range(2):
            result=subprocess.run([sys.executable,'-B','scripts/package_outputs.py','--root',str(self.root),'--allocate','desktop-determinism'],cwd=self.repo,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr);allocated.append(Path(result.stdout.strip()))
        self.assertNotEqual(allocated[0],allocated[1]);self.assertEqual(candidate.read_bytes(),b'accepted')
        self.assertTrue(all(p.is_dir() and list(p.iterdir())==[] for p in allocated))

    def test_actual_linux_clean_recipes_preserve_package_and_archive(self):
        (self.repo/'scripts').mkdir();(self.repo/'make').mkdir()
        for name in ('package_outputs.py','package_paths.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        for name in ('package-linux-worker.mk','package-linux-desktop.mk'):shutil.copy2(ROOT/'make'/name,self.repo/'make'/name)
        (self.repo/'Makefile').write_text('RELEASE_DIR=build/release\nRELEASE_PROGRAM_KEY=physics_sim\nRELEASE_PRODUCT_NAME=kinetiC\nRELEASE_VERSION=test\nWORKER_VERSION=test\nRELEASE_CHANNEL=stable\ninclude make/package-linux-worker.mk\ninclude make/package-linux-desktop.mk\n')
        stage=self.repo/'build/release';stage.mkdir(parents=True)
        for target,variable in [('package-linux-worker-clean','LINUX_WORKER_DIR'),('package-linux-desktop-clean','LINUX_DESKTOP_DIR')]:
            held=stage/target;held.mkdir();receipt=held/'proof.json';receipt.write_text('{"signed":true}')
            result=subprocess.run(['make',target,variable+'='+str(held)],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=10)
            self.assertNotEqual(result.returncode,0);self.assertEqual(receipt.read_text(),'{"signed":true}')

    def test_actual_release_clean_recipe_preserves_predecessor_and_bad_override(self):
        (self.repo/'scripts').mkdir();(self.repo/'make').mkdir()
        for name in ('package_outputs.py','package_paths.py','build_outputs.py','clean_outputs.py','check_clean_root.py'):shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        shutil.copy2(ROOT/'make/release.mk',self.repo/'make/release.mk')
        (self.repo/'Makefile').write_text('RELEASE_DIR=build/release\ninclude make/release.mk\n')
        stage=self.repo/'build/release';stage.mkdir(parents=True);p=stage/'accepted.zip';p.write_bytes(b'preserved')
        for args in (['release-clean'],['release-clean','RELEASE_DIR=src']):
            result=subprocess.run(['make',*args],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=10)
            self.assertNotEqual(result.returncode,0)
            self.assertEqual(p.read_bytes(),b'preserved')

if __name__=='__main__':unittest.main()

class BoundDataOutputs(unittest.TestCase):
    def setUp(self):
        from unittest.mock import patch
        import package_paths
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name).resolve()
        self.repo=self.base/'source/physics_sim';self.repo.mkdir(parents=True)
        self.data=self.base/'data';self.data.mkdir()
        self.patch=patch.object(package_paths,'DATA_ROOT',self.data);self.patch.start();self.addCleanup(self.patch.stop)
        self.selected='physics_sim/build/release-authenticated/raor_'+'a'*64
        self.root=self.data/self.selected/'targets'/('rapt_'+package_paths.digest({'package_target':'release-local-artifact'}))
        self.contract={'schema_version':'production-registry/release-authorization-precommit-preparation-contract/v1',
            'program':'physics_sim','source_data_roots':{'source_workspace_root':str(self.repo.parent),'data_workspace_root':str(self.data)},
            'owner_adapter_binding':{'repository_path':'physics_sim','package_targets':['release-local-artifact']},
            'output_root_binding':{'selected_root':self.selected,'candidate_scope_id':'raor_'+'a'*64}}
        store=self.data/package_paths.CONTRACTS;store.mkdir(parents=True)
        self.record=store/(package_paths.digest(self.contract)+'.json');self.record.write_text(json.dumps(self.contract))

    def test_exact_target_admission_and_reservation_preserve_cleanup_boundary(self):
        from clean_outputs import no_symlinks
        target=self.root/'kinetiC.app';validate=lambda:plan(self.repo,self.root,[target],[])
        result=declare(validate(),validate)
        self.assertEqual(result['status'],'fresh_outputs_admitted')
        self.assertFalse(validate()['fresh'])
        with self.assertRaises(ValueError):no_symlinks(target,self.repo)

    def test_unbound_broader_wrong_target_and_escape_refused(self):
        for root in [self.data/self.selected,self.root.parent,self.root.parent/('rapt_'+'b'*64),self.data/'other/build/attempt']:
            with self.subTest(root=root),self.assertRaises(ValueError):plan(self.repo,root,[root/'app'],[])
        with self.assertRaises(ValueError):plan(self.repo,self.root,[self.root.parent/'escaped'],[])
        with self.assertRaises(ValueError):plan(self.repo,self.root/'../alias',[self.root/'app'],[])
        self.assertFalse(self.root.exists())

    def test_tampered_contract_wrong_source_and_symlink_refused(self):
        self.record.write_text('{}')
        with self.assertRaises(ValueError):plan(self.repo,self.root,[self.root/'app'],[])
        self.record.write_text(json.dumps(self.contract))
        with self.assertRaises(ValueError):plan(self.base/'another/physics_sim',self.root,[self.root/'app'],[])
        self.root.parent.mkdir(parents=True);self.root.symlink_to(self.repo)
        with self.assertRaises(ValueError):plan(self.repo,self.root,[self.root/'app'],[])

    def test_symlinked_contract_and_store_refused(self):
        external=self.base/'contract.json';external.write_text(self.record.read_text())
        self.record.unlink();self.record.symlink_to(external)
        with self.assertRaises(ValueError):plan(self.repo,self.root,[self.root/'app'],[])
        self.record.unlink();store=self.record.parent;store.rmdir();store.symlink_to(self.base)
        with self.assertRaises(ValueError):plan(self.repo,self.root,[self.root/'app'],[])

    def test_release_assembly_selects_its_own_desktop_stage(self):
        makefile=self.repo/'makefile'
        makefile.write_text('DIST_DIR=dist/gate\nRELEASE_ROOT=build/fresh-release\ninclude '+str(ROOT/'make/package-paths.mk')+'\nrelease-local-artifact:\n\t@printf "%s\\n" "$(PACKAGE_APP_DIR)" "$(PHYSICS_SIM_DIST_ROOT)"\nordinary:\n\t@printf "%s\\n" "$(PACKAGE_APP_DIR)"\n')
        release=subprocess.run(['make','release-local-artifact'],cwd=self.repo,env=ENV,capture_output=True,text=True)
        self.assertEqual(release.returncode,0,release.stderr)
        self.assertEqual(release.stdout.splitlines(),['build/fresh-release/kinetiC.app','build/fresh-release'])
        ordinary=subprocess.run(['make','ordinary'],cwd=self.repo,env=ENV,capture_output=True,text=True)
        self.assertEqual(ordinary.returncode,0,ordinary.stderr)
        self.assertEqual(ordinary.stdout.strip(),'dist/gate/kinetiC.app')
