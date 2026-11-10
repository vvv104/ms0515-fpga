#!/usr/bin/env python3
"""The fab's files of the routed board (docs/plan.md 8f; what they are: docs/pcb/fab.md).

    python tools/fab.py [BOARD]      - build/fab/<board>/: gerber/ (the layers and the drill
                                       files), <board>-gerbers.zip of them, assembly-top.pdf

BOARD is kicad/ns4.kicad_pcb unless given.  Needs KiCad's command line: $KICAD_CLI, else
kicad-cli on the PATH.  The planes are refilled before plotting (--check-zones).
"""
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import kicad  # noqa: E402

GERBER_LAYERS = ("F.Cu,In1.Cu,In2.Cu,In3.Cu,In4.Cu,In5.Cu,In6.Cu,B.Cu,"
                 "F.Mask,B.Mask,F.SilkS,B.SilkS,Edge.Cuts")
ASSEMBLY_LAYERS = "F.Fab,F.SilkS,Edge.Cuts"


def commands(cli, board, out):
    """kicad-cli's three runs: the gerbers, the drill files beside them, the assembly drawing."""
    gerber = os.path.join(out, "gerber")
    return [
        [cli, "pcb", "export", "gerbers", "-o", gerber, "--layers", GERBER_LAYERS,
         "--subtract-soldermask", "--check-zones", board],
        [cli, "pcb", "export", "drill", "-o", gerber + os.sep, "--format", "excellon",
         "--excellon-separate-th", "--generate-map", "--map-format", "pdf", board],
        [cli, "pcb", "export", "pdf", "-o", os.path.join(out, "assembly-top.pdf"), "--layers", ASSEMBLY_LAYERS,
         "--mode-single", "--black-and-white", "--sketch-pads-on-fab-layers", "--check-zones", board],
    ]


def main(argv):
    board = os.path.abspath(argv[1]) if len(argv) > 1 else os.path.join(ROOT, "kicad", "ns4.kicad_pcb")
    name = os.path.splitext(os.path.basename(board))[0]
    out = os.path.join(ROOT, "build", "fab", name)
    cli = kicad.kicad_cli()
    if not cli:
        print("no kicad-cli: set $KICAD_CLI or put it on the PATH")
        return 2
    os.makedirs(os.path.join(out, "gerber"), exist_ok=True)
    for cmd in commands(cli, board, out):
        done = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if done.returncode:
            print(done.stdout + done.stderr)
            return done.returncode
    files = sorted(os.listdir(os.path.join(out, "gerber")))
    pack = os.path.join(out, f"{name}-gerbers.zip")
    with zipfile.ZipFile(pack, "w", zipfile.ZIP_DEFLATED) as z:
        for f in files:
            z.write(os.path.join(out, "gerber", f), f)
    print(f"{len(files)} files -> {pack}; assembly drawing -> {os.path.join(out, 'assembly-top.pdf')}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
