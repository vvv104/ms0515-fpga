"""The PC keyboard's tables for the МС 7004 translator (tools/ms7004_keys.py)."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import ms7004_keys  # noqa: E402


class Tables(unittest.TestCase):
    def test_every_key_is_on_the_matrix_and_every_code_once(self):
        _, _, _, problems = ms7004_keys.tables()
        self.assertEqual(problems, [])

    def test_places_are_the_matrix_s(self):
        where = ms7004_keys.places()
        self.assertEqual(where["SA8"], 7 * 16 + 0)          # ВР1, ГР8
        self.assertEqual(where["SA15"], 6 * 16 + 1)         # ВР2, ГР7
        self.assertEqual(where["SA34"], 4 * 16 + 4)         # А/A: ВР5, ГР5

    def test_shift_2_in_lat_is_at_with_vr_let_go(self):
        _, char, where, _ = ms7004_keys.tables()
        self.assertEqual(char[0x1E, 1, 0], (where["SA58"], 1, 0))

    def test_backslash_in_rus_wants_lat(self):
        _, char, where, _ = ms7004_keys.tables()
        self.assertEqual(char[0x5D, 0, 1], (where["SA66"], 1, 1))

    def test_the_committed_package_is_current(self):
        pos, char, where, _ = ms7004_keys.tables()
        with open(ms7004_keys.OUT, encoding="utf-8") as f:
            self.assertEqual(f.read(), ms7004_keys.text(pos, char, where),
                             "rtl/parts/ms7004_keys_pkg.sv is stale: run tools/ms7004_keys.py")


if __name__ == "__main__":
    unittest.main()
