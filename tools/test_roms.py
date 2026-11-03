#!/usr/bin/env python3
"""The board's test ROMs, assembled from firmware/tests/*.mac.

    python tools/test_roms.py          - write firmware/tests/*.rom
    python tools/test_roms.py check    - the same without writing (exit 1 when
                                         a committed image is stale)

A test ROM is a program the processor runs from reset (172000) in place
of the machine's ROM: it works one part of the board that the machine's
software leaves alone and leaves what it saw in RAM for the bench to read
(sim/board/test_parts.cpp).  The sources are MACRO-11, assembled and linked
on the machine itself: DEC's MACRO and LINK run by the emulator's
ms0515-run, each in a folder of its own with the source, the assembler,
the linker and SYSMAC.SML.  LINK's absolute loader output (/L) becomes the
16 KB image of 140000..177777 (the window at 160000 shows its upper 8 KB),
words little-endian, blank cells 377 as an
erased К573РФ4's.

The tools are found through the environment, with the sibling
repositories as the fallback:
    MS0515_RUN       ms0515-run          (../ms0515/package/ms0515-run[.exe])
    MS0515_SOFTWARE  the collection, for software/development's MACRO.SAV,
                     LINK.SAV, SYSMAC.SML (../ms0515-software)
"""
import glob
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TESTS = os.path.join(ROOT, "firmware", "tests")
ROM_BASE = 0o140000                # the 16 KB of D65/D77: 140000..177777 (160000 up the upper 8 KB)
ROM_BYTES = 16384


def tool_paths():
    run = os.environ.get("MS0515_RUN")
    if not run:
        exe = "ms0515-run.exe" if os.name == "nt" else "ms0515-run"
        run = os.path.join(ROOT, "..", "ms0515", "package", exe)
    software = os.environ.get("MS0515_SOFTWARE", os.path.join(ROOT, "..", "ms0515-software"))
    return run, os.path.join(software, "software", "development")


def lda_records(data):
    """(address, bytes) of each block of an absolute loader file; the last
    one, of no bytes, carries the start address."""
    records, i = [], 0
    while i < len(data):
        if data[i] != 1 or data[i + 1] != 0:
            i += 1                                  # leader / padding
            continue
        count = data[i + 2] | data[i + 3] << 8
        address = data[i + 4] | data[i + 5] << 8
        body = data[i + 6:i + count]
        if (sum(data[i:i + count + 1]) & 0xFF) != 0:
            raise ValueError(f"LDA block at byte {i}: bad checksum")
        records.append((address, bytes(body)))
        if count == 6:
            break
        i += count + 1
    return records


def rom_image(records):
    image = bytearray([0o377] * ROM_BYTES)
    for address, body in records[:-1]:
        if address < ROM_BASE or address + len(body) > ROM_BASE + ROM_BYTES:
            raise ValueError(f"{len(body)} bytes at {address:06o}: outside the ROM")
        image[address - ROM_BASE:address - ROM_BASE + len(body)] = body
    return bytes(image)


def assemble(source, run, development):
    """The ROM image a source makes, and the listing."""
    name = os.path.splitext(os.path.basename(source))[0].upper()
    with tempfile.TemporaryDirectory() as work:
        for tool in ("MACRO.SAV", "LINK.SAV", "SYSMAC.SML"):
            shutil.copy(os.path.join(development, tool), work)
        with open(source, encoding="ascii") as f:
            text = f.read()
        with open(os.path.join(work, name + ".MAC"), "w", encoding="ascii", newline="\r\n") as f:
            f.write(text)
        for line in (f"MACRO {name},{name}={name}", f"LINK {name}.LDA={name}/L"):
            prog, args = line.split(" ", 1)
            subprocess.run([run, prog, args], cwd=work, check=True, capture_output=True)
        found = {f.upper(): f for f in os.listdir(work)}
        with open(os.path.join(work, found[name + ".LST"]), encoding="ascii", errors="replace") as f:
            listing = f.read()
        if "Errors detected:  0" not in listing:
            raise ValueError(f"{source}: MACRO reports errors\n{listing}")
        with open(os.path.join(work, found[name + ".LDA"]), "rb") as f:
            return rom_image(lda_records(f.read())), listing


def main(argv):
    check = argv[1:] == ["check"]
    run, development = tool_paths()
    stale = []
    for source in sorted(glob.glob(os.path.join(TESTS, "*.mac"))):
        image, _ = assemble(source, run, development)
        target = os.path.splitext(source)[0] + ".rom"
        old = open(target, "rb").read() if os.path.exists(target) else None
        if old != image:
            stale.append(os.path.basename(target))
            if not check:
                with open(target, "wb") as f:
                    f.write(image)
    for name in stale:
        print(("stale: " if check else "written: ") + name)
    return 1 if check and stale else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
