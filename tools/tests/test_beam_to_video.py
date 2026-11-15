"""beam_to_video: the frames drawn from the ticks, no point sampled."""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import beam_to_video as bv  # noqa: E402


def beam_file(lines):
    out = struct.pack("<H", len(lines))
    for line in lines:
        out += struct.pack("<H", len(line))
        for tick, colour in line:
            out += struct.pack("<HB", tick, colour)
    return out


class BeamToVideo(unittest.TestCase):
    def test_reads_what_run_board_writes(self):
        # the bytes of sim/board/test_beam.cpp's frame
        data = b"\x01\x00" b"\x02\x00" b"\x00\x00\x00" b"\x01\x00\x09"
        self.assertEqual(bv.read_frame(data), [[(0, 0), (1, 9)]])

    def test_each_change_held_to_the_next(self):
        ticks = bv.line_ticks([(0, 0), (2, 7), (5, 0)], 8)
        self.assertEqual(list(ticks), [0, 0, 7, 7, 7, 0, 0, 0])

    def test_a_column_is_its_ticks_mean_light(self):
        # white not bright (7) for 3 of a column's 4 ticks: three quarters of 170
        image = bv.draw([[(0, 7), (3, 0)]], per=4, first=0, last=8)
        self.assertEqual(image.shape, (2, 2, 3))
        self.assertEqual(list(image[0, 0]), [128, 128, 128])
        self.assertEqual(list(image[0, 1]), [0, 0, 0])
        self.assertEqual(list(image[1, 0]), [128, 128, 128])

    def test_bright_blue_is_full_blue(self):
        image = bv.draw([[(0, 9)]], per=1, first=0, last=1)
        self.assertEqual(list(image[0, 0]), [0, 0, 255])

    def test_frames_cut_or_filled_to_one_height(self):
        image = bv.draw([[(0, 7)]], per=2, first=0, last=4, height=3)
        self.assertEqual(image.shape, (6, 2, 3))
        self.assertEqual(list(image[5, 0]), [0, 0, 0])

    def test_round_trip_of_a_file(self):
        lines = [[(0, 1), (100, 10)], [(0, 4)]]
        self.assertEqual(bv.read_frame(beam_file(lines)), lines)


if __name__ == "__main__":
    unittest.main()
