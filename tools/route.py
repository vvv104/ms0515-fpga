#!/usr/bin/env python3
"""Route kicad/ns4.kicad_pcb with Freerouting (docs/plan.md stage 8d).

Run with KiCad's own Python, which has pcbnew:

    <KiCad>/bin/python tools/route.py start
    <KiCad>/bin/python tools/route.py run [passes per chunk] [chunks without gain]
    <KiCad>/bin/python tools/route.py finish

The routing is long (a night and more) and is made to survive being
stopped.  `start` exports the placed board as a Specctra design with its
wires left out - the board routed on 2026-11-04 (51 unrouted), exported
with its wires by KiCad, came into Freerouting as 1346 unrouted, more than
the empty board's 1257 - and keeps it as build/route/state.dsn.  `run`
routes that state a few passes at a time: each chunk is a Freerouting run
of its own, which writes its session; the session's wires and vias go
into the design's wiring at their exact coordinates (Freerouting cannot
save a design without its window: the file comes out empty), and only
then do state.dsn and state.ses replace the last ones (atomic renames).
Loaded back, a chunk's design counts what the chunk ended with (521 and
521 on the first pass).  Stopping `run`, or the computer, loses the chunk
in progress and nothing before it; `run` again goes on from the last
state.  It stops by itself when nothing is left unrouted or when that
many chunks in a row gained nothing.  Each chunk's line goes to
build/route/progress.txt.  `finish` puts state.ses on the board in place
of the board's wires, fills the planes and saves the board; then
`tools/pcb.py check` reads the nets back off the pads.

Freerouting's jar: $FREEROUTING_JAR, else
tools/freerouting/freerouting-2.5.0.jar of $MS0515_DATA (../ms0515_data).
"""
import datetime
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOARD = os.path.join(ROOT, "kicad", "ns4.kicad_pcb")
WORK = os.path.join(ROOT, "build", "route")
STATE = os.path.join(WORK, "state.dsn")
NEXT = os.path.join(WORK, "state.next.dsn")
SESSION = os.path.join(WORK, "state.ses")
NEXT_SES = os.path.join(WORK, "state.next.ses")
LOG = os.path.join(WORK, "freerouting.log")
PROGRESS = os.path.join(WORK, "progress.txt")

PASS_DONE = re.compile(r"\((\d+) unrouted and (\d+) violations\)")


def jar():
    path = os.environ.get("FREEROUTING_JAR")
    if not path:
        data = os.environ.get("MS0515_DATA", os.path.join(ROOT, "..", "ms0515_data"))
        path = os.path.join(data, "tools", "freerouting", "freerouting-2.5.0.jar")
    if not os.path.exists(path):
        raise SystemExit(f"no Freerouting at {path}: set FREEROUTING_JAR")
    return path


def stamp():
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def put_aside(path):
    """Keep an earlier file under its time instead of overwriting it."""
    if os.path.exists(path):
        when = datetime.datetime.fromtimestamp(os.path.getmtime(path)).strftime("%Y%m%d-%H%M%S")
        base, ext = os.path.splitext(path)
        os.replace(path, f"{base}.{when}{ext}")


def without_wiring(text):
    """The design with its (wiring ...) section emptied."""
    start = text.find("(wiring")
    if start < 0:
        return text
    depth = 0
    for end in range(start, len(text)):
        depth += {"(": 1, ")": -1}.get(text[end], 0)
        if depth == 0:
            return text[:start] + "(wiring\n  )" + text[end + 1:]
    raise SystemExit("the design's wiring section does not close")


TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))')


def sexpr(text):
    """A Specctra s-expression as nested lists of strings."""
    stack, pos = [[]], 0
    while True:
        m = TOKEN.match(text, pos)
        if not m or m.end() == pos:
            break
        pos = m.end()
        if m.group(1):
            stack.append([])
        elif m.group(2):
            done = stack.pop()
            stack[-1].append(done)
        else:
            stack[-1].append(m.group(3) if m.group(3) is not None else m.group(4))
    return stack[0][0]


def child(node, name):
    return next((c for c in node[1:] if isinstance(c, list) and c and c[0] == name), None)


def quoted(name):
    return '"' + name.replace('"', '\\"') + '"'


