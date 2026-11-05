"""The sampler's test vectors (engine/tests/vectors/sampler/) are what tools/sound/export_sampler_vectors.py makes from
the recipes now. The engine's C++ tests check its voices against those files; if a recipe change moves a value (FUT01's
basslines.hermite or context4.bass_voice, an envelope, a kit's layer set), this test names the file, and the vectors
are re-exported (and the C++ tests rerun) in a commit of their own."""
import hashlib
import unittest

import tools.sound  # noqa: F401  (the SIMD pins, before numpy)

from tools.sound import export_sampler_vectors as X


class TestSamplerVectors(unittest.TestCase):

    def test_the_committed_vectors_are_a_fresh_export(self):
        files = X.vectors()
        files['vectors.sha256'] = X.manifest(files).encode()
        for name, data in sorted(files.items()):
            with self.subTest(file=name):
                path = X.OUT / name
                self.assertTrue(path.is_file(), f'{name} is not committed: run tools/sound/export_sampler_vectors.py')
                self.assertEqual(hashlib.sha256(data).hexdigest(), hashlib.sha256(path.read_bytes()).hexdigest(),
                                 f'{name} differs from what the recipes give now: re-export it')

    def test_no_stale_vector_is_committed(self):
        exported = {name for name in X.vectors()}
        self.assertEqual(sorted(exported), sorted(p.name for p in X.OUT.glob('*.f32')))

    def test_check_mode_passes(self):
        self.assertEqual(0, X.main(['--check']))


if __name__ == '__main__':
    unittest.main()
