"""ZIP content safety, bounded admission, and native fixture compatibility."""
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import unittest
import warnings
import zipfile
from unittest.mock import patch
import test_release_app_stage as fixture

sys.path.insert(0,str(fixture.ROOT/'scripts'))
from release_zip_validation import verify, LIMITS

class Zip(unittest.TestCase):
    def setUp(self):
        fixture.AppStage.setUp(self)
        self.archive=self.repo/'fixture.zip'
        subprocess.run([str(self.tool),'-c','-k','--keepParent',str(self.app),str(self.archive)],check=True,capture_output=True)

    def rewrite(self,change):
        with zipfile.ZipFile(self.archive) as old:rows=[(copy.copy(info),old.read(info)) for info in old.infolist()]
        rows=change(rows)
        with warnings.catch_warnings():
            warnings.simplefilter('ignore',UserWarning)
            with zipfile.ZipFile(self.archive,'w') as new:
                for info,data in rows:new.writestr(info,data)

    def test_exact_payload_modes_empty_directories_and_links(self):
        result=verify(self.archive,self.app)
        self.assertEqual(result['status'],'verified_ordinary_app_payload')
        self.assertFalse(result['complete_metadata_identity_verified'])
        self.assertEqual(result['app_members'],13)

    def test_missing_extra_traversal_and_duplicate_members_hold(self):
        original=self.archive.read_bytes()
        for kind in ('missing','extra','traversal','duplicate'):
            self.archive.write_bytes(original)
            def change(rows):
                if kind=='missing':return rows[1:]
                info=copy.copy(rows[-1][0])
                if kind=='extra':info.filename='foreign.app/payload'
                elif kind=='traversal':info.filename=self.app.name+'/../escape'
                else:info.filename=rows[-1][0].filename
                return rows+[(info,b'payload')]
            self.rewrite(change)
            with self.assertRaises(ValueError):verify(self.archive,self.app)

    def test_same_size_payload_corruption_and_wrong_links_hold(self):
        original=self.archive.read_bytes()
        for kind in ('file','symlink'):
            self.archive.write_bytes(original)
            def change(rows):
                for index,(info,data) in enumerate(rows):
                    if (kind=='file' and info.filename.endswith('/physics-sim-bin') or
                        kind=='symlink' and info.filename.endswith('/alias.dylib')):
                        rows[index]=(info,b'x'*len(data));break
                return rows
            self.rewrite(change)
            with self.assertRaisesRegex(ValueError,'differ'):verify(self.archive,self.app)

    def test_mode_and_special_file_encoding_hold(self):
        def change(rows):
            info,data=rows[-1];info.external_attr=(0o010644<<16);return rows
        self.rewrite(change)
        with self.assertRaisesRegex(ValueError,'type/mode'):verify(self.archive,self.app)

    def test_central_bounds_and_forged_count_hold_before_zipfile_allocation(self):
        original=self.archive.read_bytes()
        for kind in ('entry-cap','central-cap','forged-count'):
            self.archive.write_bytes(original);limits=dict(LIMITS)
            if kind=='entry-cap':limits['max_entries']=2
            elif kind=='central-cap':limits['max_central_bytes']=10
            else:
                raw=bytearray(original);offset=raw.rfind(b'PK\x05\x06')
                struct.pack_into('<HH',raw,offset+8,1,1);self.archive.write_bytes(raw)
            with patch('release_zip_validation.zipfile.ZipFile',side_effect=AssertionError('Must preflight central directory')):
                with self.assertRaisesRegex(ValueError,'central'):verify(self.archive,self.app,limits=limits)

    def test_expanded_budget_incomplete_deflate_and_corrupt_crc_hold(self):
        limits=dict(LIMITS);limits['max_bytes']=1
        with self.assertRaisesRegex(ValueError,'expanded byte'):verify(self.archive,self.app,limits=limits)
        raw=bytearray(self.archive.read_bytes())
        offset=raw.find(b'PK\x03\x04');name_length,extra_length=struct.unpack_from('<HH',raw,offset+26)
        # Select the first nonempty regular entry and flip its compressed bytes.
        with zipfile.ZipFile(self.archive) as payload:
            info=next(i for i in payload.infolist() if i.filename.endswith('/physics-sim-bin'))
        name_length,extra_length=struct.unpack_from('<HH',raw,info.header_offset+26)
        raw[info.header_offset+30+name_length+extra_length]^=1;self.archive.write_bytes(raw)
        with self.assertRaises(ValueError):verify(self.archive,self.app)
        # Corrupt the central CRC while leaving the payload unchanged.
        self.archive.unlink()
        subprocess.run([str(self.tool),'-c','-k','--keepParent',str(self.app),str(self.archive)],check=True,capture_output=True)
        raw=bytearray(self.archive.read_bytes());position=raw.find(b'PK\x01\x02')
        while position>=0:
            name_length,extra_length,comment_length=struct.unpack_from('<HHH',raw,position+28)
            name=bytes(raw[position+46:position+46+name_length])
            if name.endswith(b'/physics-sim-bin'):
                raw[position+16]^=1;break
            position=raw.find(b'PK\x01\x02',position+46+name_length+extra_length+comment_length)
        self.assertGreaterEqual(position,0);self.archive.write_bytes(raw)
        with self.assertRaisesRegex(ValueError,'CRC'):verify(self.archive,self.app)

    def test_large_deflate_payload_and_forced_zip64_local_headers(self):
        binary=self.app/'Contents/MacOS/physics-sim-bin';binary.write_bytes(b'A'*(3*1048576)+b'end')
        with zipfile.ZipFile(self.archive,'w') as payload:
            for path in [self.app]+sorted(self.app.rglob('*')):
                name=str(path.relative_to(self.app.parent))
                if path.is_symlink():
                    info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=path.lstat().st_mode<<16
                    payload.writestr(info,os.fsencode(os.readlink(path)))
                elif path.is_file():
                    info=zipfile.ZipInfo.from_file(path,name);info.compress_type=zipfile.ZIP_DEFLATED
                    with payload.open(info,'w',force_zip64=True) as stream:stream.write(path.read_bytes())
                else:payload.write(path,name)
        result=verify(self.archive,self.app)
        self.assertGreater(result['decompressed_work_bytes'],result['expanded_bytes'])
        self.assertGreater(result['expanded_bytes'],3*1048576)
        raw=self.archive.read_bytes();position=raw.rfind(b'PK\x05\x06')
        end=list(struct.unpack('<4s4H2LH',raw[position:position+22]))
        zip64=struct.pack('<4sQ2H2L4Q',b'PK\x06\x06',44,45,45,0,0,end[3],end[4],end[5],end[6])
        locator=struct.pack('<4sLQL',b'PK\x06\x07',0,position,1)
        end[3]=end[4]=65535;end[5]=end[6]=4294967295
        self.archive.write_bytes(raw[:position]+zip64+locator+struct.pack('<4s4H2LH',*end))
        self.assertEqual(verify(self.archive,self.app)['status'],'verified_ordinary_app_payload')

    def test_wall_deadline_holds_before_zipfile_allocation(self):
        import release_zip_validation as validator
        original=validator.central_bounds;expired=False
        def clock():return 121 if expired else 0
        def expire(*args):
            nonlocal expired
            expired=True
            return original(*args)
        with patch('release_zip_validation.time.monotonic',side_effect=clock),patch('release_zip_validation.central_bounds',side_effect=expire),patch('release_zip_validation.zipfile.ZipFile',side_effect=AssertionError('Must stop on deadline')):
            with self.assertRaisesRegex(ValueError,'wall bound'):verify(self.archive,self.app)

    @unittest.skipUnless(sys.platform=='darwin','native macOS archive encoding')
    def test_native_ditto_resource_metadata_and_symlinks(self):
        file=self.app/'Contents/MacOS/physics-sim-bin'
        subprocess.run(['/usr/bin/xattr','-w','com.apple.ResourceFork','fixture resource fork',str(file)],check=True,capture_output=True)
        subprocess.run(['/usr/bin/xattr','-w','com.codework.lifecycle','fixture attribute',str(file)],check=True,capture_output=True)
        native=self.repo/'native.zip'
        subprocess.run(['/usr/bin/ditto','-c','-k','--sequesterRsrc','--keepParent',str(self.app),str(native)],check=True,timeout=30)
        result=verify(native,self.app);self.assertGreater(result['appledouble_members'],0)
        self.assertFalse(result['complete_metadata_identity_verified'])

    def test_forged_appledouble_metadata_is_rejected(self):
        def change(rows):
            info=zipfile.ZipInfo('__MACOSX/'+self.app.name+'/Contents/MacOS/._physics-sim-bin')
            info.create_system=3;info.external_attr=0o100644<<16
            return rows+[(info,b'not AppleDouble')]
        self.rewrite(change)
        with self.assertRaisesRegex(ValueError,'AppleDouble'):verify(self.archive,self.app)

if __name__=='__main__':unittest.main()