def session_wiring(ses_text, design_text):
    """The design with the session's wires and vias as its wiring.

    A session counts in its resolution's steps (0.1 um here), the design
    in its unit (um); the session's routes are scaled across."""
    session = sexpr(ses_text)
    routes = child(session, "routes")
    res = child(routes, "resolution")
    unit = re.search(r"\(unit\s+(\w+)\)", design_text)
    if res[1] != (unit.group(1) if unit else res[1]):
        raise SystemExit(f"the session counts in {res[1]}, the design in {unit.group(1)}")
    scale = int(res[2])
    places = len(str(scale)) - 1

    def num(value):            # exact: a wire's end must stay on its pin
        text = f"{int(value) / scale:.{places}f}"
        return text.rstrip("0").rstrip(".") if places else text
    lines = ["(wiring"]
    for net in child(routes, "network_out")[1:]:
        name = quoted(net[1])
        for item in net[2:]:
            if item[0] == "wire":
                path = child(item, "path")
                coords = " ".join(num(v) for v in path[3:])
                lines.append(f"    (wire (path {path[1]} {num(path[2])} {coords})(net {name})(type route))")
            elif item[0] == "via":
                lines.append(f"    (via {quoted(item[1])} {num(item[2])} {num(item[3])} (net {name})(type route))")
    lines.append("  )")
    empty = without_wiring(design_text)
    at = empty.find("(wiring\n  )")
    return empty[:at] + "\n".join(lines) + empty[at + len("(wiring\n  )"):]


def freerouting(design, output, passes):
    """One Freerouting run; its log is appended to LOG, its last count returned."""
    begin = os.path.getsize(LOG) if os.path.exists(LOG) else 0
    with open(LOG, "a", encoding="utf-8") as out:
        out.write(f"\n==== {stamp()} {os.path.basename(design)} -> {os.path.basename(output)}, "
                  f"{passes} passes\n")
        out.flush()
        subprocess.run(["java", "-jar", jar(), "-de", design, "-do", output, "-mp", str(passes),
                        "--gui.enabled=false"], cwd=WORK, stdout=out, stderr=subprocess.STDOUT,
                       check=False)
    with open(LOG, encoding="utf-8", errors="replace") as log:
        log.seek(begin)
        counts = PASS_DONE.findall(log.read())
    return tuple(map(int, counts[-1])) if counts else None


def start():
    import pcbnew               # KiCad's Python only; the rest is plain
    os.makedirs(WORK, exist_ok=True)
    exported = os.path.join(WORK, "ns4.dsn")
    if not pcbnew.ExportSpecctraDSN(pcbnew.LoadBoard(BOARD), exported):
        raise SystemExit("the Specctra export failed")
    with open(exported, encoding="utf-8") as src:
        design = without_wiring(src.read())
    put_aside(STATE)
    put_aside(SESSION)
    with open(STATE, "w", encoding="utf-8") as dst:
        dst.write(design)
    print(f"{STATE}: the placed board, no wires; `run` routes it")
    return 0


def chunk_once(passes):
    """One chunk: STATE routed further, the result written over STATE and SESSION."""
    for path in (NEXT, NEXT_SES):
        if os.path.exists(path):
            os.remove(path)
    count = freerouting(STATE, NEXT_SES, passes)
    if count is None or not os.path.exists(NEXT_SES):
        raise SystemExit(f"the chunk saved no session; see {LOG}")
    with open(NEXT_SES, encoding="utf-8") as ses, open(STATE, encoding="utf-8") as design:
        routed = session_wiring(ses.read(), design.read())
    with open(NEXT, "w", encoding="utf-8") as out:
        out.write(routed)
    os.replace(NEXT, STATE)
    os.replace(NEXT_SES, SESSION)
    return count


def run(chunk, patience):
    if not os.path.exists(STATE):
        raise SystemExit(f"no {STATE}: `start` first")
    best, idle = None, 0
    while idle < patience:
        count = chunk_once(chunk)
        line = f"{stamp()}  {count[0]} unrouted, {count[1]} violations"
        with open(PROGRESS, "a", encoding="utf-8") as progress:
            progress.write(line + "\n")
        print(line, flush=True)
        if count[0] == 0:
            break
        idle = 0 if best is None or count[0] < best else idle + 1
        best = count[0] if best is None else min(best, count[0])
    print(f"stopped; `finish` puts {STATE} on the board")
    return 0


def finish():
    import pcbnew
    if not os.path.exists(SESSION):
        raise SystemExit(f"no {SESSION}: `start` and `run` first")
    ses = SESSION
    board = pcbnew.LoadBoard(BOARD)
    for track in list(board.GetTracks()):
        board.Remove(track)
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise SystemExit("the session's import failed")
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(BOARD)
    tracks = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    vias = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print(f"routed: {tracks} track segments, {vias} vias -> {BOARD}")
    return 0


def main(argv):
    if len(argv) < 2 or argv[1] not in ("start", "run", "finish"):
        raise SystemExit(__doc__)
    if argv[1] == "start":
        return start()
    if argv[1] == "run":
        chunk = int(argv[2]) if len(argv) > 2 else 2
        patience = int(argv[3]) if len(argv) > 3 else 5
        return run(chunk, patience)
    return finish()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
