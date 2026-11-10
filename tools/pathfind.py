#!/usr/bin/env python3
"""PathFinder on the 1991 board's hidden layers: Э3's nets joined inside, between the
holes the board already has.

    <KiCad>/bin/python tools/pathfind.py export [BOARD]     - build/pathfind/board.json
    python tools/pathfind.py route [ITERATIONS] [LAYER...]   - build/pathfind/routes.json
    <KiCad>/bin/python tools/pathfind.py apply OUT [BOARD]   - the routes on a copy of the board

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
FORBID = 1e18             # ...far below a cell of another net's
PLANES = ("GND", "+5V")
OPEN, SHUT = -1, -2
LAYERS = ["In1.Cu", "In3.Cu", "In4.Cu", "In6.Cu"]


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
        for x, y, r, net, _ in self.board["pads"]:
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

    def _holes(self):
        """The cells a route may change layer in, each with its hole: (x, y, free hole or None)."""
        hole = {}
        spots = [(x, y, r, None) for x, y, r, net, _ in self.board["pads"] if net]
        spots += [(x, y, r, None) for x, y, r, _ in self.board["vias"]]
        spots += [(x, y, FREE_R, k) for k, (x, y) in enumerate(self.board["free"])]
        for x, y, r, free in spots:
            for i, j in zip(*self._cells(x, y, x, y, max(r + WIDTH / 2, REACH))):
                hole.setdefault(j * self.nx + i, (x, y, free))
        return sorted(hole), hole

    def _items(self):
        """{net: [(layer or None, x0, y0, x1, y1, r)]}: the copper of each net."""
        items = {}
        for x, y, r, net, _ in self.board["pads"]:
            if net:
                items.setdefault(net, []).append((None, x, y, x, y, r))
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
            if over == 0 and not any(failed.values()):
                break
            hist[occ > 1] += hist_step * (occ[occ > 1] - 1)
            pres = min(pres * pres_growth, PRES_MAX)
            shared = occ > 1
            todo = [n for n in nets if failed[n] or shared[self._used(routes[n])].any()]
            if it % FULL_EVERY == 0:      # the nets in a shared cell's way move too, now and then
                todo = list(nets)
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
        pads.append([mm(p.GetPosition().x), mm(p.GetPosition().y), r, p.GetNetname(),
                     f"{p.GetParentFootprint().GetReference()}.{p.GetNumber()}"])
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            vias.append([mm(t.GetPosition().x), mm(t.GetPosition().y), mm(t.GetWidth(pcbnew.F_Cu)) / 2, t.GetNetname()])
        else:
            tracks.append([board.GetLayerName(t.GetLayer()), mm(t.GetStart().x), mm(t.GetStart().y),
                           mm(t.GetEnd().x), mm(t.GetEnd().y), mm(t.GetWidth()), t.GetNetname()])
    with open(os.path.join(ROOT, "netlist", "copper.json"), encoding="utf-8") as f:
        holes = [(x * pcb.MM_PER_PX, y * pcb.MM_PER_PX) for x, y in json.load(f)["pads"]]
    taken = [(p[0], p[1]) for p in pads] + [(v[0], v[1]) for v in vias]
    free = [h for h in holes if min(math.dist(h, t) for t in taken) > 0.3]
    box = board.GetBoardEdgesBoundingBox()
    data = {"pads": pads, "vias": vias, "tracks": tracks, "free": free,
            "box": [mm(box.GetLeft()), mm(box.GetTop()), mm(box.GetRight()), mm(box.GetBottom())]}
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False)
    print(f"{len(pads)} pads, {len(vias)} vias, {len(tracks)} tracks, {len(free)} holes no copper takes -> {out}")


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


def route(iterations, layers):
    with open(os.path.join(WORK, "board.json"), encoding="utf-8") as f:
        board = json.load(f)
    t = time.time()
    model = Model(board, layers)
    joins = sum(len(v) - 1 for v in model.islands.values())
    print(f"grid {model.nx} x {model.ny} x {len(layers)} ({', '.join(layers)}); "
          f"{len(model.islands)} nets, {joins} joins to make; {time.time() - t:.0f} s", flush=True)
    finder = PathFinder(model, lambda s: print(s, flush=True))
    result = finder.run(iterations)
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


def apply(out, board_path):
    """The routes as tracks (WIDTH) and vias (pcb.VIA_1991) on a copy of the board."""
    import pcbnew
    sys.path.insert(0, HERE)
    import pcb
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
        v.SetWidth(pcbnew.FromMM(pcb.VIA_1991["via"]))
        v.SetDrill(pcbnew.FromMM(pcb.VIA_1991["drill"]))
        v.SetNet(board.FindNet(net))
        board.Add(v)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(out)
    print(f"{len(routes['tracks'])} track pieces, {len(routes['vias'])} vias -> {out}")
    return 0


def main(argv):
    board = os.path.join(ROOT, "kicad", "ns4.kicad_pcb")
    if len(argv) > 1 and argv[1] == "export":
        export(argv[2] if len(argv) > 2 else board, os.path.join(WORK, "board.json"))
        return 0
    if len(argv) > 1 and argv[1] == "route":
        return route(int(argv[2]) if len(argv) > 2 else 100, argv[3:] or LAYERS)
    if len(argv) > 2 and argv[1] == "apply":
        return apply(os.path.join(ROOT, argv[2]), argv[3] if len(argv) > 3 else board)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
