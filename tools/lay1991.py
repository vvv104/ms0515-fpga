#!/usr/bin/env python3
"""The 1991 board's outer copper as netted tracks (docs/plan.md stage 8d).

netlist/lines-1991.json (tools/copper1991.py) holds Mick's layers 1, 2, 7
and 8 as centre lines, each with its width and the holes its copper area
reaches.  This module gives them nets and turns them into tracks; tools/pcb.py
lays them on the board, locked, and Freerouting routes the rest inside.

- A line's net is the net of the hole nearest to it along the copper: the
  lines of a layer are a graph (their ends the nodes, an end on a hole its
  copper reaches the hole's node), and every hole with a net - a part's
  pad, or a via whose copper group (netlist/copper.json) reaches one net -
  sends its net out along the shortest paths.  A line whose two ends are
  nearest to holes of two nets is where the trace read merged two runs: it
  is cut.  Copper that reaches no netted hole is left out: it continues on
  the inner layers, which no scan shows, and is routed again there.
- A line on a hole runs on to the hole's centre.
- A run shorter than a stub that comes within the gap of another net's
  copper or hole got a neighbour's net from the read: it is taken off.
- Every hole no part stands on that a track ends on takes a via, with the
  net of the tracks ending there.
Coordinates are Mick's pixels throughout (2.54 mm a 39.4 px, as
netlist/placement*.csv).
"""
import heapq
import math
from collections import Counter, defaultdict

REACH_PX = 24             # a line's end this near a hole its copper reaches is on it
CELL = 40


def _near_hole(p, allowed, holes):
    best, bd = None, REACH_PX
    for i in allowed:
        d = math.dist(p, holes[i])
        if d < bd:
            best, bd = i, d
    return best


def _length(pts):
    return sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))


def _graph_into(adj, lines, holes, layer):
    """Add a layer's lines to adj (their ends as nodes - ("h", hole) where an
    end is on a hole its copper reaches, every layer's, a hole being plated
    through; ("p", layer, x, y) otherwise); return adj and the ends."""
    ends = []
    for ln in lines:
        pts, allowed = ln["pts"], ln.get("holes", ())
        nodes = []
        for p in (pts[0], pts[-1]):
            h = _near_hole(p, allowed, holes)
            nodes.append(("h", h) if h is not None else ("p", layer, round(p[0]), round(p[1])))
        ends.append(tuple(nodes))
        L = _length(pts)
        adj[nodes[0]].append((nodes[1], L))
        adj[nodes[1]].append((nodes[0], L))
    return adj, ends


def _spread(adj, net_of):
    """Each node's net: the nearest netted hole's along the lines."""
    dist, label, heap = {}, {}, []
    for node in adj:
        if node[0] == "h" and net_of(node[1]):
            dist[node], label[node] = 0.0, net_of(node[1])
            heap.append((0.0, node))
    heapq.heapify(heap)
    while heap:
        d, node = heapq.heappop(heap)
        if d > dist.get(node, math.inf):
            continue
        for other, L in adj[node]:
            if other[0] == "h" and net_of(other[1]):
                continue                     # a netted hole keeps its own
            if d + L < dist.get(other, math.inf):
                dist[other], label[other] = d + L, label[node]
                heapq.heappush(heap, (d + L, other))
    return label


def label(lines, holes, hole_net, group_net):
    """[[net, width, pts]] of a layer's lines (see the module's text).
    hole_net: {hole: net} of the parts' pads; group_net: {hole: net} of the
    vias whose copper group reaches one net."""
    return label_layers({None: lines}, holes, hole_net, group_net)[None]


def label_layers(layers, holes, hole_net, group_net):
    """{layer: [[net, width, pts]]}: label() over all the layers at once, a
    net going on through the holes from layer to layer."""
    def net_of(i):
        return hole_net.get(i) or group_net.get(i)
    adj, ends = defaultdict(list), {}
    for layer, lines in layers.items():
        adj, ends[layer] = _graph_into(adj, lines, holes, layer)
    lab = _spread(adj, net_of)
    return {layer: _tracks(lines, ends[layer], lab, holes) for layer, lines in layers.items()}


def _tracks(lines, ends, lab, holes):
    out = []
    for ln, (na, nb) in zip(lines, ends):
        la, lb = lab.get(na), lab.get(nb)
        if (la is None and lb is None) or (la and lb and la != lb):
            continue
        pts = [list(p) for p in ln["pts"]]
        if na[0] == "h":
            pts.insert(0, [float(c) for c in holes[na[1]]])
        if nb[0] == "h":
            pts.append([float(c) for c in holes[nb[1]]])
        out.append([la or lb, ln["w"], pts])
    return out


