"""Session storage admission refuses links/source targets before mutation."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,SessionError,atomic,read,regular_lock
from session_paths import checked,root_path,asset_path


class Paths(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name).resolve();self.repo=self.base/'repo';self.repo.mkdir()
        (self.repo/'.git').write_text('fixture git marker')

    def test_source_root_ancestor_and_source_subtrees_are_held(self):
        for root in (self.repo,self.base,self.repo/'src/session',self.repo/'scripts/session',self.repo/'data/session'):
            with self.assertRaises(ValueError):root_path(root,self.repo)
        self.assertFalse((self.repo/'src').exists())

    def test_supported_generated_and_external_roots_are_admitted(self):
        for root in (self.repo/'build/session',self.repo/'tmp/session',self.repo/'data/runtime/sessions',self.repo/'data/experiments/proof',self.base/'external/sessions'):
            self.assertEqual(root_path(root,self.repo),root)
            self.assertFalse(root.exists())

    def test_root_leaf_or_parent_symlink_is_not_resolved(self):
        target=self.base/'target';target.mkdir();link=self.base/'link';link.symlink_to(target,target_is_directory=True)
        for root in (link,link/'new'):
            with self.assertRaisesRegex(ValueError,'symlink'):root_path(root,self.repo)
        self.assertEqual(list(target.iterdir()),[])

    def test_protected_app_git_and_other_checkout_are_held(self):
        foreign=self.base/'foreign';foreign.mkdir();(foreign/'.git').write_text('gitdir: fixture')
        for root in (self.base/'Fake.app/Contents/Resources/data/runtime',self.repo/'.git/session',foreign/'build/sessions'):
            with self.assertRaises(ValueError):root_path(root,self.repo)

    def test_bad_child_storage_and_lock_hold_before_other_allocation(self):
        root=self.base/'sessions';root.mkdir();outside=self.base/'outside';outside.mkdir()
        (root/'runs').symlink_to(outside,target_is_directory=True)
        with self.assertRaises(ValueError):Service(root)
        self.assertFalse((root/'scenes').exists());self.assertEqual(list(outside.iterdir()),[])
        (root/'runs').unlink();os.mkfifo(root/'service.lock')
        with self.assertRaises(SessionError):Service(root)
        self.assertFalse((root/'scenes').exists())

    def test_json_and_atomic_outputs_refuse_linked_or_special_paths(self):
        original=self.base/'original.json';original.write_bytes(b'{"old":true}')
        linked=self.base/'linked.json';linked.symlink_to(original)
        for operation in (lambda:read(linked),lambda:atomic(linked,{'new':True})):
            with self.assertRaises(ValueError):operation()
        fifo=self.base/'fifo';os.mkfifo(fifo)
        with self.assertRaises(SessionError):read(fifo)
        with self.assertRaises(SessionError):atomic(fifo,{'new':True})
        self.assertEqual(original.read_bytes(),b'{"old":true}')

    def test_live_storage_recheck_refuses_replaced_link(self):
        root=self.base/'sessions';service=Service(root);outside=self.base/'outside';outside.mkdir()
        (root/'runs').rmdir();(root/'runs').symlink_to(outside,target_is_directory=True)
        with self.assertRaises(ValueError):
            with service.lock():pass
        self.assertEqual((root/'service.lock').read_bytes(),b'');self.assertEqual(list(outside.iterdir()),[])

    def test_owner_locks_refuse_symlinks_and_fifo(self):
        root=self.base/'sessions';root.mkdir();target=self.base/'untouched';target.write_bytes(b'original')
        lock=root/'owner.lock';lock.symlink_to(target)
        with self.assertRaises(ValueError):Service.owner_alive(root)
        lock.unlink();os.mkfifo(lock)
        with self.assertRaises(SessionError):Service.owner_alive(root)
        self.assertEqual(target.read_bytes(),b'original')

    def test_assets_cannot_escape_or_follow_links(self):
        scene=self.base/'scene';scene.mkdir();outside=self.base/'outside';outside.write_bytes(b'original')
        for name in ('../outside',str(outside),'nested/../../outside',''):
            with self.assertRaises(ValueError):asset_path(scene,name)
        (scene/'linked').symlink_to(outside)
        with self.assertRaises(ValueError):asset_path(scene,'linked')
        self.assertEqual(asset_path(scene,'assets/shape.stl'),scene/'assets/shape.stl')
        self.assertFalse((scene/'assets').exists())

    def test_symlinked_run_does_not_read_external_request(self):
        root=self.base/'sessions';service=Service(root);external=self.base/'external';external.mkdir()
        (external/'request.json').write_text('{}');(root/'runs/run').symlink_to(external,target_is_directory=True)
        with self.assertRaises(ValueError):service.run_dir('run')
        self.assertEqual(list(external.iterdir()),[external/'request.json'])

    def test_normal_regular_json_and_lock_work_without_side_effects_elsewhere(self):
        selected=self.base/'state.json';atomic(selected,{'value':1,'quoted':'[\"{]'});self.assertEqual(read(selected),{'value':1,'quoted':'[\"{]'})
        lock=self.base/'owner.lock'
        with regular_lock(lock):pass
        self.assertTrue(lock.is_file());self.assertEqual(lock.read_bytes(),b'')

    def test_broad_system_and_user_roots_are_held(self):
        for selected in (Path('/private/tmp'),Path('/System/test-session'),Path('/Applications/test-session'),Path.home()/'Library',Path.home()/'Desktop'):
            with self.assertRaises(ValueError):root_path(selected,self.repo)

    def test_duplicate_nonfinite_deep_and_oversize_json_are_held(self):
        selected=self.base/'state.json'
        for content in ('{"run_id":"first","run_id":"second"}','{"x":NaN}','['*2000+'0'+']'*2000):
            selected.write_text(content)
            with self.assertRaises((ValueError,SessionError)):read(selected)
        with selected.open('wb') as stream:stream.truncate(16777217)
        with self.assertRaises(SessionError):read(selected)

    def test_live_real_directory_replacement_is_held_by_descriptor_identity(self):
        for name in ('root','scenes','runs'):
            root=self.base/('sessions-'+name);service=Service(root)
            selected=root if name=='root' else root/name
            retained=selected.with_name(selected.name+'-retained');selected.rename(retained);selected.mkdir()
            sentinel=selected/'keep';sentinel.write_bytes(b'replacement directory')
            with self.assertRaisesRegex(SessionError,'identity changed'):
                with service.lock():pass
            with self.assertRaisesRegex(SessionError,'identity changed'):service.capabilities()
            self.assertEqual(sentinel.read_bytes(),b'replacement directory')
            self.assertTrue(retained.exists());service.close()

    def test_replaced_regular_lock_is_held_and_not_used(self):
        root=self.base/'sessions';service=Service(root);lock=root/'service.lock'
        retained=root/'old-service.lock';lock.rename(retained);lock.write_bytes(b'replacement lock')
        with self.assertRaisesRegex(SessionError,'identity changed'):
            with service.lock():pass
        self.assertEqual(lock.read_bytes(),b'replacement lock');self.assertEqual(retained.read_bytes(),b'')

    def test_close_releases_witnesses_and_client_cannot_resume(self):
        root=self.base/'sessions';service=Service(root);descriptors=list(service._storage_descriptors.values())
        self.assertEqual(len(descriptors),4);service.close();service.close()
        for descriptor in descriptors:
            with self.assertRaises(OSError):os.fstat(descriptor)
        with self.assertRaisesRegex(SessionError,'closed'):service.capabilities()
        with self.assertRaisesRegex(SessionError,'closed'):
            with service.lock():pass
        reattached=Service(root);self.assertEqual(reattached.capabilities()['root'],str(root));reattached.close()

    def test_service_lock_rechecks_identity_after_body(self):
        root=self.base/'sessions';service=Service(root)
        with self.assertRaisesRegex(SessionError,'identity changed'):
            with service.lock():
                (root/'runs').rename(root/'retained-runs');(root/'runs').mkdir()
        self.assertTrue((root/'retained-runs').exists())

    def test_public_read_rechecks_storage_after_body(self):
        root=self.base/'sessions';service=Service(root);run=root/'runs/run';run.mkdir();(run/'request.json').write_text('{}')
        def replaced_status(path):
            (root/'runs').rename(root/'retained-runs');(root/'runs').mkdir()
            return {'state':'completed'}
        with patch.object(service,'_status',side_effect=replaced_status):
            with self.assertRaisesRegex(SessionError,'identity changed'):service.run_inspect('run')
        self.assertTrue((root/'retained-runs/run/request.json').exists())
        self.assertEqual(list((root/'runs').iterdir()),[])

    def test_root_named_like_lock_is_a_directory_and_witnesses_are_not_inherited(self):
        root=self.base/'service.lock';service=Service(root)
        self.assertEqual(service.capabilities()['root'],str(root))
        for descriptor in service._storage_descriptors.values():self.assertFalse(os.get_inheritable(descriptor))
        service.close()

    def test_path_traversal_is_held_and_known_system_alias_is_explicit(self):
        with self.assertRaises(ValueError):checked(self.base/'x/../y')
        if Path('/tmp').is_symlink():
            self.assertEqual(checked('/tmp/physics-session-fixture'),Path('/private/tmp/physics-session-fixture'))

if __name__=='__main__':unittest.main()
