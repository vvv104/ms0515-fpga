#!/usr/bin/env python3
"""Route kicad/ns4.kicad_pcb with Freerouting (docs/plan.md stage 8d).

Run with KiCad's own Python, which has pcbnew:

    <KiCad>/bin/python tools/route.py [vvv104] [--work DIR] start [--from BOARD | --keep]
    <KiCad>/bin/python tools/route.py [vvv104] [--work DIR] run [passes per chunk] [chunks without gain]
    <KiCad>/bin/python tools/route.py [vvv104] [--work DIR] finish [OUT.kicad_pcb]

Э3's board (kicad/ns4.kicad_pcb, work files in build/route) or, named, a
variant's (kicad/ns4-vvv104.kicad_pcb, build/route-vvv104); `--work DIR`
puts the work files elsewhere (a probe beside a routing kept).  `start` is
from the empty board.  `start --from BOARD` (ns4 for Э3's, or a variant's
name) takes the other board's routing, so `run` routes only what differs
- the two boards have one placement; `start --keep` takes the board's own
last state, when it was made again with a part added.  The routing
carried over loses the nets that lost a pin (their wires may still reach
it); a net that only gained one keeps its wires.  It is refused when a
part stands elsewhere than it stood: wires of other nets may pass where
it stands now (a routing carried across 11 moved parts on 2026-11-05 kept
12 clearance violations no pass took out).

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
of the board's wires, fills the planes and saves the board (or, with OUT
given, a copy there, the tracked board left as it is); then `tools/pcb.py
check` reads the nets back off the pads.

What the design hands over and what Freerouting is told (the routing of
2026-11-05 and KiCad's DRC of it, docs/baseline.md step 8):

- The planes.  `start` refuses a design whose layers are all `(type
  signal)`: Freerouting then routes signals over the plane layers and cuts
  the planes to pieces (that routing ran 55 m of its 79 m of track there).
  tools/pcb.py gives the plane layers the type power, which the Specctra
  export writes as `(type power)`; Freerouting makes such a layer inactive
  ("a dedicated power plane and cannot be routed") and takes the plane's
  net as routed by it - but only when the layer also carries a `(plane
  ...)`: on a small board (2026-11-07) a bare power layer took the wire,
  the same board with the planes put it on In4.Cu.  The probe of the
  six-layer board (`build/route-six`, one pass, 486 of 1258 left) had
  0 of its 3101 segments on In2.Cu or In3.Cu.
- The track's width.  That routing had 3740 segments 0.1874 mm wide, 50
  of 0.15 and 33 of 0.125 against the rule's 0.25 (199 `track_width`
  errors): not the DSN's rules (width 250 in the design and the classes)
  nor KiCad's import, but Freerouting's own "automatic neckdown" (on by
  default, router.automatic_neckdown): when a found connection cannot be
  inserted at its width it is tried again at 3/4, 3/5 and 1/2 of the
  half-width (FoundConnectionInserter.insertFanoutMicroNeckdown: 1250 ->
  937, 750, 625 in 0.1 um, so 0.1874, 0.15, 0.125 mm), and at a pin
  narrower than the track at the pin's width (none here - the least pad is
  1.44 mm).  `run` turns it off (`--router.automatic_neckdown=false`), so a
  connection that does not fit at 0.25 mm stays unrouted for the pass
  instead of coming back narrower (the probe's 3101 segments: 3020 of
  0.25 mm, 81 of the Power class's 0.6, none narrower).
- The board's edge.  KiCad's DRC keeps copper 0.5 mm from the Edge.Cuts
  line's centre (tools/pcb.py's EDGE_CLEARANCE; the four-layer routing had
  40 `copper_edge_clearance` errors, 0.35 to 0.47 mm).  Freerouting's own
  clearance to the outline is 0.25 mm by default
  (router.copper_to_edge_clearance_um); `run` gives it EDGE_UM, and
  `start` refuses a board whose setting is another.  Seen to hold on a
  small board (2026-11-07): two pins with a wall between them down to
  1 mm from the edge - by default the wire went under the wall, 0.305 mm
  from the edge; told 500 um it went the long way over the wall.

Freerouting's jar: $FREEROUTING_JAR, else
tools/freerouting/freerouting-2.5.0.jar of $MS0515_DATA (../ms0515_data).
"""
import datetime
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(ROOT, "build", "route")         # Э3's board's work files


EDGE_UM = 500               # copper to the board's edge, as KiCad's DRC checks it


