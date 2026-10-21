"""The film's frame durations: whole milliseconds that keep to the machine's
own time."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import frames_to_video  # noqa: E402


class Durations(unittest.TestCase):
    def test_a_frame_of_the_machine_lasts_its_ticks(self):
        # 260 lines of 64 us: 16.64 ms, 1 996 800 master ticks
        spans = [(0, 1996800)] * 3
        self.assertEqual(sum(frames_to_video.durations(spans)), 50)

    def test_the_rounding_never_drifts(self):
        spans = [(i * 1996800, (i + 1) * 1996800) for i in range(600)]
        ms = frames_to_video.durations(spans)
        self.assertTrue(all(d in (16, 17) for d in ms))
        self.assertEqual(sum(ms), round(600 * 16.64))

    def test_an_odd_frame_keeps_its_own_length(self):
        self.assertEqual(frames_to_video.durations([(0, 2 * 1996800)]), [33])


class Canvas(unittest.TestCase):
    def test_the_film_is_as_tall_as_most_frames(self):
        # the first frames after power-on are of other lengths
        self.assertEqual(frames_to_video.canvas_height([251, 284, 260, 260, 260]), 260)


if __name__ == "__main__":
    unittest.main()
