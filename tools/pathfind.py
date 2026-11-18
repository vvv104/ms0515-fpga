#!/usr/bin/env python3
"""PathFinder on the 1991 board's hidden layers: Э3's nets joined inside, between the
holes the board already has.

    <KiCad>/bin/python tools/pathfind.py export [BOARD]     - build/pathfind/board.json
    python tools/pathfind.py route [ITERATIONS] [LAYER...]   - build/pathfind/routes.json
    python tools/pathfind.py smooth                          - routes.json's staircases pulled
                                       straight where the line keeps clear of other nets
    <KiCad>/bin/python tools/pathfind.py apply OUT [BOARD]   - the routes on a copy of the board
    <KiCad>/bin/python tools/pathfind.py planes [ROUTED]     - the plane pins the routing's
                                       vias cut off the planes into board.json; `route`, `apply` again

`pathfind.py ext ...` does the same on the memory expansion module's board
(kicad/ext.kicad_pcb, build/pathfind-ext): every track of it on its two outer layers,
on a 0.635 mm grid, a layer changed only in a pad, a hole of the original
(netlist/ext/holes.csv) or one of lattice()'s, a hole made a via as the module's own
(tools/pcb_ext.py).

`export` takes the placed board with Mick's outer copper laid (kicad/ns4.kicad_pcb) and the
1991 holes (netlist/copper.json); `route` runs on numpy and scipy, which KiCad's Python
has not, so it is the system's; `apply` writes the routes as tracks and vias.

Every hole of a real board shows on its outer layers, so the 1991 board's inner layers
changed layer only in holes Mick's scans show: a part's pin, a via of the outer copper,
or a hole no outer copper reaches (a via of the inner layers alone).  The routes keep to
that: tracks on a 1.27 mm grid (one track between two pins 2.54 mm apart, as the
board's own photos show), layer changes only in holes - by default on In3 and In4, the
two hidden signal layers, and on In1 and In6 where Mick's layers 2 and 7 leave room (In3
and In4 alone left 16 cells shared after 400 rounds, 2026-11-10).  PathFinder (McMurchie and Ebeling, 1995 - the FPGA router): each
net is routed alone, sharing allowed; a cell more nets use than it holds grows dearer,
at once and for good, until no cell is shared.
"""
import json
import math
import os
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WORK = os.path.join(ROOT, "build", "pathfind")
PITCH = 1.27                 # mm: half the 2.54 mm hole grid
WIDTH, CLEAR, EDGE = 0.25, 0.2, 0.5
HOLE_CLEAR = 0.25            # the board's rule from a hole; a pad with no net is taken as one
FREE_R = 0.5                 # a hole no copper takes: a via's ring (pcb.VIA_1991)
REACH = 0.9                  # a pad's centre to the farthest cell a stub may join it from
VIA_COST = 2.0
PRES_MAX = 1e6            # the price of sharing stops growing here...
FULL_EVERY = 10          # every tenth round reroutes every net, not the sharing ones alone
ROUNDS = 100              # `route`'s rounds unless told
PRICES = {}               # PathFinder.run()'s prices of sharing, its own unless set
FORBID = 1e18             # ...far below a cell of another net's
PLANES = ("GND", "+5V")
OPEN, SHUT = -1, -2
LAYERS = ["In1.Cu", "In3.Cu", "In4.Cu", "In6.Cu"]
HOLES = "ns4"             # whose free holes `export` takes, and what a free hole becomes
TAIL = 0.15               # mm: smooth() lays no run shorter, a slant to an off-grid end instead


def seg_dist(px, py, x0, y0, x1, y1):
    """Distances of points (px, py) to the segment (x0, y0)-(x1, y1)."""
    dx, dy = x1 - x0, y1 - y0
    k = ((px - x0) * dx + (py - y0) * dy) / (dx * dx + dy * dy or 1.0)
    k = np.clip(k, 0.0, 1.0)
    return np.hypot(x0 + k * dx - px, y0 + k * dy - py)


def pad_radius(square, sx, sy, drill):
    """How far a pad's copper (or its bare hole) reaches from its centre: a square one to its corners."""
    return max(math.hypot(sx, sy) if square else max(sx, sy), drill) / 2