def configure(variant=None, work=None):
    """The board routed and its work files: Э3's in build/route, a
    variant's (tools/pcb.py derive) in build/route-<variant>, or the
    folder `work` (relative to the repository's root)."""
    global BOARD, WORK, STATE, NEXT, SESSION, NEXT_SES, LOG, PROGRESS
    name = f"ns4-{variant}" if variant else "ns4"
    BOARD = os.path.join(ROOT, "kicad", f"{name}.kicad_pcb")
    WORK = os.path.join(ROOT, "build", f"route-{variant}" if variant else "route")
    if work:
        WORK = os.path.normpath(os.path.join(ROOT, work))
    STATE = os.path.join(WORK, "state.dsn")
    NEXT = os.path.join(WORK, "state.next.dsn")
    SESSION = os.path.join(WORK, "state.ses")
    NEXT_SES = os.path.join(WORK, "state.next.ses")
    LOG = os.path.join(WORK, "freerouting.log")
    PROGRESS = os.path.join(WORK, "progress.txt")


configure()

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
    """A Specctra s-expression as nested lists of strings.  A design's
    header names its quote character with a lone one, `(string_quote ")`."""
    text = text.replace('(string_quote ")', "(string_quote quote)")
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


def session_wiring(ses_text, design_text, drop=()):
    """The design with the session's wires and vias as its wiring, but
    those of the nets in `drop`.

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
        if net[1] in drop:
            continue
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


def design_nets(design_text):
    """{net: {pin}} of a design's network section."""
    network = child(sexpr(design_text), "network")
    return {n[1]: set(child(n, "pins")[1:]) for n in network[1:]
            if isinstance(n, list) and n[0] == "net" and child(n, "pins")}


def design_layers(design_text):
    """{layer: type} of a design's structure section, in the stack's order."""
    structure = child(sexpr(design_text), "structure") or ["structure"]
    return {layer[1]: child(layer, "type")[1] for layer in structure[1:]
            if isinstance(layer, list) and layer[0] == "layer" and child(layer, "type")}


def check_layers(design_text):
    """The design's layers told; refused when a plane (a layer with a
    (plane ...) polygon on it in the structure section) lies on a signal
    layer, when a power layer carries no plane, or when there is no plane
    at all.  Freerouting keeps its wires off a layer only when it is both
    `(type power)` and has a plane: a bare power layer is routed over, a
    plane on a signal layer is cut to pieces (the routing of 2026-11-05,
    docs/baseline.md step 8; both seen on a small board, 2026-11-07)."""
    layers = design_layers(design_text)
    structure = child(sexpr(design_text), "structure") or ["structure"]
    planes = {child(p, "polygon")[1] for p in structure[1:] if isinstance(p, list) and p[0] == "plane"}
    wrong = sorted(layer for layer in planes if layers.get(layer) != "power")
    if wrong:
        raise SystemExit(f"{', '.join(wrong)}: a plane on a signal layer - tools/pcb.py makes the "
                         "plane layers power layers; make the board again")
    bare = sorted(layer for layer, kind in layers.items() if kind == "power" and layer not in planes)
    if bare:
        raise SystemExit(f"{', '.join(bare)}: a power layer with no plane on it - Freerouting would "
                         "route over it; tools/pcb.py puts a plane on every power layer")
    if "power" not in layers.values():
        raise SystemExit("no power layer in the design: tools/pcb.py's board has its planes on "
                         "layers of type power; make the board again")
    by_type = {kind: [layer for layer, t in layers.items() if t == kind] for kind in ("signal", "power")}
    return ", ".join(f"{' '.join(names)} {kind}" for kind, names in by_type.items() if names)


def design_places(design_text):
    """{ref: (x, y, side, rotation)} of a design's placement section."""
    placement = child(sexpr(design_text), "placement") or ["placement"]
    return {p[1]: tuple(p[2:6]) for comp in placement[1:] if isinstance(comp, list)
            for p in comp[1:] if isinstance(p, list) and p[0] == "place"}


def carried(ses_text, old_design, new_design):
    """The new design with the old one's routing (its session) on it, but
    the nets that lost a pin since - their wires may still reach it.  A net
    that only gained a pin keeps its wires, the new pin is routed to them.
    Refused when a part moved: the old wires know nothing of where it
    stands now.  Returns the design and the nets left unrouted."""
    old, new = design_nets(old_design), design_nets(new_design)
    was, now = design_places(old_design), design_places(new_design)
    moved = sorted(ref for ref, at in now.items() if ref in was and was[ref] != at)
    if moved:
        raise SystemExit(f"{len(moved)} parts moved since that routing ({', '.join(moved[:8])}"
                         f"{', ...' if len(moved) > 8 else ''}): start from the empty board")
    drop = sorted(net for net, pins in old.items() if pins - new.get(net, set()))
    return session_wiring(ses_text, new_design, drop=drop), drop


def carry_from(options):
    """The work folder whose routing `start` takes: none (the empty board),
    the board's own (--keep), another board's (--from ns4 | <variant>)."""
    if "--keep" in options:
        return WORK
    if "--from" not in options:
        return None
    name = options[options.index("--from") + 1:][:1]
    if not name:
        raise SystemExit("--from which board: ns4 or a variant's name")
    other = BASE if name[0] == "ns4" else os.path.join(ROOT, "build", f"route-{name[0]}")
    if os.path.normpath(other) == os.path.normpath(WORK):
        raise SystemExit("--from the board itself: that is --keep")
    return other


