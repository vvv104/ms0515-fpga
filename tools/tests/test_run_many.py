"""run_many: the runs' summaries read and judged."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import run_many  # noqa: E402

LINE = ("summary: ms=6500 seed=2 fetches=2627517 matched=2626825 diverged=0 waits=2 waited=692 "
        "frames=391 last_differ=0")


class Summary(unittest.TestCase):
    def test_the_last_summary_line_is_read(self):
        log = "6490 ms: ...\n" + LINE + "\n"
        s = run_many.summary(log)
        self.assertEqual(s["seed"], 2)
        self.assertEqual(s["matched"], 2626825)
        self.assertEqual(s["last_differ"], 0)

    def test_a_run_without_one_has_none(self):
        self.assertIsNone(run_many.summary("10 ms: 0 fetches\n"))

    def test_a_run_is_good_with_the_emulator_kept_to_and_the_screen_the_same(self):
        self.assertTrue(run_many.good(run_many.summary(LINE)))
        self.assertFalse(run_many.good(run_many.summary(LINE.replace("diverged=0", "diverged=1"))))
        self.assertFalse(run_many.good(run_many.summary(LINE.replace("last_differ=0", "last_differ=8"))))
        self.assertFalse(run_many.good(None))


if __name__ == "__main__":
    unittest.main()