class Model:
    """The routing grid: cells per layer, who may use each, the islands to join."""

    def __init__(self, board, layers):
        self.board, self.layers = board, list(layers)
        x0, y0, x1, y1 = board["box"]
        self.ox, self.oy = self._origin(0, x0), self._origin(1, y0)
        self.nx = int((x1 - self.ox) // PITCH) + 1
        self.ny = int((y1 - self.oy) // PITCH) + 1
        self.per_layer = self.nx * self.ny
        names = sorted({p[3] for p in board["pads"] if p[3]} | {v[3] for v in board["vias"] if v[3]})
        self.nets = names
        self.net_id = {n: k for k, n in enumerate(names)}
        self.free_base = len(names)
        own = np.full((len(self.layers), self.ny, self.nx), OPEN, dtype=np.int32)
        self._own = own
        # the steps between cells, by their middles: across (j, i)-(j, i+1) and down (j, i)-(j+1, i)
        self._step = {"across": np.full((len(self.layers), self.ny, self.nx - 1), OPEN, dtype=np.int32),
                      "down": np.full((len(self.layers), self.ny - 1, self.nx), OPEN, dtype=np.int32)}
        self._claim_all()
        self._shut_edge()
        self.owner = own.reshape(-1)
        self.via_cells, self.via_hole = self._holes()
        self.items = self._items()
        self.islands = {n: self._island_cells(n, isl) for n, isl in self._islands().items()}

    def _origin(self, axis, low):
        pads = np.array([p[axis] for p in self.board["pads"]] or [low])
        a = np.angle(np.exp(2j * np.pi * pads / PITCH).mean()) / (2 * np.pi) * PITCH
        return low + (a - low) % PITCH

    def cell(self, x, y):
        return int(round((x - self.ox) / PITCH)), int(round((y - self.oy) / PITCH))

    def node(self, layer, i, j):
        return layer * self.per_layer + j * self.nx + i

    def layer_of(self, node):
        return node // self.per_layer

    def xy(self, node):
        c = node % self.per_layer
        return self.ox + (c % self.nx) * PITCH, self.oy + (c // self.nx) * PITCH

    def _cells(self, x0, y0, x1, y1, reach, grid=None):
        """(i, j) of the cells (or the steps' middles of grid) nearer than reach to the
        segment (a point when both ends meet)."""
        if grid is None:
            dx, dy, nx, ny = 0.0, 0.0, self.nx, self.ny
        elif grid == "across":
            dx, dy, nx, ny = 0.5, 0.0, self.nx - 1, self.ny
        else:
            dx, dy, nx, ny = 0.0, 0.5, self.nx, self.ny - 1
        ox, oy = self.ox + dx * PITCH, self.oy + dy * PITCH
        i0 = max(0, int(math.floor((min(x0, x1) - reach - ox) / PITCH)))
        i1 = min(nx - 1, int(math.ceil((max(x0, x1) + reach - ox) / PITCH)))
        j0 = max(0, int(math.floor((min(y0, y1) - reach - oy) / PITCH)))
        j1 = min(ny - 1, int(math.ceil((max(y0, y1) + reach - oy) / PITCH)))
        if i1 < i0 or j1 < j0:
            return np.zeros(0, int), np.zeros(0, int)
        jj, ii = np.mgrid[j0:j1 + 1, i0:i1 + 1]
        d = seg_dist(ox + ii * PITCH, oy + jj * PITCH, x0, y0, x1, y1)
        m = d < reach
        return ii[m], jj[m]

    def _claim(self, layers, x0, y0, x1, y1, reach, who):
        """Cells nearer than reach to the copper, and the steps whose straight run may come
        nearer than reach: their middles within hypot(reach, half a step)."""
        targets = [(self._own, self._cells(x0, y0, x1, y1, reach))]
        far = math.hypot(reach, PITCH / 2)
        targets += [(own, self._cells(x0, y0, x1, y1, far, g)) for g, own in self._step.items()]
        for own, (ii, jj) in targets:
            for k in layers:
                sub = own[k, jj, ii]
                own[k, jj, ii] = np.where((sub == OPEN) | (sub == who), who, SHUT)

    def _claim_all(self):
        everywhere = range(len(self.layers))
        for x, y, r, net, *_ in self.board["pads"]:
            who, gap = (self.net_id[net], CLEAR) if net else (SHUT, HOLE_CLEAR)
            self._claim(everywhere, x, y, x, y, r + gap + WIDTH / 2, who)
        for x, y, r, net in self.board["vias"]:
            self._claim(everywhere, x, y, x, y, r + CLEAR + WIDTH / 2, self.net_id[net])
        for k, (x, y) in enumerate(self.board["free"]):
            self._claim(everywhere, x, y, x, y, FREE_R + CLEAR + WIDTH / 2, self.free_base + k)
        for layer, x0, y0, x1, y1, w, net in self.board["tracks"]:
            if layer in self.layers:
                self._claim([self.layers.index(layer)], x0, y0, x1, y1, w / 2 + CLEAR + WIDTH / 2,
                            self.net_id.get(net, SHUT))

    def step_owner(self, a, b):
        """Who may take the step between the cells a and b of one layer (OPEN: anyone)."""
        k, ca, cb = self.layer_of(a), a % self.per_layer, b % self.per_layer
        ca, cb = min(ca, cb), max(ca, cb)
        j, i = divmod(ca, self.nx)
        return self._step["across" if cb == ca + 1 else "down"][k, j, i]

    def edge_shut_for(self, a, b, net):
        who = self.step_owner(a, b)
        return bool(who == SHUT or (0 <= who < self.free_base and who != self.net_id[net]))

    def _shut_edge(self):
        x0, y0, x1, y1 = self.board["box"]
        gap = EDGE + WIDTH / 2
        xs = self.ox + np.arange(self.nx) * PITCH
        ys = self.oy + np.arange(self.ny) * PITCH
        self._own[:, :, (xs < x0 + gap) | (xs > x1 - gap)] = SHUT
        self._own[:, (ys < y0 + gap) | (ys > y1 - gap), :] = SHUT
        for cx0, cy0, cx1, cy1 in self.board.get("cuts", []):     # a notch cut out of the outline
            self._own[:, ((ys > cy0 - gap) & (ys < cy1 + gap))[:, None]
                      & ((xs > cx0 - gap) & (xs < cx1 + gap))[None, :]] = SHUT

    def _holes(self):
        """The cells a route may change layer in, each with its hole: (x, y, free hole or None)."""
        hole = {}
        spots = [(x, y, r, None) for x, y, r, net, *_ in self.board["pads"] if net]
        spots += [(x, y, r, None) for x, y, r, _ in self.board["vias"]]
        spots += [(x, y, FREE_R, k) for k, (x, y) in enumerate(self.board["free"])]
        for x, y, r, free in spots:
            for i, j in zip(*self._cells(x, y, x, y, max(r + WIDTH / 2, REACH))):
                hole.setdefault(j * self.nx + i, (x, y, free))
        return sorted(hole), hole

    def _items(self):
        """{net: [(layer or None, x0, y0, x1, y1, r)]}: the copper of each net; a pad by
        the circle inside it (a square one's sides), so copper past its corners is not on it."""
        items = {}
        for x, y, r, net, *inner in self.board["pads"]:
            if net:
                items.setdefault(net, []).append((None, x, y, x, y, inner[1] if len(inner) > 1 else r))
        for x, y, r, net in self.board["vias"]:
            items.setdefault(net, []).append((None, x, y, x, y, r))
        for layer, x0, y0, x1, y1, w, net in self.board["tracks"]:
            if net in self.net_id:
                items.setdefault(net, []).append((layer, x0, y0, x1, y1, w / 2))
        return items

    def _islands(self):
        """{net: [[item]]}: each net's copper in touching groups, with a hole in each."""
        out = {}
        for net, items in self.items.items():
            if net in PLANES:
                if net in self.board.get("plane_splits", {}):
                    out[net] = self._plane_islands(net, items)
                continue
            parent = list(range(len(items)))

            def find(a):
                while parent[a] != a:
                    parent[a] = parent[parent[a]]
                    a = parent[a]
                return a
            for a, b in touching(items):
                parent[find(a)] = find(b)
            groups = {}
            for k in range(len(items)):
                groups.setdefault(find(k), []).append(items[k])
            holed = [g for g in groups.values() if any(it[0] is None for it in g)]
            if len(holed) > 1:
                out[net] = holed
        return out

    def _plane_islands(self, net, items):
        """[[item]]: the plane's net as the plane holds it, and each piece of its fill cut off
        the plane (board["plane_splits"]: {net: [[pin]]}, tools/readback.py) with its pads."""
        at = {p[4]: (p[0], p[1]) for p in self.board["pads"]}
        cut = []
        for pins in self.board["plane_splits"][net]:
            where = {at[name] for name in pins if name in at}
            cut.append([it for it in items if it[0] is None and (it[1], it[2]) in where])
        taken = {id(it) for group in cut for it in group}
        return [[it for it in items if id(it) not in taken]] + [g for g in cut if g]

    def _island_cells(self, net, islands):
        """[(set of nodes, items)]: per island the cells a route may end in."""
        who = self.net_id[net]
        out = []
        for items in islands:
            nodes = set()
            for layer, x0, y0, x1, y1, r in items:
                ks = range(len(self.layers)) if layer is None else \
                    [self.layers.index(layer)] if layer in self.layers else []
                ii, jj = self._cells(x0, y0, x1, y1, max(r + WIDTH / 2, REACH) if layer is None else r + WIDTH / 2)
                for k in ks:
                    for i, j in zip(ii, jj):
                        n = self.node(k, i, j)
                        if self.owner[n] == who:
                            nodes.add(n)
            out.append((nodes, items))
        return out


def touching(items):
    """Pairs of items (index, index) whose copper touches: a hole is on every layer."""
    pairs = []
    for a in range(len(items)):
        la, ax0, ay0, ax1, ay1, ar = items[a]
        for b in range(a + 1, len(items)):
            lb, bx0, by0, bx1, by1, br = items[b]
            if la is not None and lb is not None and la != lb:
                continue
            d = min(seg_dist(ax0, ay0, bx0, by0, bx1, by1), seg_dist(ax1, ay1, bx0, by0, bx1, by1),
                    seg_dist(bx0, by0, ax0, ay0, ax1, ay1), seg_dist(bx1, by1, ax0, ay0, ax1, ay1))
            if d < ar + br:
                pairs.append((a, b))
    return pairs


class Result:
    def __init__(self, paths, failed, overused, iterations):
        self.paths, self.failed, self.overused, self.iterations = paths, failed, overused, iterations


class PathFinder:
    """Negotiated congestion over the model's grid (scipy's Dijkstra on a sparse graph)."""

    def __init__(self, model, log=None):
        from scipy.sparse import csr_matrix
        self.m, self.log = model, log
        rows, cols, whose = self._edges()
        n = len(model.owner)
        g = csr_matrix((np.arange(1, len(rows) + 1, dtype=float), (rows, cols)), shape=(n, n))
        g.sort_indices()
        self.g = g
        whose = whose[g.data.astype(np.int64) - 1]               # each step's owner, in the graph's order
        self.step_net = np.where((whose >= 0) & (whose < model.free_base), whose, -1)
        src = np.repeat(np.arange(n), np.diff(g.indptr))
        self.extra = np.where(src // model.per_layer != g.indices // model.per_layer, VIA_COST, 0.0)
        self.res = np.arange(n)
        free = model.owner >= model.free_base
        self.res[free] = n + model.owner[free] - model.free_base
        self.n_res = n + len(model.board["free"])
        own = model.owner
        self.foreign = (own >= 0) & (own < model.free_base)      # a net's own cells

    def _edges(self):
        m = self.m
        own = m.owner.reshape(len(m.layers), m.ny, m.nx)
        rows, cols, whose = [], [], []
        idx = np.arange(m.per_layer).reshape(m.ny, m.nx)
        for k in range(len(m.layers)):
            ok = (own[k] != SHUT).reshape(-1)
            base = k * m.per_layer
            for a, b, step in ((idx[:, :-1], idx[:, 1:], m._step["across"][k]),
                               (idx[:-1, :], idx[1:, :], m._step["down"][k])):
                a, b, step = a.reshape(-1), b.reshape(-1), step.reshape(-1)
                both = ok[a] & ok[b] & (step != SHUT)
                rows += [base + a[both], base + b[both]]
                cols += [base + b[both], base + a[both]]
                whose += [step[both], step[both]]
        cells = np.array(m.via_cells, dtype=np.int64)
        for k in range(len(m.layers)):
            for q in range(k + 1, len(m.layers)):
                both = (own[k].reshape(-1)[cells] != SHUT) & (own[q].reshape(-1)[cells] != SHUT)
                a, b = k * m.per_layer + cells[both], q * m.per_layer + cells[both]
                rows += [a, b]
                cols += [b, a]
                whose += [np.full(len(a), OPEN), np.full(len(a), OPEN)]
        return np.concatenate(rows), np.concatenate(cols), np.concatenate(whose)

    def _route_net(self, net, cost):
        """[path]: the net's islands joined one by one to the growing tree; [island] failed."""
        from scipy.sparse.csgraph import dijkstra
        islands = self.m.islands[net]
        self.g.data = cost[self.g.indices] + self.extra
        self.g.data[(self.step_net >= 0) & (self.step_net != self.m.net_id[net])] = FORBID
        tree = set(islands[0][0])
        left = list(range(1, len(islands)))
        paths, failed = [], []
        while left:
            if not tree:
                return paths, left
            dist, pred, _ = dijkstra(self.g, indices=sorted(tree), min_only=True, return_predecessors=True)
            best = min(((dist[list(islands[k][0])].min() if islands[k][0] else np.inf, k) for k in left))
            if not best[0] < FORBID / 2:
                return paths, failed + left
            k = best[1]
            nodes = list(islands[k][0])
            node = nodes[int(np.argmin(dist[nodes]))]
            path = [node]
            while path[-1] not in tree:
                path.append(int(pred[path[-1]]))
            paths.append(path[::-1])
            tree.update(path)
            tree.update(islands[k][0])
            left.remove(k)
        return paths, failed

    def run(self, iterations, pres=0.5, pres_growth=1.5, hist_step=0.4):
        m = self.m
        nets = sorted(m.islands, key=lambda n: -self._span(n))
        occ = np.zeros(self.n_res)
        hist = np.zeros(self.n_res)
        routes, failed = {}, {}
        todo = list(nets)
        over = 0
        best = None              # (islands unreached, cells shared, iteration, routes, failed): the round kept
        for it in range(1, iterations + 1):
            t = time.time()
            for net in todo:
                if net in routes:
                    occ[self._used(routes[net])] -= 1
                cost = ((1 + hist) * (1 + pres * occ))[self.res]
                cost[self.foreign & (m.owner != m.net_id[net])] = FORBID
                routes[net], failed[net] = self._route_net(net, cost)
                occ[self._used(routes[net])] += 1
            over = int((occ > 1).sum())
            if self.log:
                self.log(f"iteration {it}: {len(todo)} nets routed, {over} cells shared, "
                         f"{sum(len(f) for f in failed.values())} islands unreached, {time.time() - t:.0f} s")
            score = (sum(len(f) for f in failed.values()), over)
            if best is None or score < best[:2]:
                best = (*score, it, dict(routes), dict(failed))
            if over == 0 and not any(failed.values()):
                break
            hist[occ > 1] += hist_step * (occ[occ > 1] - 1)
            pres = min(pres * pres_growth, PRES_MAX)
            shared = occ > 1
            todo = [n for n in nets if failed[n] or shared[self._used(routes[n])].any()]
            if it % FULL_EVERY == 0:      # the nets in a shared cell's way move too, now and then
                todo = list(nets)
        _, over, it, routes, failed = best
        if self.log:
            self.log(f"kept iteration {it}: {over} cells shared")
        return Result(routes, [(n, k) for n in nets for k in failed[n]], over, it)

    def shared(self, result):
        """[(x, y, layer or "hole", [net])]: the cells more than one net still uses."""
        users = {}
        for net, paths in result.paths.items():
            for r in self._used(paths):
                users.setdefault(int(r), []).append(net)
        out = []
        n = len(self.m.owner)
        for r, nets in sorted(users.items()):
            if len(nets) > 1:
                if r < n:
                    x, y = self.m.xy(r)
                    out.append((x, y, self.m.layers[self.m.layer_of(r)], sorted(nets)))
                else:
                    x, y = self.m.board["free"][r - n]
                    out.append((x, y, "hole", sorted(nets)))
        return out

    def _used(self, paths):
        if not paths:
            return np.zeros(0, dtype=np.int64)
        return np.unique(self.res[np.concatenate([np.array(p, dtype=np.int64) for p in paths])])

    def _span(self, net):
        pts = [(it[1], it[2]) for _, items in self.m.islands[net] for it in items]
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        return max(xs) - min(xs) + max(ys) - min(ys)


def export(board_path, out):
    """The board's copper and the 1991 holes no copper takes, in mm, as JSON."""
    import pcbnew
    sys.path.insert(0, HERE)
    import pcb
    board = pcbnew.LoadBoard(board_path)
    mm = pcbnew.ToMM
    pads, vias, tracks = [], [], []
    for p in board.GetPads():
        s = p.GetSize(pcbnew.F_Cu)
        square = p.GetShape(pcbnew.F_Cu) not in (pcbnew.PAD_SHAPE_CIRCLE, pcbnew.PAD_SHAPE_OVAL)
        r = pad_radius(square, mm(s.x), mm(s.y), mm(p.GetDrillSize().x))
        inside = min(mm(s.x), mm(s.y)) / 2
        pads.append([mm(p.GetPosition().x), mm(p.GetPosition().y), r, p.GetNetname(),
                     f"{p.GetParentFootprint().GetReference()}.{p.GetNumber()}", inside])
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            vias.append([mm(t.GetPosition().x), mm(t.GetPosition().y), mm(t.GetWidth(pcbnew.F_Cu)) / 2, t.GetNetname()])
        else:
            tracks.append([board.GetLayerName(t.GetLayer()), mm(t.GetStart().x), mm(t.GetStart().y),
                           mm(t.GetEnd().x), mm(t.GetEnd().y), mm(t.GetWidth()), t.GetNetname()])
    holes = free_holes(pcb)
    taken = [(p[0], p[1]) for p in pads] + [(v[0], v[1]) for v in vias]
    free = [h for h in holes if min(math.dist(h, t) for t in taken) > 0.3]
    if HOLES == "ext":       # a via in one must stay clear of every pad, via and other hole's via
        rings = [(p[0], p[1], p[2]) for p in pads] + [(v[0], v[1], v[2]) for v in vias]
        kept = []
        for h in free:
            if all(math.dist(h, (x, y)) > r + FREE_R + CLEAR for x, y, r in rings) and \
                    all(math.dist(h, k) > 2 * FREE_R + CLEAR for k in kept):
                kept.append(h)
        free = kept
    box = board.GetBoardEdgesBoundingBox()
    box = [mm(box.GetLeft()), mm(box.GetTop()), mm(box.GetRight()), mm(box.GetBottom())]
    if HOLES == "ext":
        free += lattice(pads, vias, tracks, free, box)
    data = {"pads": pads, "vias": vias, "tracks": tracks, "free": free, "box": box}
    if HOLES == "ext":
        import pcb_ext
        data["cuts"] = [list(c) for c in pcb_ext.CUTS.values()]
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False)
    print(f"{len(pads)} pads, {len(vias)} vias, {len(tracks)} tracks, {len(free)} holes no copper takes -> {out}")


def lattice(pads, vias, tracks, free, box):
    """[(x, y)]: holes a via may be drilled in besides the original's - the centres
    between four pins of the pads' 2.54 mm grid, 2 mm inside the board, as clear of every
    pad (0.6 mm), via and hole (1.3 mm) and track (0.6 mm) as a via between two DIP rows
    stands.  The original's holes alone leave the module's board too few to route in."""
    xs, ys = [p[0] for p in pads], [p[1] for p in pads]

    def phase(vs):                       # the pads' grid's offset, mod 2.54, as their mean angle
        a = sum(math.cos(2 * math.pi * v / 2.54) for v in vs), sum(math.sin(2 * math.pi * v / 2.54) for v in vs)
        return math.atan2(a[1], a[0]) / (2 * math.pi) * 2.54 + 1.27
    ox, oy = phase(xs), phase(ys)
    x0, y0, x1, y1 = box
    taken = [(p[0], p[1], p[2] + 0.6) for p in pads] + [(v[0], v[1], FREE_R + 0.95) for v in vias]
    taken += [(x, y, FREE_R + 0.95) for x, y in free]
    out = []
    for i in range(math.floor((x0 - ox) / 2.54), math.ceil((x1 - ox) / 2.54) + 1):
        for j in range(math.floor((y0 - oy) / 2.54), math.ceil((y1 - oy) / 2.54) + 1):
            x, y = ox + i * 2.54, oy + j * 2.54
            if x0 + 2 < x < x1 - 2 and y0 + 2 < y < y1 - 2 and \
                    all(math.dist((x, y), (tx, ty)) > r for tx, ty, r in taken) and \
                    all(seg_dist(x, y, *t[1:5]) >= t[5] / 2 + 0.6 for t in tracks):
                out.append((x, y))
    return out


def free_holes(pcb):
    """The board's holes in mm: the 1991 board's (netlist/copper.json), or the module's
    (netlist/ext/holes.csv)."""
    if HOLES == "ext":
        import netlist
        return [(float(r["x"]), float(r["y"])) for r in netlist.rows(os.path.join(ROOT, "netlist", "ext", "holes.csv"))]
    with open(os.path.join(ROOT, "netlist", "copper.json"), encoding="utf-8") as f:
        return [(x * pcb.MM_PER_PX, y * pcb.MM_PER_PX) for x, y in json.load(f)["pads"]]


def via_size():
    """A free hole made a via: the 1991 board's (pcb.VIA_1991) or the module's (pcb_ext.SIGNAL)."""
    sys.path.insert(0, HERE)
    if HOLES == "ext":
        import pcb_ext
        return {"via": pcb_ext.SIGNAL["via"], "drill": pcb_ext.SIGNAL["drill"]}
    import pcb
    return pcb.VIA_1991


def _nearest_on(items, layer, x, y):
    """The point of the net's copper on layer (a hole: any layer) nearest (x, y)."""
    best = None
    for lay, x0, y0, x1, y1, _ in items:
        if lay is not None and lay != layer:
            continue
        dx, dy = x1 - x0, y1 - y0
        k = min(1.0, max(0.0, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy or 1.0)))
        p = (x0 + k * dx, y0 + k * dy)
        if best is None or math.dist(p, (x, y)) < math.dist(best, (x, y)):
            best = p
    return best


def geometry(model, result):
    """(tracks, vias): [layer, x0, y0, x1, y1, net] and the free holes used, [x, y, net]."""
    m = model
    tracks, vias = [], {}

    def stub(layer, node, net):
        x, y = m.xy(node)
        items = m.items[net]
        p = _nearest_on(items, m.layers[layer], x, y)
        if p is not None and math.dist(p, (x, y)) > 0.01:
            tracks.append([m.layers[layer], x, y, p[0], p[1], net])
    for net, paths in result.paths.items():
        ends = set().union(*(nodes for nodes, _ in m.islands[net]))
        for path in paths:
            for end in (path[0], path[-1]):
                if end in ends:
                    stub(m.layer_of(end), end, net)
            run = [path[0]]
            for a, b in zip(path, path[1:]):
                if m.layer_of(a) != m.layer_of(b):
                    hx, hy, free = m.via_hole[a % m.per_layer]
                    x, y = m.xy(a)
                    for node in (a, b):
                        if math.dist((hx, hy), (x, y)) > 0.01:
                            tracks.append([m.layers[m.layer_of(node)], x, y, hx, hy, net])
                    if free is not None:
                        vias[free] = [hx, hy, net]
                    tracks += _straight(m, run, net)
                    run = [b]
                else:
                    run.append(b)
            tracks += _straight(m, run, net)
    return tracks, list(vias.values())


def _straight(m, run, net):
    """A run of cells on one layer as its straight pieces."""
    out = []
    if len(run) < 2:
        return out
    pts = [m.xy(n) for n in run]
    start = pts[0]
    for k in range(1, len(pts) - 1):
        a, b, c = pts[k - 1], pts[k], pts[k + 1]
        if (b[0] - a[0]) * (c[1] - b[1]) != (b[1] - a[1]) * (c[0] - b[0]):
            out.append([m.layers[m.layer_of(run[0])], start[0], start[1], b[0], b[1], net])
            start = b
    out.append([m.layers[m.layer_of(run[0])], start[0], start[1], pts[-1][0], pts[-1][1], net])
    return out


def _seg_seg(ax, ay, bx, by, w):
    """Distances of the segment (ax, ay)-(bx, by) to the segments w[:, 0:4] (x0, y0, x1, y1)."""
    x0, y0, x1, y1 = w[:, 0], w[:, 1], w[:, 2], w[:, 3]

    def to_seg(px, py, sx0, sy0, sx1, sy1):
        dx, dy = sx1 - sx0, sy1 - sy0
        n = dx * dx + dy * dy
        k = np.clip(((px - sx0) * dx + (py - sy0) * dy) / np.where(n > 0, n, 1.0), 0.0, 1.0)
        return np.hypot(sx0 + k * dx - px, sy0 + k * dy - py)

    def side(px, py, qx, qy, rx, ry):
        return np.sign((qx - px) * (ry - py) - (qy - py) * (rx - px))
    d = np.minimum.reduce([to_seg(ax, ay, x0, y0, x1, y1), to_seg(bx, by, x0, y0, x1, y1),
                           to_seg(x0, y0, ax, ay, bx, by), to_seg(x1, y1, ax, ay, bx, by)])
    cross = (side(ax, ay, bx, by, x0, y0) * side(ax, ay, bx, by, x1, y1) < 0) & \
            (side(x0, y0, x1, y1, ax, ay) * side(x0, y0, x1, y1, bx, by) < 0)
    return np.where(cross, 0.0, d)


class _Copper:
    """Every piece of copper as rows (layer or -1 for a hole, x0, y0, x1, y1, how far a
    track's centre keeps from its centre line, net or -1): the board's, and the routes'
    pieces, which smooth() swaps for straight ones as it goes."""

    def __init__(self, board, tracks, vias, layers):
        self.layers, ids = layers, {}
        rows = []
        for x, y, r, net, *_ in board["pads"]:
            rows.append((-1, x, y, x, y, r + (CLEAR if net else HOLE_CLEAR), self._id(ids, net)))
        for x, y, r, net in board["vias"]:
            rows.append((-1, x, y, x, y, r + CLEAR, self._id(ids, net)))
        for layer, x0, y0, x1, y1, w, net in board["tracks"]:
            if layer in layers:
                rows.append((layers.index(layer), x0, y0, x1, y1, w / 2 + CLEAR, self._id(ids, net)))
        for cx0, cy0, cx1, cy1 in board.get("cuts", []):
            for a, b in (((cx0, cy0), (cx1, cy0)), ((cx1, cy0), (cx1, cy1)), ((cx1, cy1), (cx0, cy1)),
                         ((cx0, cy1), (cx0, cy0))):
                rows.append((-1, *a, *b, EDGE, -1))
        for x, y, net in vias:
            rows.append((-1, x, y, x, y, FREE_R + CLEAR, self._id(ids, net)))
        self.fixed = len(rows)
        for layer, x0, y0, x1, y1, net in tracks:
            rows.append((layers.index(layer), x0, y0, x1, y1, WIDTH / 2 + CLEAR, self._id(ids, net)))
        self.ids, self.rows = ids, np.array(rows, dtype=float).reshape(-1, 7)
        self.alive = np.ones(len(self.rows), bool)

    @staticmethod
    def _id(ids, net):
        return ids.setdefault(net, len(ids)) if net else -1

    def clear(self, layer, net, ax, ay, bx, by):
        """Whether a track of net from (ax, ay) to (bx, by) on layer keeps clear of other nets."""
        w, gap = self.rows, WIDTH / 2
        far = w[:, 5] + gap
        near = self.alive & ((w[:, 0] == layer) | (w[:, 0] == -1)) & (w[:, 6] != self.ids.get(net, -2)) & \
            (np.minimum(w[:, 1], w[:, 3]) - far < max(ax, bx)) & (np.maximum(w[:, 1], w[:, 3]) + far > min(ax, bx)) & \
            (np.minimum(w[:, 2], w[:, 4]) - far < max(ay, by)) & (np.maximum(w[:, 2], w[:, 4]) + far > min(ay, by))
        if not near.any():
            return True
        return bool((_seg_seg(ax, ay, bx, by, w[near, 1:5]) >= far[near] - 1e-9).all())

    def swap(self, pieces, layer, net, points):
        """The routes' pieces (their indices into tracks) replaced by the line through points."""
        self.alive[[self.fixed + k for k in pieces]] = False
        new = [(layer, *a, *b, WIDTH / 2 + CLEAR, self.ids[net]) for a, b in zip(points, points[1:])]
        self.rows = np.vstack([self.rows, np.array(new, dtype=float)])
        self.alive = np.concatenate([self.alive, np.ones(len(new), bool)])


def _chains(tracks, vias=()):
    """[(layer, net, [point], [piece])]: the routes' pieces in runs between ends - a point
    three pieces of a layer meet, a dead end, a via (vias: [x, y, net]).
    A point both layers' runs pass is no end: they join at the via beside it."""
    key = lambda x, y: (round(x, 4), round(y, 4))                          # noqa: E731
    at = {}
    for k, (layer, x0, y0, x1, y1, net) in enumerate(tracks):
        for p in (key(x0, y0), key(x1, y1)):
            at.setdefault((layer, net, p), []).append(k)
    ends = {p for (_, _, p), ks in at.items() if len(ks) != 2}
    ends |= {key(*v[:2]) for v in vias}
    seen, out = set(), []
    for k0, (layer, x0, y0, x1, y1, net) in enumerate(tracks):
        if k0 in seen:
            continue
        run = [key(x0, y0), key(x1, y1)]
        pieces = [k0]
        seen.add(k0)
        out.append(_grow(tracks, at, ends, seen, layer, net, run, pieces, key))
    return out


def _own(board):
    """{net: [(layer or None for a hole, x0, y0, x1, y1, r)]}: the board's copper by net;
    a pad by the circle inside it (a square one's sides), so a run past its corner is not on it."""
    own = {}
    for x, y, r, net, *inner in board["pads"]:
        own.setdefault(net, []).append((None, x, y, x, y, inner[1] if len(inner) > 1 else r))
    for x, y, r, net in board["vias"]:
        own.setdefault(net, []).append((None, x, y, x, y, r))
    for layer, x0, y0, x1, y1, w, net in board["tracks"]:
        own.setdefault(net, []).append((layer, x0, y0, x1, y1, w / 2))
    return own


def _touched(items, layer, p):
    """The indices of items (_own's of one net) a track's point p on layer touches."""
    return {k for k, (lay, x0, y0, x1, y1, r) in enumerate(items)
            if lay in (None, layer) and seg_dist(*p, x0, y0, x1, y1) < r + WIDTH / 2}


def _touches(items, line, need):
    """Whether the track through the points line touches every item (by index) of need."""
    if not need:
        return True
    w = np.array([items[k][1:5] for k in need], dtype=float)
    reach = np.array([items[k][5] for k in need]) + WIDTH / 2
    d = np.min([_seg_seg(*a, *b, w) for a, b in zip(line, line[1:])], axis=0)
    return bool((d < reach).all())


def _straight_on(pts):
    """pts without the points in the middle of a straight line."""
    out = pts[:1]
    for b, c in zip(pts[1:], pts[2:]):
        a = out[-1]
        if abs((b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])) > 1e-9 or \
                (b[0] - a[0]) * (c[0] - b[0]) + (b[1] - a[1]) * (c[1] - b[1]) < 0:
            out.append(b)
    return out + pts[-1:] if len(pts) > 1 else out


def _grow(tracks, at, ends, seen, layer, net, run, pieces, key):
    """One run grown both ways from its first piece until each tip is an end."""
    for forward in (True, False):
        while True:
            tip = run[-1] if forward else run[0]
            if tip in ends:
                break
            nxt = [k for k in at[(layer, net, tip)] if k not in seen]
            if not nxt:
                break
            k = nxt[0]
            seen.add(k)
            pieces.append(k)
            t = tracks[k]
            a, b = key(t[1], t[2]), key(t[3], t[4])
            other = b if a == tip else a
            run.append(other) if forward else run.insert(0, other)
    return layer, net, run, pieces


def _split(tracks):
    """The tracks with each piece cut where another piece of its net and layer ends on it
    (a branch off its middle), so that every join is some pieces' common end."""
    by = {}
    for t in {tuple(t) for t in tracks}:                    # a piece laid twice counts once
        by.setdefault((t[0], t[5]), []).append(t)
    out = []
    for (layer, net), pieces in by.items():
        ends = {(x, y) for t in pieces for x, y in (t[1:3], t[3:5])}
        for _, x0, y0, x1, y1, _ in pieces:
            length = math.hypot(x1 - x0, y1 - y0)
            cuts = sorted(((x - x0) * (x1 - x0) + (y - y0) * (y1 - y0)) / length ** 2
                          for x, y in ends if length and seg_dist(x, y, x0, y0, x1, y1) < 1e-4)
            cuts = [0.0] + [k for k in cuts if 1e-9 < k < 1 - 1e-9] + [1.0]
            pts = [(x0 + k * (x1 - x0), y0 + k * (y1 - y0)) for k in cuts]
            out += [[layer, *a, *b, net] for a, b in zip(pts, pts[1:]) if a != b]
    return out


def _to_centres(board, tracks):
    """The tracks, each dead end that lies on a pad or via of its net carried on to its
    centre: a run ends in the hole, not on the copper beside it."""
    count = {}
    for layer, x0, y0, x1, y1, net in tracks:
        for p in ((x0, y0), (x1, y1)):
            k = (layer, net, round(p[0], 4), round(p[1], 4))
            count[k] = count.get(k, 0) + 1
    holes = {}
    for x, y, r, net, *inner in board["pads"]:
        holes.setdefault(net, []).append((x, y, inner[1] if len(inner) > 1 else r))
    for x, y, r, net in board["vias"]:
        holes.setdefault(net, []).append((x, y, r))
    out = [list(t) for t in tracks]
    for (layer, net, x, y), n in count.items():
        near = [(math.dist((x, y), (hx, hy)), hx, hy) for hx, hy, r in holes.get(net, ())
                if math.dist((x, y), (hx, hy)) < r + WIDTH / 2]
        if n == 1 and near and min(near)[0] > 1e-4:
            _, hx, hy = min(near)
            out.append([layer, x, y, hx, hy, net])
    return out


def smooth(board, tracks, vias=()):
    """The routed tracks ([layer, x0, y0, x1, y1, net]; their vias [x, y, net]) with every
    run between two ends pulled straight wherever it keeps clear of the other nets' copper:
    the grid's staircases as runs at 0, 45 and 90 degrees, a corner never sharper than a
    right angle, a run's end in a hole's centre.  The ends stay, and a run still touches
    every pad, via and track of its net it touched on the way, so the joins do."""
    tracks = _split(_to_centres(board, tracks))
    layers = sorted({t[0] for t in tracks})
    copper = _Copper(board, tracks, vias, layers)
    own = _own(board)
    out = []
    holes = {(round(x, 4), round(y, 4)) for x, y, *_ in board["pads"] + board["vias"] + list(vias)}
    for layer, net, run, pieces in _chains(tracks, vias):
        if run[0] in holes and run[-1] not in holes:
            run = run[::-1]                     # the off-grid end last, where _slide sets a corner by it
        k, items = layers.index(layer), own.get(net, [])
        touched = [_touched(items, layer, p) for p in run]

        def fits(start, line, far):
            """Whether line, standing for run[start..far], touches all that did."""
            return _touches(items, line, set().union(*touched[start + 1:far]))
        pts, src, i = [run[0]], [0], 0
        while i < len(run) - 1:
            j, best = i, None
            for far in range(i + 1, len(run)):
                way = _octo_way(copper, k, net, pts, run[far], run[far] in holes)
                if way is not None and not fits(i, pts[-1:] + way, far):
                    way = None
                if way is None and far > i + 1:
                    break
                if way is not None and i + 1 < far < len(run) - 1 and \
                        _octo_way(copper, k, net, (pts[-1:] + way)[-2:], run[far + 1]) is None:
                    continue                    # a corner the run could not go on from
                j, best = far, way
            scrap = best is not None and (len(best) == 1 or math.dist(best[0], run[j]) < 2 * TAIL)
            slid = _slide(copper, k, net, pts, run[j]) if scrap and len(pts) > 1 else None
            if slid is not None and fits(src[-2], [pts[-2], slid, run[j]], j):
                pts[-1], best = slid, [run[j]]
            pts += best or [run[j]]
            src += [j] * len(best or [run[j]])
            i = j
        pts = _straight_on(pts)
        if pts != run:
            copper.swap(pieces, k, net, pts)
        out += [[layer, *a, *b, net] for a, b in zip(pts, pts[1:]) if a != b]
    once = {}                                   # a loop pulled tight lays a piece twice
    for t in out:
        once.setdefault((t[0], t[5], frozenset([tuple(t[1:3]), tuple(t[3:5])])), t)
    return list(once.values())


def _octo(a, b):
    """Whether the run a-b goes at 0, 45 or 90 degrees."""
    dx, dy = abs(b[0] - a[0]), abs(b[1] - a[1])
    return min(dx, dy, abs(dx - dy)) < 1e-6


def _slide(copper, layer, net, pts, end):
    """pts[-1] moved the least along the run into it (from pts[-2]) to where a run on to
    end goes at 0, 45 or 90 degrees, turning no sharper than a right angle: the corner
    before an off-grid end set by it, no slant or scrap left onto it; None for none."""
    (ax, ay), (px, py), (ex, ey) = pts[-2], pts[-1], end
    ux, uy = px - ax, py - ay
    if _octo(pts[-1], end) or not (ux or uy):
        return None
    best = None
    for vx, vy in ((1, 0), (0, 1), (1, 1), (1, -1)):
        det = vx * uy - vy * ux
        if abs(det) < 1e-12:
            continue
        s = (vx * (ey - ay) - vy * (ex - ax)) / det          # A + s u on the line through end along v
        q = (ax + s * ux, ay + s * uy)
        slide = math.dist(q, (px, py))
        if s <= 0 or math.dist(q, end) < 1e-6 or \
                ux * (ex - q[0]) + uy * (ey - q[1]) < -1e-9:
            continue
        if (best is None or slide < best[0]) and copper.clear(layer, net, ax, ay, *q) and \
                copper.clear(layer, net, *q, ex, ey):
            best = (slide, q)
    return best and best[1]


def _octo_way(copper, layer, net, pts, end, hole=False):
    """The points after pts[-1] of a clear way to end at 0, 45 and 90 degrees - one
    straight or diagonal run, or a diagonal and a straight one either way round -
    turning no sharper than a right angle; None when every way is blocked.  No scrap
    of a run onto an off-grid end but in a hole's copper (hole: end is a hole's centre),
    a slant to it otherwise."""
    (ax, ay), (bx, by) = pts[-1], end
    dx, dy = bx - ax, by - ay
    d = min(abs(dx), abs(dy))
    sx, sy = math.copysign(1, dx), math.copysign(1, dy)
    diagonal, straight = [(ax + sx * d, ay + sy * d), end], [(bx - sx * d, by - sy * d), end]
    if d < TAIL:
        ways = [straight, [end]] if hole and d > 1e-9 else [[end]]
    elif abs(abs(dx) - abs(dy)) < TAIL:
        ways = [diagonal, [end]] if hole and abs(abs(dx) - abs(dy)) > 1e-9 else [[end]]
    else:
        ways = [diagonal, straight]
    for way in ways:
        line = pts[-2:] + way
        if all((b[0] - a[0]) * (c[0] - b[0]) + (b[1] - a[1]) * (c[1] - b[1]) >= -1e-9
               for a, b, c in zip(line, line[1:], line[2:])) and \
                all(copper.clear(layer, net, *a, *b) for a, b in zip([pts[-1]] + way, way)):
            return way
    return None


def route(iterations, layers):
    with open(os.path.join(WORK, "board.json"), encoding="utf-8") as f:
        board = json.load(f)
    t = time.time()
    model = Model(board, layers)
    joins = sum(len(v) - 1 for v in model.islands.values())
    print(f"grid {model.nx} x {model.ny} x {len(layers)} ({', '.join(layers)}); "
          f"{len(model.islands)} nets, {joins} joins to make; {time.time() - t:.0f} s", flush=True)
    finder = PathFinder(model, lambda s: print(s, flush=True))
    result = finder.run(iterations, **PRICES)
    tracks, vias = geometry(model, result)
    shared = finder.shared(result)
    for x, y, layer, nets in shared:
        print(f"  shared at ({x:.2f}, {y:.2f}) {layer}: {', '.join(nets)}")
    out = os.path.join(WORK, "routes.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"layers": layers, "tracks": tracks, "vias": vias, "failed": result.failed, "shared_cells": shared,
                   "shared": result.overused, "iterations": result.iterations}, f, ensure_ascii=False)
    print(f"{len(tracks)} track pieces, {len(vias)} free holes made vias, {len(result.failed)} islands unreached, "
          f"{result.overused} cells shared -> {out}")
    return 0