def freerouting_args(design, output, passes):
    """Freerouting's command line: the design, the session to write, the
    passes; no neckdown (the rule's width kept), the edge's clearance."""
    return ["java", "-jar", jar(), "-de", design, "-do", output, "-mp", str(passes),
            "--gui.enabled=false", "--router.automatic_neckdown=false",
            f"--router.copper_to_edge_clearance_um={EDGE_UM}"]


def freerouting(design, output, passes):
    """One Freerouting run; its log is appended to LOG, its last count returned."""
    begin = os.path.getsize(LOG) if os.path.exists(LOG) else 0
    with open(LOG, "a", encoding="utf-8") as out:
        out.write(f"\n==== {stamp()} {os.path.basename(design)} -> {os.path.basename(output)}, "
                  f"{passes} passes\n")
        out.flush()
        subprocess.run(freerouting_args(design, output, passes), cwd=WORK, stdout=out,
                       stderr=subprocess.STDOUT, check=False)
    with open(LOG, encoding="utf-8", errors="replace") as log:
        log.seek(begin)
        counts = PASS_DONE.findall(log.read())
    return tuple(map(int, counts[-1])) if counts else None


def start(base=None):
    """The board as a design to route: with no wires, or with the routing
    of the work folder `base` (carry_from) on it."""
    import pcbnew               # KiCad's Python only; the rest is plain
    os.makedirs(WORK, exist_ok=True)
    exported = os.path.join(WORK, "ns4.dsn")
    board = pcbnew.LoadBoard(BOARD)
    edge = pcbnew.ToMM(board.GetDesignSettings().m_CopperEdgeClearance) * 1000
    if round(edge) != EDGE_UM:
        raise SystemExit(f"{BOARD} keeps copper {edge:.0f} um from the edge, Freerouting is told {EDGE_UM}")
    if not pcbnew.ExportSpecctraDSN(board, exported):
        raise SystemExit("the Specctra export failed")
    with open(exported, encoding="utf-8") as src:
        design = without_wiring(src.read())
    layers = check_layers(design)
    told = f"the placed board, no wires; {layers}"
    if base:
        ses_path, old_path = (os.path.join(base, n) for n in ("state.ses", "state.dsn"))
        if not os.path.exists(ses_path):
            raise SystemExit(f"no {ses_path} to start from")
        with open(ses_path, encoding="utf-8") as ses, open(old_path, encoding="utf-8") as old:
            design, drop = carried(ses.read(), old.read(), design)
        told = f"{base}'s routing, but {', '.join(drop) or 'no net'}"
    put_aside(STATE)
    put_aside(SESSION)
    with open(STATE, "w", encoding="utf-8") as dst:
        dst.write(design)
    print(f"{STATE}: {told}; `run` routes it")
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


def finish(out=None):
    """The session on the board, saved as the board or as `out`."""
    import pcbnew
    if not os.path.exists(SESSION):
        raise SystemExit(f"no {SESSION}: `start` and `run` first")
    ses = SESSION
    board = pcbnew.LoadBoard(BOARD)
    for track in list(board.GetTracks()):
        board.Delete(track)     # Remove leaves pcbnew's object to Python, which crashed
    if not pcbnew.ImportSpecctraSES(board, ses):
        raise SystemExit("the session's import failed")
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    out = os.path.join(ROOT, out) if out else BOARD
    board.Save(out)
    tracks = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_TRACK")
    vias = sum(1 for t in board.GetTracks() if t.GetClass() == "PCB_VIA")
    print(f"routed: {tracks} track segments, {vias} vias -> {out}")
    return 0


COMMANDS = ("start", "run", "finish")


def parse(argv):
    """(variant, work folder, command, its arguments) off the command line:
    `route.py [variant] [--work DIR] command [args]`."""
    argv = argv[1:]
    variant = work = None
    if argv and argv[0] not in COMMANDS and argv[0] != "--work":
        variant, argv = argv[0], argv[1:]
    if argv[:1] == ["--work"]:
        if len(argv) < 2:
            raise SystemExit("--work which folder")
        work, argv = argv[1], argv[2:]
    if not argv or argv[0] not in COMMANDS:
        raise SystemExit(__doc__)
    return variant, work, argv[0], argv[1:]


def main(argv):
    variant, work, command, args = parse(argv)
    configure(variant, work)
    if command == "start":
        return start(carry_from(args))
    if command == "run":
        chunk = int(args[0]) if args else 2
        patience = int(args[1]) if len(args) > 1 else 5
        return run(chunk, patience)
    return finish(args[0] if args else None)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