def _seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    k = 0.0 if dx == dy == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(a[0] + k * dx - p[0], a[1] + k * dy - p[1])


def _segs_dist(a, b, c, d):
    return min(_seg_dist(a, c, d), _seg_dist(b, c, d), _seg_dist(c, a, b), _seg_dist(d, a, b))


def _too_near(track, others, holes, hole_net, gap_px, hole_r_px):
    net, w, pts = track
    segs = list(zip(pts, pts[1:])) or [(pts[0], pts[0])]
    for i, h in enumerate(holes):
        if hole_net.get(i) not in (None, net):
            r = hole_r_px.get(i, 0) if isinstance(hole_r_px, dict) else hole_r_px
            if any(_seg_dist(h, a, b) < r + w / 2 + gap_px for a, b in segs):
                return True
    for onet, ow, opts in others:
        if onet == net:
            continue
        for c, d in zip(opts, opts[1:]):
            if any(_segs_dist(a, b, c, d) < (w + ow) / 2 + gap_px for a, b in segs):
                return True
    return False


def drop_stubs(tracks, holes, hole_net, stub_px, gap_px, hole_r_px=8.0):
    """The tracks without the short runs that touch another net's copper or
    hole; and how many went."""
    kept, gone = {}, 0
    for layer, runs in tracks.items():
        keep = []
        for t in runs:
            if _length(t[2]) < stub_px and _too_near(t, runs, holes, hole_net, gap_px, hole_r_px):
                gone += 1
                continue
            keep.append(t)
        kept[layer] = keep
    return kept, gone


