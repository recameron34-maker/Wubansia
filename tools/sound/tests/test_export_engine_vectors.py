"""tools/sound/export_engine_vectors.py --check: a generated tap header matches the committed one when its text is the
same and each tap is within 1 ulp (numpy's vectorized sin and exp can round a tap one ulp apart with the arrays'
memory alignment: the script and `python -m` gave halfband.h's kHalfband1[18] as ...877 and ...8774)."""
import math
import unittest

from tools.sound import export_engine_vectors as X

HEADER = (b'#pragma once\n\nnamespace wubansia::engine::fx\n{\n'
          b'    inline constexpr std::array<double, 3> kTaps {\n'
          b'        0.05453258828118877,\n        -8.208760425127059e-05,\n        0.5,\n    };\n}\n')


def nudge(text, old, ulps):
    v = float(old)
    for _ in range(abs(ulps)):
        v = math.nextafter(v, math.inf if ulps > 0 else -math.inf)
    return text.replace(old.encode(), repr(v).encode())


class TestSameHeader(unittest.TestCase):
    def test_identical_bytes_match(self):
        self.assertTrue(X.same_header(HEADER, HEADER))

    def test_a_tap_one_ulp_apart_matches(self):
        self.assertTrue(X.same_header(HEADER, nudge(HEADER, '0.05453258828118877', 1)))
        self.assertTrue(X.same_header(HEADER, nudge(HEADER, '-8.208760425127059e-05', -1)))

    def test_a_tap_two_ulps_apart_differs(self):
        self.assertFalse(X.same_header(HEADER, nudge(HEADER, '0.05453258828118877', 2)))

    def test_other_text_must_be_identical(self):
        self.assertFalse(X.same_header(HEADER, HEADER.replace(b'kTaps', b'kTips')))
        self.assertFalse(X.same_header(HEADER, HEADER.replace(b'        0.5,\n', b'')))

    def test_the_committed_headers_pass_the_check(self):
        for path, data in X.files().items():
            if path.parent == X.GENERATED:
                self.assertTrue(X.same_header(path.read_bytes(), data), path.name)


if __name__ == '__main__':
    unittest.main()
