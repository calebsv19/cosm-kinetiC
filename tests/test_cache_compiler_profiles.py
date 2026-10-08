"""Actual cache reader classification under local Clang optimization profiles.
This qualifies the tested compiler/host only; it does not qualify solver math.
"""
import ctypes
from pathlib import Path
import shlex
import struct
import subprocess
import unittest
from cache_fixture import Header, frame_bytes
import test_cache_publish_admission as admission
ROOT = admission.ROOT

class CacheCompilerProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        admission.CacheAdmission.setUpClass()
        cls.addClassCleanup(admission.CacheAdmission.tearDownClass)
        base = Path(admission.CacheAdmission.t.name)
        cls.binaries = []
        json_cflags = shlex.split(subprocess.check_output(
            ['pkg-config', '--cflags', 'json-c'], text=True))
        json_flags = shlex.split(subprocess.check_output(
            ['pkg-config', '--cflags', '--libs', 'json-c'], text=True))
        for name, flags in (('optimized', ['-O2']),
                            ('finite', ['-O2', '-ffinite-math-only']),
                            ('fast', ['-O2', '-ffast-math'])):
            obj = base / (name + '.o')
            binary = base / name
            subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            *flags, '-I' + str(ROOT / 'include'), *json_cflags,
                            '-c', str(ROOT / 'src/app/scene_project_cache_output.c'),
                            '-o', str(obj)], check=True)
            # Profile only the boundary object. Do not alter library/runtime FP
            # startup modes through linker flags or conflate them with this check.
            subprocess.run(['clang', '-std=c11', '-I' + str(ROOT / 'include'),
                            str(base / 'probe.c'), str(obj),
                            str(ROOT / 'src/app/physics_sim_json_helpers.c'),
                            str(ROOT / 'src/app/physics_sim_job_json.c'),
                            *json_flags, '-o', str(binary)], check=True)
            cls.binaries.append((name, binary))

    def check_bytes(self, data, expected):
        for name, binary in self.binaries:
            with self.subTest(profile=name):
                native = admission.CacheAdmission()
                native.setUp()
                try:
                    native.binary = binary
                    retained = native.retained()
                    (native.source / 'frame_000000.vf3d').write_bytes(data)
                    result = native.invoke()
                    self.assertEqual(result.returncode, expected, result.stdout)
                    if expected:
                        native.assert_retained(retained)
                        self.assertEqual(list((native.project / 'physics_sim').glob(
                            '.cache-publication-attempt-*')), [])
                    else:
                        self.assertEqual((native.slots()[1] / 'frame_000000.vf3d').read_bytes(), data)
                finally:
                    native.doCleanups()

    def test_valid_and_finite_extreme_payloads_publish_exact_bytes(self):
        self.check_bytes(frame_bytes(), 0)
        data = bytearray(frame_bytes())
        # Positive/negative zero, subnormal, maximum finite, negative maximum.
        struct.pack_into('@5I', data, ctypes.sizeof(Header),
                         0, 0x80000000, 1, 0x7f7fffff, 0xff7fffff)
        self.check_bytes(data, 0)

    def test_every_payload_field_nonfinite_encoding_holds_before_attempt(self):
        for field in range(5):
            for bits in (0x7f800000, 0xff800000, 0x7fc00001,
                         0xffc00001, 0x7f800001, 0xff800001):
                with self.subTest(field=field, bits=hex(bits)):
                    data = bytearray(frame_bytes())
                    struct.pack_into('@I', data, ctypes.sizeof(Header) + field * 4, bits)
                    self.check_bytes(data, 2)

    def test_every_header_float_nonfinite_encoding_holds_before_attempt(self):
        for field in ('time', 'dt', 'ox', 'oy', 'oz', 'voxel', 'upx', 'upy', 'upz'):
            double = field in ('time', 'dt')
            encodings = ((0x7ff0000000000000, 0xfff0000000000000,
                          0x7ff8000000000001, 0xfff8000000000001,
                          0x7ff0000000000001, 0xfff0000000000001) if double else
                         (0x7f800000, 0xff800000, 0x7fc00001,
                          0xffc00001, 0x7f800001, 0xff800001))
            for bits in encodings:
                with self.subTest(field=field, bits=hex(bits)):
                    data = bytearray(frame_bytes())
                    struct.pack_into('@Q' if double else '@I', data,
                                     getattr(Header, field).offset, bits)
                    self.check_bytes(data, 2)

if __name__ == '__main__':
    unittest.main()
