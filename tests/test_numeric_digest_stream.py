"""Byte-level compatibility for the lossless bulk binary64 digest path."""
import math
from pathlib import Path
import random
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import numeric_digest_stream as stream
from surface_sources.growth_fire_v1 import _digest_form, canonical, digest as reference


class DigestTests(unittest.TestCase):
    def assert_bytes(self, value):
        expected = canonical(_digest_form({k: v for k, v in value.items() if k != 'digest'}))
        chunks = []

        class Capture:
            def update(self, part):
                chunks.append(bytes(part))

            def hexdigest(self):
                return 'captured'

        with patch.object(stream.hashlib, 'sha256', return_value=Capture()):
            self.assertEqual(stream.digest(value), 'captured')
        self.assertEqual(b''.join(chunks), expected)
        self.assertEqual(stream.digest(value), reference(value))

    def test_bulk_and_chunk_boundaries(self):
        for size in (0, 1, 255, 256, 257, 1023, 1024, 1025, 4095, 4096, 4097, 8192, 8193):
            with self.subTest(size=size):
                self.assert_bytes({'values': [float(i) / 7 for i in range(size)]})

    def test_random_finite_binary64_bits(self):
        rng = random.Random(20261031)
        values = []
        while len(values) < 10000:
            value = struct.unpack('>d', rng.getrandbits(64).to_bytes(8, 'big'))[0]
            if math.isfinite(value):
                values.append(value)
        self.assert_bytes({'values': values})

    def test_extremes_and_signed_zero(self):
        values = [0., -0., 5e-324, -5e-324, sys.float_info.max,
                  -sys.float_info.max, sys.float_info.min, -sys.float_info.min] * 600
        self.assert_bytes({'values': values})
        self.assertNotEqual(stream.digest({'values': [0.] * 256}),
                            stream.digest({'values': [-0.] * 256}))

    def test_mixed_types_and_escaped_sorted_keys(self):
        self.assert_bytes({'z': [True, False, None, 0, 0., -0., 2**53, 2**54],
                           '\u03a9\n"\\': {'\ud800': ['\U0001f525', '\t']},
                           'a': [{'digest': 'nested retained', 'x': [1.] * 256}],
                           'digest': 'top-level excluded'})
        self.assert_bytes({'values': [0.] * 256 + [1, True, None]})

    def test_top_level_exclusion_only(self):
        self.assertEqual(stream.digest({'a': 1}), stream.digest({'a': 1, 'digest': 'x'}))
        self.assertNotEqual(stream.digest({'a': {'digest': 'x'}}),
                            stream.digest({'a': {'digest': 'y'}}))

    def test_nonfinite_refused_at_every_bulk_position(self):
        for invalid in (float('nan'), float('inf'), -float('inf')):
            for size, index in ((1, 0), (256, 255), (4097, 4096), (8193, 4095)):
                values = [0.] * size
                values[index] = invalid
                with self.subTest(size=size, index=index, invalid=invalid):
                    with self.assertRaises(ValueError):
                        stream.digest({'values': values})
                    with self.assertRaises(ValueError):
                        reference({'values': values})

    def test_inexact_and_overflowing_integers_refused(self):
        for invalid in (2**53+1, -(2**53+1), 10**400):
            for value in ({'x': invalid}, {'x': [0.] * 256 + [invalid]}):
                with self.assertRaises(ValueError):
                    stream.digest(value)
                with self.assertRaises(ValueError):
                    reference(value)

    def test_sealed_identity(self):
        value = {'field': [float(i) for i in range(5000)]}
        sealed = stream.sealed(value)
        self.assertEqual(sealed['digest'], reference(value))
        self.assertIs(sealed['field'], value['field'])

    def test_integer_bulk_boundaries_and_exact_range(self):
        rng = random.Random(53128)
        for size in (255, 256, 257, 4095, 4096, 4097, 8193):
            self.assert_bytes({'values': [rng.randint(-(1<<53), 1<<53) for _ in range(size)]})
        self.assert_bytes({'values': [-(1<<53), 1<<53, 0, 1, -1] * 1000})
        self.assert_bytes({'mask': [0, 1] * 5000})

    def test_large_integer_fallback_preserves_acceptance_and_refusal(self):
        for valid in (1<<54, -(1<<54), 1<<1000):
            self.assert_bytes({'values': [valid] * 256})
        for invalid in ((1<<53)+1, -((1<<53)+1), 1<<1024):
            for index in (0, 255, 4096):
                values = [0] * 4097
                values[index] = invalid
                with self.assertRaises(ValueError):
                    stream.digest({'values': values})
                with self.assertRaises(ValueError):
                    reference({'values': values})

    def test_integer_bulk_never_coerces_bool_or_mixed_values(self):
        for tail in (True, False, None, 1., -0., {'nested': 1}):
            self.assert_bytes({'values': [0] * 4096 + [tail]})
        self.assert_bytes({'values': [True, False] * 2048})
        self.assertNotEqual(stream.digest({'values': [0] * 256}),
                            stream.digest({'values': [False] * 256}))


if __name__ == '__main__':
    unittest.main()
