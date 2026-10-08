"""Exact declared cache frame/bundle inventory and raw structural admission."""
import ctypes
import json
import os
from pathlib import Path
import unittest
import test_cache_status_admission as status
from cache_fixture import Header,frame_bytes,write_cache

class CacheInventory(status.CacheStatusAdmission):
    def test_missing_source_artifacts_hold_before_predecessor_displacement(self):
        sentinels=self.retained()
        for name in ('manifest.json','scene_bundle.json','frame_000000.vf3d'):
            selected=self.source/name;original=selected.read_bytes();selected.unlink()
            self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels);self.assertEqual(self.attempts(),[])
            selected.write_bytes(original)
    def test_source_manifest_sequence_contract_and_bundle_linkage_hold(self):
        sentinels=self.retained();manifest=json.loads((self.source/'manifest.json').read_text());bundle=json.loads((self.source/'scene_bundle.json').read_text())
        for key,value in (('frames',[]),('frame_contract','vf2d'),('grid_w',0),('manifest_version',1)):
            row=dict(manifest);row[key]=value;(self.source/'manifest.json').write_text(json.dumps(row))
            self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
        (self.source/'manifest.json').write_text(json.dumps(manifest));bundle['fluid_source']['path']='../outside'
        (self.source/'scene_bundle.json').write_text(json.dumps(bundle));self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
    def test_raw_header_version_index_geometry_and_finite_fields_hold(self):
        sentinels=self.retained();selected=self.source/'frame_000000.vf3d'
        for key,value in (('magic',0),('version',2),('w',2),('frame',1),('time',float('nan')),('dt',-1),('voxel',0),('upz',0)):
            header=Header.from_buffer_copy(frame_bytes()[:ctypes.sizeof(Header)]);setattr(header,key,value);selected.write_bytes(bytes(header)+bytes(21))
            self.assertEqual(self.invoke().returncode,2,key);self.assert_retained(sentinels)
    def test_truncated_trailing_empty_and_oversized_shape_hold(self):
        sentinels=self.retained();selected=self.source/'frame_000000.vf3d';valid=frame_bytes()
        for data in (b'',valid[:20],valid[:-1],valid+b'trailing'):
            selected.write_bytes(data);self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
        manifest=json.loads((self.source/'manifest.json').read_text());manifest.update(grid_w=2147483647,grid_h=2147483647,grid_d=2147483647)
        (self.source/'manifest.json').write_text(json.dumps(manifest));header=Header.from_buffer_copy(valid[:ctypes.sizeof(Header)]);header.w=header.h=header.d=2147483647
        selected.write_bytes(bytes(header)+bytes(21));self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
    def test_extra_frames_noncanonical_names_and_ambiguous_source_hold(self):
        sentinels=self.retained()
        for name in ('frame_000001.vf3d','frame_0.vf3d','frame_000009.pack','frame_'+('9'*100)+'.vf3d'):
            extra=self.source/name;extra.write_bytes(frame_bytes());self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels);extra.unlink()
        write_cache(self.source.parent/'second');self.assertEqual(self.invoke().returncode,2);self.assert_retained(sentinels)
    def test_silent_staged_copy_corruption_is_caught_before_replacement(self):
        self.predecessors();sentinels=[p/'retained' for p in self.slots()];env=os.environ.copy();env['CACHE_TEST_CORRUPT_COPY']='1'
        result=self.invoke(env=env);self.assertEqual(result.returncode,2,result.stdout);self.assert_retained(sentinels)
        for target in self.targets()[4:]:self.assertEqual(target.read_bytes(),b'previous-manifest')
        self.assertTrue((self.project/'physics_sim/.cache-publication.pending').exists());self.assertEqual(len(self.attempts()),1)
    def test_readiness_holds_when_declared_active_frame_is_missing_or_corrupted(self):
        self.published();selected=self.project/'assets/vf3d/active/frame_000000.vf3d';valid=selected.read_bytes()
        selected.unlink();self.status(2,False);selected.write_bytes(valid[:-1]);self.status(2,False)
        selected.write_bytes(valid);self.status()
    def test_copied_manifest_agreement_and_optional_pack_stems(self):
        self.published();selected=self.project/'assets/physics/active/manifest.json';row=json.loads(selected.read_text());row['grid_w']=2;selected.write_text(json.dumps(row));self.status(2,False)
        selected.write_bytes((self.source/'manifest.json').read_bytes());self.status()
        optional=self.source/'frame_000000.pack';optional.write_bytes(b'controlled-optional-pack-not-qualified');self.assertEqual(self.invoke().returncode,0);self.status()

if __name__=='__main__':unittest.main()
