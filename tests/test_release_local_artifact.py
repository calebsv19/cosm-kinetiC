"""Actual unsigned artifact recipe retains attempts and preserves final predecessors."""
import hashlib
import json
from pathlib import Path
import sys
import unittest
import test_release_audit as fixture

class LocalArtifact(unittest.TestCase):
    make=fixture.Audit.make
    def setUp(self):
        fixture.Audit.setUp(self)
        import shutil
        shutil.copy2(fixture.ROOT/'scripts/verify_release_local_artifact.py',self.repo/'scripts/verify_release_local_artifact.py')
        self.archive_tool=self.repo/'ditto'
        self.archive_tool.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nimport sys\nPath(sys.argv[-1]).write_bytes(b"fixture archive bytes")\n');self.archive_tool.chmod(0o755)
        self.hash_tool=self.repo/'shasum'
        self.hash_tool.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nimport hashlib,sys\nprint(hashlib.sha256(Path(sys.argv[-1]).read_bytes()).hexdigest()+"  "+sys.argv[-1])\n');self.hash_tool.chmod(0o755)
        with (self.repo/'makefile').open('a') as stream:
            stream.write('RELEASE_APP_ZIP=$(RELEASE_DIR)/artifact name.zip\nRELEASE_MANIFEST=$(RELEASE_DIR)/artifact manifest.txt\nRELEASE_DITTO='+str(self.archive_tool)+'\nRELEASE_SHASUM='+str(self.hash_tool)+'\nRELEASE_PRODUCT_NAME=fixture\nRELEASE_PROGRAM_KEY=physics_sim\nRELEASE_VERSION=test\nRELEASE_PLATFORM=fixture\nRELEASE_ARCH=fixture\nRELEASE_CHANNEL=local\n')
        self.release=self.repo/'build/release'

    def receipt(self):return next((self.release/'.package-transactions').glob('*/receipt.json'))

    def test_actual_recipe_publishes_portable_checksum_and_reuses_without_writes(self):
        first=self.make('release-local-artifact');self.assertEqual(first.returncode,0,first.stdout+first.stderr)
        archive=self.release/'artifact name.zip';before=(archive.read_bytes(),archive.stat().st_mtime_ns)
        checksum=(self.release/'artifact name.zip.sha256').read_text()
        self.assertEqual(checksum,hashlib.sha256(before[0]).hexdigest()+'  artifact name.zip\n')
        manifest=(self.release/'artifact manifest.txt').read_text();self.assertIn('signed=false\nnotarized=false',manifest)
        self.assertIn('artifact=artifact name.zip',manifest)
        second=self.make('release-local-artifact');self.assertEqual(second.returncode,0,second.stdout+second.stderr)
        self.assertIn('verified_reuse',second.stdout);self.assertEqual((archive.read_bytes(),archive.stat().st_mtime_ns),before)
        row=json.loads(self.receipt().read_text());self.assertEqual(row['state'],'completed');self.assertTrue(row['terminal_processes_verified'])
        self.assertEqual(len(row['published']),3)

    def test_transaction_stage_preserves_outer_package_source_with_real_path_recipe(self):
        import shutil
        shutil.copy2(fixture.ROOT/'make/package-paths.mk',self.repo/'make/package-paths.mk')
        source_root=self.repo/'build/source-package'
        source_root.mkdir(parents=True)
        moved=source_root/'kinetiC.app'
        shutil.move(self.app,moved)
        self.app=moved
        recipe=self.repo/'makefile'
        text=recipe.read_text()
        text=text.replace('package-desktop-self-test:\n',
            'DIST_DIR=$(if $(PHYSICS_SIM_DIST_ROOT),$(PHYSICS_SIM_DIST_ROOT),dist)\n'
            'RELEASE_ROOT='+str(source_root)+'\ninclude make/package-paths.mk\npackage-desktop-self-test:\n')
        recipe.write_text(text)
        self.archive_tool.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nimport sys\n'
            'source=Path(sys.argv[-2]);assert source.is_dir(),str(source)\n'
            'assert (source/"Contents/MacOS/physics-sim-bin").read_bytes()==b"fixture only"\n'
            'Path(sys.argv[-1]).write_bytes(b"fixture archive bytes")\n')
        result=self.make('release-local-artifact')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        archive=source_root/'artifact name.zip'
        self.assertEqual(archive.read_bytes(),b'fixture archive bytes')
        self.assertEqual((moved/'Contents/MacOS/physics-sim-bin').read_bytes(),b'fixture only')
        receipts=list((source_root/'.package-transactions').glob('*/receipt.json'))
        self.assertEqual(len(receipts),1)
        self.assertEqual(json.loads(receipts[0].read_text())['state'],'completed')

    def test_failed_archive_creation_retains_partial_stage_and_publishes_nothing(self):
        self.archive_tool.write_text('#!'+sys.executable+'\nfrom pathlib import Path\nimport sys\nPath(sys.argv[-1]).write_bytes(b"partial archive");print("archive failure",flush=True);raise SystemExit(7)\n');self.archive_tool.chmod(0o755)
        result=self.make('release-local-artifact');self.assertNotEqual(result.returncode,0)
        row=json.loads(self.receipt().read_text());self.assertEqual(row['state'],'failed_retained');self.assertEqual(row['published'],[])
        self.assertEqual((self.receipt().parent/'stage/artifact name.zip').read_bytes(),b'partial archive')
        self.assertFalse((self.release/'artifact name.zip').exists());self.assertFalse((self.release/'artifact manifest.txt').exists())

    def test_existing_archive_or_changed_package_holds_without_replacement(self):
        self.release.mkdir(parents=True);archive=self.release/'artifact name.zip';archive.write_bytes(b'unknown predecessor')
        result=self.make('release-local-artifact');self.assertNotEqual(result.returncode,0);self.assertEqual(archive.read_bytes(),b'unknown predecessor')
        self.assertFalse((self.release/'artifact manifest.txt').exists())

    def test_changed_package_inputs_prevent_completed_reuse(self):
        result=self.make('release-local-artifact');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        archive=self.release/'artifact name.zip';before=(archive.read_bytes(),archive.stat().st_mtime_ns)
        (self.app/'Contents/MacOS/physics-sim-bin').write_bytes(b'changed package bytes')
        result=self.make('release-local-artifact');self.assertNotEqual(result.returncode,0)
        self.assertEqual((archive.read_bytes(),archive.stat().st_mtime_ns),before)
        self.assertIn('inputs changed',result.stderr)

    def test_malformed_checksum_never_completes_or_publishes(self):
        self.hash_tool.write_text('#!/bin/sh\nprintf "%064d  %s\\n" 0 "$3"\n');self.hash_tool.chmod(0o755)
        result=self.make('release-local-artifact');self.assertNotEqual(result.returncode,0)
        row=json.loads(self.receipt().read_text());self.assertEqual(row['state'],'failed_retained');self.assertEqual(row['published'],[])
        self.assertFalse((self.release/'artifact name.zip').exists())
        self.assertIn('checksum or basename mismatch',(self.receipt().parent/'assembly.stderr').read_text())

    def test_selected_root_with_spaces_preserves_portable_checksum(self):
        root=self.repo/'build/release root'
        result=self.make('release-local-artifact','RELEASE_DIR='+str(root));self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        archive=root/'artifact name.zip'
        self.assertEqual((root/'artifact name.zip.sha256').read_text(),hashlib.sha256(archive.read_bytes()).hexdigest()+'  artifact name.zip\n')

    def test_manifest_corruption_is_rejected_by_identity_verifier(self):
        result=self.make('release-local-artifact');self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        sys.path.insert(0,str(fixture.ROOT/'scripts'))
        from verify_release_local_artifact import verify
        archive=self.release/'artifact name.zip';checksum=self.release/'artifact name.zip.sha256';manifest=self.release/'artifact manifest.txt'
        original=manifest.read_text()
        for changed in (original.replace('signed=false','signed=true'),original.replace('format=zip','format=tar'),original+'sha256=duplicate\n',original.replace('artifact=artifact name.zip','artifact=other.zip')):
            manifest.write_text(changed)
            with self.assertRaises(ValueError):verify(archive,checksum,manifest)
        manifest.write_text(original);self.assertEqual(verify(archive,checksum,manifest),hashlib.sha256(archive.read_bytes()).hexdigest())

    def test_direct_assembler_protected_override_holds_before_archive_tool(self):
        protected=self.repo/'src';protected.mkdir();sentinel=protected/'artifact.zip';sentinel.write_bytes(b'protected')
        result=self.make('_release-local-artifact','RELEASE_DIR='+str(protected),'RELEASE_APP_ZIP='+str(sentinel))
        self.assertNotEqual(result.returncode,0);self.assertEqual(sentinel.read_bytes(),b'protected')
        self.assertFalse((protected/'artifact manifest.txt').exists())

if __name__=='__main__':unittest.main()