def smooth_routes():
    """routes.json's tracks pulled straight (smooth()), in place."""
    with open(os.path.join(WORK, "board.json"), encoding="utf-8") as f:
        board = json.load(f)
    path = os.path.join(WORK, "routes.json")
    with open(path, encoding="utf-8") as f:
        routes = json.load(f)
    before = len(routes["tracks"])
    routes["tracks"] = smooth(board, routes["tracks"], routes["vias"])
    with open(path, "w", encoding="utf-8") as f:
        json.dump(routes, f, ensure_ascii=False)
    print(f"{before} track pieces -> {len(routes['tracks'])} -> {path}")
    return 0


def apply(out, board_path):
    """The routes as tracks (WIDTH) and vias (via_size()) on a copy of the board."""
    import pcbnew
    size = via_size()
    with open(os.path.join(WORK, "routes.json"), encoding="utf-8") as f:
        routes = json.load(f)
    board = pcbnew.LoadBoard(board_path)

    def at(x, y):
        return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))
    for layer, x0, y0, x1, y1, net in routes["tracks"]:
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(at(x0, y0))
        t.SetEnd(at(x1, y1))
        t.SetWidth(pcbnew.FromMM(WIDTH))
        t.SetLayer(board.GetLayerID(layer))
        t.SetNet(board.FindNet(net))
        board.Add(t)
    for x, y, net in routes["vias"]:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(at(x, y))
        v.SetWidth(pcbnew.FromMM(size["via"]))
        v.SetDrill(pcbnew.FromMM(size["drill"]))
        v.SetNet(board.FindNet(net))
        board.Add(v)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(out)
    print(f"{len(routes['tracks'])} track pieces, {len(routes['vias'])} vias -> {out}")
    return 0