def drop_scraps(tracks, holes, hole_r_px):
    """The tracks without the runs that reach no hole, alone or through the runs of
    their layer they share a point with (pieces left by the cuts); and how many went."""
    near = defaultdict(list)
    for i, (x, y) in enumerate(holes):
        near[(int(x // CELL), int(y // CELL))].append(i)

    def on_hole(p):
        cx, cy = int(p[0] // CELL), int(p[1] // CELL)
        return any(math.dist(p, holes[i]) <= hole_r_px
                   for gx in (-1, 0, 1) for gy in (-1, 0, 1) for i in near[(cx + gx, cy + gy)])
    kept, gone = {}, 0
    for layer, runs in tracks.items():
        parent = list(range(len(runs)))

        def find(a):
            while parent[a] != a:
                parent[a] = parent[parent[a]]
                a = parent[a]
            return a
        first = {}
        for k, (_, _, pts) in enumerate(runs):
            for p in pts:
                key = (round(p[0], 1), round(p[1], 1))
                if key in first:
                    parent[find(k)] = find(first[key])
                else:
                    first[key] = k
        anchored = {find(k) for k, (_, _, pts) in enumerate(runs) if on_hole(pts[0]) or on_hole(pts[-1])}
        kept[layer] = [t for k, t in enumerate(runs) if find(k) in anchored]
        gone += len(runs) - len(kept[layer])
    return kept, gone


def _point_seg(p, s):
    return _seg_dist(p, (s[1], s[2]), (s[3], s[4]))


def _pad_dist(pad, s):
    """From the segment's centre line to the pad's copper: a round pad (x, y, r, net),
    or a square one (x, y, r, net, half width, half height) by its sides."""
    x, y = pad[0], pad[1]
    if len(pad) < 6:
        return _point_seg((x, y), s) - pad[2]
    hw, hh = pad[4], pad[5]
    a, b = (s[1], s[2]), (s[3], s[4])

    def to_box(p):
        return math.hypot(max(abs(p[0] - x) - hw, 0.0), max(abs(p[1] - y) - hh, 0.0))
    corners = [(x + sx * hw, y + sy * hh) for sx in (-1, 1) for sy in (-1, 1)]
    d = min([to_box(a), to_box(b)] + [_seg_dist(c, a, b) for c in corners])
    if d > 0 and _crosses_box(a, b, x, y, hw, hh):
        return 0.0
    return d


def _crosses_box(a, b, x, y, hw, hh):
    """Does the segment a-b pass through the box (both ends outside)?"""
    for t in (k / 32 for k in range(33)):
        px, py = a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])
        if abs(px - x) <= hw and abs(py - y) <= hh:
            return True
    return False


def fit_widths(segs, pads, width, floor, clear, cell=5.0, fixed=()):
    """Each segment's width (mm): `width`, or as much less as keeps `clear` from
    another net's pad (x, y, r, net) on any layer, from another net's fixed run
    (layer, x0, y0, x1, y1, half width, net) on its layer and, sharing the narrowing,
    from another net's segment on its layer; never below `floor`.  The traces' centre
    lines are off by up to half a pixel of Mick's images, so the board's width does
    not fit everywhere it did on the board."""
    grid = defaultdict(list)
    for k, s in enumerate(segs):
        for gx in range(int(min(s[1], s[3]) // cell), int(max(s[1], s[3]) // cell) + 1):
            for gy in range(int(min(s[2], s[4]) // cell), int(max(s[2], s[4]) // cell) + 1):
                grid[(gx, gy)].append(k)
    pad_grid, fixed_grid = defaultdict(list), defaultdict(list)
    for p in pads:
        pad_grid[(int(p[0] // cell), int(p[1] // cell))].append(p)
    for f in fixed:
        for gx in range(int(min(f[1], f[3]) // cell), int(max(f[1], f[3]) // cell) + 1):
            for gy in range(int(min(f[2], f[4]) // cell), int(max(f[2], f[4]) // cell) + 1):
                fixed_grid[(gx, gy)].append(f)
    out = []
    for s in segs:
        w = width
        cells = [(gx, gy) for gx in range(int(min(s[1], s[3]) // cell) - 1, int(max(s[1], s[3]) // cell) + 2)
                 for gy in range(int(min(s[2], s[4]) // cell) - 1, int(max(s[2], s[4]) // cell) + 2)]
        for j in {j for c in cells for j in grid.get(c, ())}:
            o = segs[j]
            if o[0] == s[0] and o[5] != s[5]:
                d = min(_point_seg((s[1], s[2]), o), _point_seg((s[3], s[4]), o),
                        _point_seg((o[1], o[2]), s), _point_seg((o[3], o[4]), s))
                w = min(w, d - clear)
        for p in (p for c in cells for p in pad_grid.get(c, ())):
            if p[3] != s[5]:
                w = min(w, 2 * (_pad_dist(p, s) - clear))
        for o in (f for c in cells for f in fixed_grid.get(c, ())):
            if o[0] == s[0] and o[6] != s[5]:
                d = min(_point_seg((s[1], s[2]), o), _point_seg((s[3], s[4]), o),
                        _point_seg((o[1], o[2]), s), _point_seg((o[3], o[4]), s))
                w = min(w, 2 * (d - o[5] - clear))
        out.append(max(floor, w))
    return out


def fit_runs(segs, pads, width, floor, clear, fixed=(), piece=2.54):
    """[(segment, width)]: fit_widths on the segments cut in pieces no longer than
    `piece`, the pieces of one segment of one width joined again - so a long run
    narrows only where it is tight."""
    pieces, of = [], []
    for k, (layer, x0, y0, x1, y1, net) in enumerate(segs):
        n = max(1, math.ceil(math.dist((x0, y0), (x1, y1)) / piece))
        for q in range(n):
            a, b = q / n, (q + 1) / n
            pieces.append((layer, x0 + a * (x1 - x0), y0 + a * (y1 - y0), x0 + b * (x1 - x0), y0 + b * (y1 - y0), net))
            of.append(k)
    widths = fit_widths(pieces, pads, width, floor, clear, fixed=fixed)
    out = []
    for k, s, w in zip(of, pieces, widths):
        if out and out[-1][2] == k and abs(out[-1][1] - w) < 1e-9:
            seg = out[-1][0]
            out[-1] = ((seg[0], seg[1], seg[2], s[3], s[4], seg[5]), w, k)
        else:
            out.append((s, w, k))
    return [(s, w) for s, w, _ in out]


def vias(tracks, holes, pad_holes):
    """{hole: net} for the holes no part stands on that a track ends on: the
    net with the most track length ending there (a stub loses to a run)."""
    where = {(round(x, 1), round(y, 1)): i for i, (x, y) in enumerate(holes)}
    votes = defaultdict(Counter)
    for runs in tracks.values():
        for net, _, pts in runs:
            for end in (pts[0], pts[-1]):
                i = where.get((round(end[0], 1), round(end[1], 1)))
                if i is not None and i not in pad_holes:
                    votes[i][net] += _length(pts)
    return {i: c.most_common(1)[0][0] for i, c in votes.items()}
