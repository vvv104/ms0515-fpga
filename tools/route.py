#!/usr/bin/env python3
"""Route kicad/ns4.kicad_pcb with Freerouting (docs/plan.md stage 8d).

Run with KiCad's own Python, which has pcbnew:

    <KiCad>/bin/python tools/route.py [passes]

The placed board (tools/pcb.py) goes out as a Specctra design, Freerouting
routes it without its window, its session comes back onto the board, the
planes are filled and the board saved; then `tools/pcb.py check` reads the
nets back off the pads.  Freerouting's jar: $FREEROUTING_JAR, else
tools/freerouting/freerouting-2.5.0.jar of $MS0515_DATA (../ms0515_data).
The run's work files stay in build/route.
"""
import os
import subprocess
import sys

import pcbnew

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOARD = os.path.join(ROOT, "kicad", "ns4.kicad_pcb")
WORK = os.path.join(ROOT, "build", "route")


def jar():
    path = os.environ.get("FREEROUTING_JAR")
    if not path:
        data = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
        path = os.path.join(data, "tools", "freerouting", "freerouting-2.5.0.jar")
    if not os.path.exists(path):
        raise SystemExit(f"no Freerouting at {path}: set FREEROUTING_JAR")
    return path


def main(argv):
    passes = argv[1] if len(argv) > 1 else "100"
    os.makedirs(WORK, exist_ok=True)
    dsn, ses = os.path.join(WORK, "ns4.dsn"), os.path.join(WORK, "ns4.ses")
    board = pcbnew.LoadBoard(BOARD)
    if not pcbnew.ExportSpecctraDSN(board, dsn):
        raise SystemExit("the Specctra export failed")
    if os.path.exists(ses):
        os.remove(ses)
    log = os.path.join(WORK, "freerouting.log")
    with open(log, "w", encoding="utf-8") as out:
        subprocess.run(["java", "-jar", jar(), "-de", dsn, "-do", ses, "-mp", passes,
                        "--gui.enabled=false"], cwd=WORK, stdout=out, stderr=subprocess.STDOUT, check=False)
    if not os.path.exists(ses):
        raise SystemExit(f"Freerouting wrote no session; see {log}")
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise SystemExit("the session's import failed")
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(BOARD)
    tracks = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    vias = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print(f"routed: {tracks} track segments, {vias} vias -> {BOARD}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
