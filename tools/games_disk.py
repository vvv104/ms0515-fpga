#!/usr/bin/env python3
"""The diskette the board runs games from (docs/bench.md, "Games").

    python tools/games_disk.py [OUT.dsk]        (build/games/games.dsk)

A 400 KB single-sided diskette for ROM-B: the emulator's DEC RT-11 SJ
system diskette (its ms0515-run's, the smallest there is: the monitor,
SWAP, TT, DZ and a start file of one line, `SET TT QUIET`) with two games
of the software collection on it:

  BIRDS.SAV                 sound from its first second
  SABOT2.SAV, SABOT2.DAT    Saboteur 2, the 1991 build (the collection's
                            software/games/sabot2/omega): the one that
                            reads the joystick on the МС 7007's port, and
                            works through the memory dispatcher's banks.
                            The 1990 build, on the ОСА diskettes, never
                            reads the port.

Nothing of it is kept in this repository.  Where the parts are:

    MS0515_EMULATOR_DIR  the emulator (../ms0515): src/tools/run/assets/system.dsk
    MS0515_DISK          its ms0515-disk (<emulator>/package/ms0515-disk[.exe])
    MS0515_SOFTWARE      the collection (../ms0515-software)
"""
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAMES = ("BIRDS.SAV", os.path.join("sabot2", "omega", "SABOT2.SAV"), os.path.join("sabot2", "omega", "SABOT2.DAT"))


def places():
    emulator = os.environ.get("MS0515_EMULATOR_DIR", os.path.join(ROOT, "..", "ms0515"))
    exe = ".exe" if os.name == "nt" else ""
    tool = os.environ.get("MS0515_DISK", os.path.join(emulator, "package", "ms0515-disk" + exe))
    software = os.environ.get("MS0515_SOFTWARE", os.path.join(ROOT, "..", "ms0515-software"))
    system = os.path.join(emulator, "src", "tools", "run", "assets", "system.dsk")
    files = [os.path.join(software, "software", "games", name) for name in GAMES]
    for path in [tool, system] + files:
        if not os.path.exists(path):
            raise SystemExit(f"no {path}: see the script's first lines")
    return tool, system, files


def main(argv):
    out = argv[1] if len(argv) > 1 else os.path.join(ROOT, "build", "games", "games.dsk")
    tool, system, files = places()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    shutil.copyfile(system, out)
    subprocess.run([tool, "put", out] + files, check=True, stdout=subprocess.DEVNULL)
    listing = subprocess.run([tool, "dir", out], check=True, capture_output=True, text=True).stdout
    print(listing.rstrip())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