def planes(routed):
    """The routed board read back (tools/readback.py): the plane pins cut off the planes
    go into board.json, so that `route` again joins them by a run."""
    sys.path.insert(0, HERE)
    import readback
    groups = readback.pin_groups(*readback.read_board(routed))
    splits = readback.plane_splits(groups, readback.wanted("ext" if HOLES == "ext" else "ns4"))
    path = os.path.join(WORK, "board.json")
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    data["plane_splits"] = splits
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False)
    told = "; ".join(f"{net}: {', '.join(' '.join(g) for g in gs)}" for net, gs in splits.items())
    print(f"plane pins cut off: {told or 'none'} -> {path}")
    return 0


def where(argv):
    """(board, work folder, the rest of argv); `ext` first: the module's board, its holes
    and its two outer layers."""
    global HOLES, LAYERS, PITCH, FREE_R, REACH, ROUNDS, PRICES
    if len(argv) > 1 and argv[1] == "ext":
        HOLES, LAYERS = "ext", ["F.Cu", "B.Cu"]
        FREE_R = 0.35                                   # the module's via, 0.6 mm (pcb_ext.SIGNAL), off the grid
        REACH = 0.6                                     # inside a 0.6 mm via's cells: a stub stays on its own
        PITCH = 0.635                                   # a quarter of the pins' 2.54: two tracks between pins
        # the module's whole board routed with nothing shared in 328 rounds (2026-11-18); the
        # factors of four tried the fewest vias, 289
        ROUNDS, PRICES = 400, {"pres": 1.0, "pres_growth": 1.25, "hist_step": 0.5}
        return os.path.join(ROOT, "kicad", "ext.kicad_pcb"), os.path.join(ROOT, "build", "pathfind-ext"), argv[2:]
    return os.path.join(ROOT, "kicad", "ns4.kicad_pcb"), os.path.join(ROOT, "build", "pathfind"), argv[1:]


def main(argv):
    global WORK
    board, WORK, rest = where(argv)
    argv = [argv[0]] + rest
    if len(argv) > 1 and argv[1] == "export":
        export(argv[2] if len(argv) > 2 else board, os.path.join(WORK, "board.json"))
        return 0
    if len(argv) > 1 and argv[1] == "route":
        return route(int(argv[2]) if len(argv) > 2 else ROUNDS, argv[3:] or LAYERS)
    if len(argv) > 1 and argv[1] == "smooth":
        return smooth_routes()
    if len(argv) > 2 and argv[1] == "apply":
        return apply(os.path.join(ROOT, argv[2]), argv[3] if len(argv) > 3 else board)
    if len(argv) > 1 and argv[1] == "planes":
        return planes(os.path.join(ROOT, argv[2]) if len(argv) > 2 else board)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
