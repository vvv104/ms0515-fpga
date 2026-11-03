import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import test_roms  # noqa: E402


def block(address, body):
    data = bytes([1, 0, (6 + len(body)) & 0xFF, (6 + len(body)) >> 8, address & 0xFF, address >> 8]) + body
    return data + bytes([(-sum(data)) & 0xFF])


def test_lda_blocks_and_the_start():
    data = bytes(4) + block(0o172000, b"\x01\x02\x03") + block(0o172000, b"")
    assert test_roms.lda_records(data) == [(0o172000, b"\x01\x02\x03"), (0o172000, b"")]


def test_a_bad_checksum_is_refused():
    bad = bytearray(block(0o172000, b"\x01"))
    bad[-1] ^= 1
    with pytest.raises(ValueError, match="checksum"):
        test_roms.lda_records(bytes(bad))


def test_the_image_is_the_rom_with_blank_cells_erased():
    image = test_roms.rom_image([(0o172000, b"\x05\x06"), (0o172000, b"")])
    assert len(image) == 16384
    assert image[0o32000:0o32002] == b"\x05\x06"
    assert image[0] == 0o377 and image[-1] == 0o377


def test_bytes_outside_the_rom_are_refused():
    with pytest.raises(ValueError, match="outside"):
        test_roms.rom_image([(0o1000, b"\x00"), (0o1000, b"")])
