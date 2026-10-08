"""Package/Desktop inventory bounds and observed filesystem drift."""
import hashlib
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from desktop_replace import inventory, InventoryBudget
from package_transaction import input_identity, output_inventory

class Inventory(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve()/'bundle';self.root.mkdir()

    def test_exact_bytes_modes_empty_directories_and_internal_links(self):
        (self.root/'empty').mkdir();file=self.root/'file';file.write_bytes(b'payload');file.chmod(0o640)
        (self.root/'link').symlink_to('file')
        row=inventory(self.root)
        self.assertEqual(row['file'],{'kind':'file','bytes':7,'mode':0o640,'sha256':hashlib.sha256(b'payload').hexdigest()})
        self.assertEqual(row['link']['target'],'file');self.assertEqual(row['empty']['kind'],'directory')
        self.assertEqual(inventory(file)['.']['sha256'],row['file']['sha256'])

    def test_enumeration_depth_and_byte_limits_hold(self):
        for n in range(5):(self.root/str(n)).write_bytes(b'1234')
        with self.assertRaisesRegex(ValueError,'entry bound'):inventory(self.root,max_entries=3)
        with self.assertRaisesRegex(ValueError,'byte bound'):inventory(self.root,max_file_bytes=3)
        with self.assertRaisesRegex(ValueError,'byte bound'):inventory(self.root,max_bytes=6)
        (self.root/'nested/deeper').mkdir(parents=True)
        with self.assertRaisesRegex(ValueError,'depth bound'):inventory(self.root,max_depth=1)

    def test_wall_and_invalid_limits_hold(self):
        with patch('desktop_replace.time.monotonic',side_effect=(0,2)):
            with self.assertRaisesRegex(ValueError,'wall bound'):inventory(self.root,wall_cap=1)
        for options in ({'max_entries':0},{'max_entries':True},{'wall_cap':float('nan')},{'max_bytes':0}):
            with self.assertRaises(ValueError):inventory(self.root,**options)

    def test_file_mutation_during_hashing_is_refused(self):
        file=self.root/'file';file.write_bytes(b'before');real_read=os.read;changed=False
        def mutate(fd,size):
            nonlocal changed
            block=real_read(fd,size)
            if block and not changed:changed=True;file.write_bytes(b'after')
            return block
        with patch('desktop_replace.os.read',side_effect=mutate):
            with self.assertRaisesRegex(ValueError,'changed during hashing'):inventory(self.root)

    def test_directory_addition_after_enumeration_is_refused(self):
        file=self.root/'file';file.write_bytes(b'before');real_read=os.read;changed=False
        def mutate(fd,size):
            nonlocal changed
            block=real_read(fd,size)
            if block and not changed:changed=True;(self.root/'late').write_bytes(b'late')
            return block
        with patch('desktop_replace.os.read',side_effect=mutate):
            with self.assertRaisesRegex(ValueError,'changed during readback'):inventory(self.root)

    def test_symlink_escape_and_special_file_are_never_opened(self):
        (self.root/'escape').symlink_to(self.root.parent/'outside')
        with self.assertRaisesRegex(ValueError,'escapes'):inventory(self.root)
        (self.root/'escape').unlink();os.mkfifo(self.root/'fifo')
        with self.assertRaisesRegex(ValueError,'special file'):inventory(self.root)

    def test_regular_file_replaced_by_fifo_before_open_never_blocks(self):
        file=self.root/'file';file.write_bytes(b'before');real_open=os.open
        def replace(path,flags,*args,**kwargs):
            if Path(path).name==file.name and not flags & os.O_DIRECTORY:file.unlink();os.mkfifo(file)
            return real_open(path,flags,*args,**kwargs)
        with patch('desktop_replace.os.open',side_effect=replace):
            with self.assertRaisesRegex(ValueError,'changed during admission'):inventory(self.root)

    def test_parent_swapped_to_external_symlink_before_open_cannot_hash_foreign_bytes(self):
        file=self.root/'file';file.write_bytes(b'owned')
        foreign=self.root.parent/'foreign';foreign.mkdir();(foreign/'file').write_bytes(b'foreign')
        original=self.root.parent/'original';real_open=os.open;changed=False;root_opens=0
        def swap(path,flags,*args,**kwargs):
            nonlocal changed,root_opens
            if Path(path).name==self.root.name and 'dir_fd' in kwargs:root_opens+=1
            if root_opens==2 and Path(path).name==self.root.name and not changed:
                changed=True;self.root.rename(original);self.root.symlink_to(foreign,target_is_directory=True)
            return real_open(path,flags,*args,**kwargs)
        with patch('desktop_replace.os.open',side_effect=swap),patch('desktop_replace.os.read',side_effect=AssertionError('Foreign bytes must not be read')):
            with self.assertRaises((OSError,ValueError)):inventory(self.root)
        self.assertEqual((foreign/'file').read_bytes(),b'foreign');self.assertEqual((original/'file').read_bytes(),b'owned')

    def test_combined_inputs_and_outputs_exhaust_one_shared_budget(self):
        first=self.root/'first';second=self.root/'second'
        first.write_bytes(b'1234');second.write_bytes(b'5678')
        self.assertEqual(inventory(first,max_bytes=6)['.']['bytes'],4)
        self.assertEqual(inventory(second,max_bytes=6)['.']['bytes'],4)
        with self.assertRaisesRegex(ValueError,'byte bound'):
            input_identity([first,second],[],budget=InventoryBudget(max_bytes=6))
        outputs=[{'relative':name,'kind':'file'} for name in ('first','second')]
        with self.assertRaisesRegex(ValueError,'byte bound'):
            output_inventory(self.root,outputs,budget=InventoryBudget(max_bytes=6))
        shared=output_inventory(self.root,outputs,budget=InventoryBudget(max_bytes=8))
        self.assertEqual(shared,{'first':inventory(first),'second':inventory(second)})

    def test_missing_inputs_and_separate_roots_share_entry_budget(self):
        missing=[self.root/str(n) for n in range(3)]
        with self.assertRaisesRegex(ValueError,'entry bound'):
            input_identity(missing,[],budget=InventoryBudget(max_entries=2))
        first=self.root/'first';second=self.root/'second';first.mkdir();second.mkdir()
        budget=InventoryBudget(max_entries=1)
        inventory(first,budget=budget)
        with self.assertRaisesRegex(ValueError,'entry bound'):inventory(second,budget=budget)

    def test_second_root_cannot_reset_shared_deadline(self):
        with patch('desktop_replace.time.monotonic',return_value=0):
            budget=InventoryBudget(wall_cap=1);inventory(self.root,budget=budget)
        with patch('desktop_replace.time.monotonic',return_value=2):
            with self.assertRaisesRegex(ValueError,'wall bound'):inventory(self.root,budget=budget)

if __name__=='__main__':unittest.main()
