"""Streaming VF3D payload admission against actual native publication/status."""
import ctypes
import json
import os
import struct
import subprocess
import unittest
import test_cache_inventory as inventory
from cache_fixture import Header,frame_bytes

def mask_hash(mask):
    value=2166136261
    for byte in mask:value=((value^byte)*16777619)&0xffffffff
    return value

class CachePayload(inventory.CacheInventory):
    def original_slots(self):
        self.predecessors();return [p/'retained' for p in self.slots()]
    def assert_originals(self,sentinels):
        self.assert_retained(sentinels)
        for target in self.targets()[4:]:self.assertEqual(target.read_bytes(),b'previous-manifest')
    def large(self,cells=8193):
        selected=self.source/'frame_000000.vf3d';header=Header.from_buffer_copy(frame_bytes()[:ctypes.sizeof(Header)])
        header.w=cells;mask=bytes(i%256 for i in range(cells));header.crc=mask_hash(mask)
        payload=struct.pack('@'+str(cells*5)+'f',*([0.125]*(cells*5)))+mask
        selected.write_bytes(bytes(header)+payload)
        row=json.loads((self.source/'manifest.json').read_text());row['grid_w']=cells;(self.source/'manifest.json').write_text(json.dumps(row))
        return selected,cells
    def test_nonfinite_each_field_holds_all_predecessors_before_attempt(self):
        sentinels=self.original_slots();selected=self.source/'frame_000000.vf3d';valid=frame_bytes();start=ctypes.sizeof(Header)
        for field in range(5):
            for value in (float('nan'),float('inf'),float('-inf')):
                with self.subTest(field=field,value=value):
                    data=bytearray(valid);struct.pack_into('@f',data,start+field*4,value);selected.write_bytes(data)
                    self.assertEqual(self.invoke().returncode,2);self.assert_originals(sentinels);self.assertEqual(self.attempts(),[])
    def test_finite_signed_extreme_field_values_remain_valid(self):
        data=bytearray(frame_bytes());struct.pack_into('@5f',data,ctypes.sizeof(Header),-100.0,3.4e38,-3.4e38,0.0,1e-30)
        (self.source/'frame_000000.vf3d').write_bytes(data);self.assertEqual(self.invoke().returncode,0)
    def test_mask_corruption_and_header_hash_mismatch_hold(self):
        sentinels=self.original_slots();selected=self.source/'frame_000000.vf3d';valid=frame_bytes();data=bytearray(valid);data[-1]=1;selected.write_bytes(data)
        self.assertEqual(self.invoke().returncode,2);self.assert_originals(sentinels)
        header=Header.from_buffer_copy(valid[:ctypes.sizeof(Header)]);header.crc=0;selected.write_bytes(bytes(header)+valid[ctypes.sizeof(Header):])
        self.assertEqual(self.invoke().returncode,2);self.assert_originals(sentinels)
    def test_nonzero_mask_semantics_with_matching_hash_are_preserved(self):
        selected=self.source/'frame_000000.vf3d'
        for value in (1,255):
            header=Header.from_buffer_copy(frame_bytes()[:ctypes.sizeof(Header)]);header.crc=mask_hash(bytes([value]))
            selected.write_bytes(bytes(header)+bytes(20)+bytes([value]));self.assertEqual(self.invoke().returncode,0)
    def test_multiple_chunk_boundaries_and_mask_tail_publish_exact_bytes(self):
        selected,cells=self.large();expected=selected.read_bytes();self.assertEqual(self.invoke().returncode,0)
        self.assertEqual((self.slots()[1]/selected.name).read_bytes(),expected)
    def test_late_chunk_nonfinite_value_holds_before_displacement(self):
        sentinels=self.original_slots();selected,cells=self.large();data=bytearray(selected.read_bytes())
        struct.pack_into('@f',data,ctypes.sizeof(Header)+(4*cells+cells-1)*4,float('nan'));selected.write_bytes(data)
        self.assertEqual(self.invoke().returncode,2);self.assert_originals(sentinels);self.assertEqual(self.attempts(),[])
    def test_active_nonfinite_or_mask_damage_clears_readiness(self):
        self.published();selected=self.project/'assets/vf3d/active/frame_000000.vf3d';valid=selected.read_bytes();data=bytearray(valid)
        struct.pack_into('@f',data,ctypes.sizeof(Header)+16,float('inf'));selected.write_bytes(data);self.status(2,False)
        data=bytearray(valid);data[-1]=255;selected.write_bytes(data);self.status(2,False)
        selected.write_bytes(valid);self.status()
    def test_finite_copy_corruption_preserves_all_seven_predecessors(self):
        sentinels=self.original_slots();expected=(self.source/'frame_000000.vf3d').read_bytes()
        env=os.environ.copy();env['CACHE_TEST_FINITE_COPY']='1'
        result=self.invoke(env=env);self.assertEqual(result.returncode,2,result.stdout)
        self.assert_originals(sentinels)
        self.assertEqual((self.source/'frame_000000.vf3d').read_bytes(),expected)
        self.assertEqual(len(self.attempts()),1)
        self.assertTrue((self.project/'physics_sim/.cache-publication.pending').is_file())
        staged=(self.attempts()[0]/'new-0/frame_000000.vf3d').read_bytes()
        self.assertEqual(len(staged),len(expected));self.assertNotEqual(staged,expected)

    def test_equal_length_pack_and_equivalent_json_copy_changes_hold(self):
        # Pack bytes are opaque here; JSON whitespace changes preserve meaning.
        for target in ('/new-0/frame_000000.pack','/new-1/frame_000000.pack',
                       '/new-0/manifest.json','/new-1/manifest.json',
                       '/new-2/scene_bundle.json','/new-3/scene_bundle.json'):
            with self.subTest(target=target):
                self.setUp();sentinels=self.original_slots()
                (self.source/'frame_000000.pack').write_bytes(b'opaque pack bytes')
                for name in ('manifest.json','scene_bundle.json'):
                    selected=self.source/name;selected.write_bytes(b' '+selected.read_bytes())
                expected=(self.source/target.rsplit('/',1)[1]).read_bytes()
                env=os.environ.copy();env['CACHE_TEST_BYTE_COPY']=target
                self.assertEqual(self.invoke(env=env).returncode,2);self.assert_originals(sentinels)
                self.assertEqual((self.source/target.rsplit('/',1)[1]).read_bytes(),expected)
                self.assertEqual(len(self.attempts()),1)
                staged=(self.attempts()[0]/target.lstrip('/')).read_bytes()
                self.assertEqual(len(staged),len(expected));self.assertNotEqual(staged,expected)

    def test_source_change_before_first_copy_holds_despite_equal_staged_bytes(self):
        for target in ('frame_000000.vf3d','frame_000000.pack','scene_bundle.json'):
            with self.subTest(target=target):
                self.setUp();sentinels=self.original_slots()
                (self.source/'frame_000000.pack').write_bytes(b'opaque source bytes')
                selected=self.source/'scene_bundle.json';selected.write_bytes(b' '+selected.read_bytes())
                before=(self.source/target).read_bytes()
                env=os.environ.copy();env['CACHE_TEST_SOURCE_MUTATE']=target
                self.assertEqual(self.invoke(env=env).returncode,2);self.assert_originals(sentinels)
                after=(self.source/target).read_bytes();self.assertNotEqual(after,before)
                self.assertEqual(len(self.attempts()),1)
                slots=(0,1) if target!='scene_bundle.json' else (2,3)
                for slot in slots:self.assertEqual((self.attempts()[0]/f'new-{slot}'/target).read_bytes(),after)
                self.assertTrue((self.project/'physics_sim/.cache-publication.pending').is_file())

    def test_same_byte_rewrite_and_new_selected_source_file_hold(self):
        for kind in ('rewrite','add'):
            with self.subTest(kind=kind):
                self.setUp();sentinels=self.original_slots()
                expected=(self.source/'frame_000000.vf3d').read_bytes()
                env=os.environ.copy();env['CACHE_TEST_SOURCE_MUTATE']='frame_000000.vf3d'
                env['CACHE_TEST_SOURCE_MUTATION_KIND']=kind
                self.assertEqual(self.invoke(env=env).returncode,2);self.assert_originals(sentinels)
                self.assertEqual((self.source/'frame_000000.vf3d').read_bytes(),expected)
                self.assertEqual(len(self.attempts()),1)
                for slot in (0,1):self.assertEqual((self.attempts()[0]/f'new-{slot}/frame_000000.vf3d').read_bytes(),expected)
                if kind=='add':
                    for slot in (2,3):self.assertEqual((self.attempts()[0]/f'new-{slot}/water_surface_added.json').read_bytes(),b'{}')
                self.assertTrue((self.project/'physics_sim/.cache-publication.pending').is_file())

    def test_late_staged_change_after_earlier_comparison_preserves_predecessors(self):
        for target in ('new-0/frame_000000.vf3d','new-1/frame_000000.vf3d','new-4','new-5','new-6'):
            with self.subTest(target=target):
                self.setUp();sentinels=self.original_slots()
                expected=(self.source/'frame_000000.vf3d').read_bytes()
                env=os.environ.copy();env['CACHE_TEST_STAGE_MUTATE']=target
                self.assertEqual(self.invoke(env=env).returncode,2);self.assert_originals(sentinels)
                self.assertEqual((self.source/'frame_000000.vf3d').read_bytes(),expected)
                self.assertEqual(len(self.attempts()),1)
                self.assertTrue((self.project/'physics_sim/.cache-publication.pending').is_file())
                if target.endswith('.vf3d'):self.assertNotEqual((self.attempts()[0]/target).read_bytes(),expected)

    def test_late_same_byte_staged_rewrite_holds(self):
        sentinels=self.original_slots();expected=(self.source/'frame_000000.vf3d').read_bytes()
        env=os.environ.copy();env['CACHE_TEST_STAGE_MUTATE']='new-0/frame_000000.vf3d'
        env['CACHE_TEST_STAGE_MUTATION_KIND']='rewrite'
        self.assertEqual(self.invoke(env=env).returncode,2);self.assert_originals(sentinels)
        self.assertEqual((self.attempts()[0]/'new-0/frame_000000.vf3d').read_bytes(),expected)

    def test_generated_manifest_wrong_intent_is_held_before_publication(self):
        for target in ('/new-4','/new-5','/new-6','all'):
            for kind in ('id','time','count','extra'):
                with self.subTest(target=target,kind=kind):
                    self.setUp();sentinels=self.original_slots()
                    env=os.environ.copy();env['CACHE_TEST_MANIFEST_COPY']=target
                    env['CACHE_TEST_MANIFEST_KIND']=kind
                    self.assertEqual(self.invoke(env=env).returncode,2);self.assert_originals(sentinels)
                    self.assertEqual(len(self.attempts()),1)
                    self.assertTrue((self.project/'physics_sim/.cache-publication.pending').is_file())

    def selection(self,source,start,stride,maximum,count):
        return subprocess.run([str(self.binary),str(self.project),str(self.run),'run-good','publish',
                               *map(str,(source,start,stride,maximum,count))],capture_output=True,text=True,timeout=5)

    def test_inconsistent_or_unbounded_selection_holds_before_attempt(self):
        sentinels=self.original_slots()
        for values in ((-1,0,1,0,1),(0,0,1,0,1),(2147483647,0,1,0,1),
                       (1,0,1,0,2),(1,1,1,0,1),(1,0,0,0,1),
                       (2,0,1,1,2),(10001,0,1,0,10001)):
            with self.subTest(values=values):
                self.assertEqual(self.selection(*values).returncode,2);self.assert_originals(sentinels)
                self.assertEqual(self.attempts(),[])

    def test_large_source_domain_with_bounded_selection_remains_supported(self):
        for values in ((2147483647,0,1,1,1),(2147483647,0,2147483647,0,1),(2,0,2,0,1)):
            with self.subTest(values=values):
                self.setUp();result=self.selection(*values);self.assertEqual(result.returncode,0,result.stdout)
                for path in self.targets()[4:]:
                    row=json.loads(path.read_text());self.assertEqual(row['retained_frame_indices'],[0])
                    self.assertEqual(row['frame_count'],1);self.assertEqual(row['export_stride'],values[2])
                    self.assertEqual(row['export_max_frames'],values[3])

    def test_sampled_deadline_and_read_failure_hold_without_attempt(self):
        sentinels=self.original_slots()
        for name in ('CACHE_TEST_PAYLOAD_EXPIRE','CACHE_TEST_PAYLOAD_READ_FAIL'):
            env=os.environ.copy();env[name]='1';self.assertEqual(self.invoke(env=env).returncode,2);self.assert_originals(sentinels);self.assertEqual(self.attempts(),[])
    def test_short_reads_and_interrupted_reads_complete_without_corruption(self):
        selected,cells=self.large();expected=selected.read_bytes();env=os.environ.copy();env['CACHE_TEST_PAYLOAD_SHORT_READ']='1'
        result=self.invoke(env=env);self.assertEqual(result.returncode,0,result.stdout);self.assertEqual((self.slots()[1]/selected.name).read_bytes(),expected)

if __name__=='__main__':unittest.main()
