"""Where the town ends: each street that leaves it is closed across its width (jersey barriers end to end, a road block
in front of them and a stop sign on each side, facing the town), past the spot the zombies climb out of. And the
barricades of the setpieces stood in a line across the street, not scattered along it.

    close_streets(scene, catalog, models_for, seed)  -> the closures added to the scene (scene.add)
    line_up(scene, catalog)                          -> a scene made before "line": its barricades' barriers set in a row

A street's entrance (net.entrances, kind "street") is where it meets the town's edge; the street's own edge gives its
direction (into the town) and its width.
"""
import math
import random

from shapely.geometry import LineString, Point
from shapely.strtree import STRtree

import parcels

OUT = 2.5          # meters past the entrance (out of the town) where the barriers stand
SIDEWALK = 2.0     # the closure spans the street and this much on each side
GAP = 0.05         # between two barriers
TOGETHER = 10.0    # exits closer than this are closed by one row


def length_of(model):
    """A barrier's length: its longer side on the floor."""
    size = model.get('size', [2.0, 0.5, 1.0])
    return max(size[0], size[1])


def _yaw_along(vx, vy):
    """The yaw that lays a model's own Z (its length, for most barriers) along the vector (vx, vy)."""
    return math.degrees(math.atan2(vy, vx)) + 90.0


def _facing(dx, dy):
    """The yaw that turns a model's front (its +Z) toward the direction (dx, dy)."""
    return math.degrees(math.atan2(dy, dx)) + 90.0


def _occupied(scene, catalog, skip=()):
    shapes = []
    for e in scene.data['entities']:
        if e.get('id') in skip or e['asset'] not in catalog['models']:
            continue
        m = catalog['models'][e['asset']]
        if 'ground' in m.get('roles', []) or e.get('role') in ('manhole', 'blood', 'letter', 'grass'):
            continue
        foot = parcels.footprint_poly(m, e['x'], e['y'], e['yaw'], e.get('scale', 1.0))
        if foot.area < 400.0:   # not the sky's cloud rings or the backdrop: nothing on the floor is that big
            shapes.append(foot)
    tree = STRtree(shapes) if shapes else None

    def hits(poly):
        return bool(tree is not None and len(tree.query(poly, predicate='intersects')))
    return hits


def _street_end(network, x, y, center=(0.0, 0.0)):
    """The street through (x, y): its direction there, pointing into the town (toward 'center'), and its width; None
    when no street passes within 6 m. The streets' lines run on past the town's edge."""
    best = None
    for edge in network.get('edges', []):
        points = edge.get('points', [])
        if len(points) < 2 or edge.get('kind') == 'alley':
            continue
        line = LineString(points)
        d = line.distance(Point(x, y))
        if best is None or d < best[0]:
            best = (d, line, edge.get('width', 7.0))
    if best is None or best[0] > 6.0:
        return None
    _, line, width = best
    at = line.project(Point(x, y))
    p, q = line.interpolate(max(0.0, at - 1.0)), line.interpolate(min(line.length, at + 1.0))
    dx, dy = q.x - p.x, q.y - p.y
    n = math.hypot(dx, dy) or 1.0
    dx, dy = dx / n, dy / n
    if dx * (center[0] - x) + dy * (center[1] - y) < 0.0:
        dx, dy = -dx, -dy
    return (dx, dy), width


def close_streets(scene, catalog, models_for, seed=1):
    rng = random.Random(seed * 71 + 5)
    network = scene.data.get('network', {})
    jerseys, blocks, signs, cones = (models_for(catalog, r) for r in ('jersey', 'roadblock', 'stop_sign', 'cone'))
    if not jerseys:
        return []
    hits = _occupied(scene, catalog)
    added = []

    def put(asset, x, y, yaw, role):
        m = catalog['models'][asset]
        foot = parcels.footprint_poly(m, x, y, yaw)
        if hits(foot.buffer(0.05)):
            return
        added.append(scene.add(asset, round(x, 3), round(y, 3), round(yaw % 360.0, 2), 1.0, zone=None, role=role))

    bounds = scene.data.get('bounds', [-1.0, -1.0, 1.0, 1.0])
    middle = ((bounds[0] + bounds[2]) / 2.0, (bounds[1] + bounds[3]) / 2.0)
    # streets that leave together (exits a few meters apart: two streets meeting at the edge) get one row across both
    exits = [(ex, ey) for ex, ey, kind in network.get('entrances', []) if kind == 'street']
    groups = []
    for p in exits:
        for g in groups:
            if any(math.dist(p, q) < TOGETHER for q in g):
                g.append(p)
                break
        else:
            groups.append([p])
    for group in groups:
        ex, ey = group[0]
        found = _street_end(network, ex, ey, middle)
        if found is None:
            continue
        (dx, dy), width = found
        ax, ay = -dy, dx                       # across the street
        gx = sum(p[0] for p in group) / len(group)
        gy = sum(p[1] for p in group) / len(group)
        cx, cy = gx - dx * OUT, gy - dy * OUT  # the line of barriers
        spread = max(abs((p[0] - gx) * ax + (p[1] - gy) * ay) for p in group)
        half = width / 2.0 + SIDEWALK + spread
        # end to end on one straight line, all at the same angle (as a mission's barricades: a jitter of a degree or two and
        # the second model's other length read as a wall put up badly)
        step = max(length_of(catalog['models'][j]) for j in jerseys) + GAP
        count = max(1, math.ceil(2.0 * half / step))
        for i in range(count):
            k = (i - (count - 1) / 2.0) * step
            put(jerseys[i % len(jerseys)], cx + ax * k, cy + ay * k, _yaw_along(ax, ay), 'barrier')
        # in front of them, on the town's side: the road block and two cones; a stop sign at each curb
        if blocks:
            put(blocks[0], cx + dx * 1.3, cy + dy * 1.3, _facing(dx, dy), 'barrier')
        for side in (-1, 1):
            if cones:
                put(rng.choice(cones), cx + dx * 2.3 + ax * side * 2.2, cy + dy * 2.3 + ay * side * 2.2, rng.uniform(0, 360), 'cone')
            if signs:
                at = width / 2.0 + 0.7
                put(signs[0], cx + dx * 0.9 + ax * side * at, cy + dy * 0.9 + ay * side * at, _facing(dx, dy), 'sign')
    return added


def line_up(scene, catalog, spawner='setpiece:barricade', reach=9.0):
    """The barriers of each barricade (the setpiece's, near each other) set end to end across the street, on the line
    their own yaw gives (the setpiece turns them across), centered where they were. A spot taken by something else is
    left out. Returns how many were moved."""
    barriers = [e for e in scene.data['entities'] if e.get('spawner') == spawner and e.get('role') == 'barrier'
                and e['asset'] in catalog['models']]
    groups = []
    for e in barriers:
        for g in groups:
            if any(math.hypot(e['x'] - o['x'], e['y'] - o['y']) < reach for o in g):
                g.append(e)
                break
        else:
            groups.append([e])
    moved = 0
    for group in groups:
        near = [e for e in scene.data['entities'] if e.get('spawner') == spawner and e.get('role') == 'cone'
                and any(math.hypot(e['x'] - o['x'], e['y'] - o['y']) < reach for o in group)]
        hits = _occupied(scene, catalog, skip={e['id'] for e in group} | {e['id'] for e in near})
        placed = []
        sx = sum(math.cos(math.radians(e['yaw'])) for e in group)
        sy = sum(math.sin(math.radians(e['yaw'])) for e in group)
        yaw = math.degrees(math.atan2(sy, sx))
        # their length along (model Z): the yaw turns (0, -1)
        a = math.radians(yaw)
        ax, ay = math.sin(a), -math.cos(a)
        cx = sum(e['x'] for e in group) / len(group)
        cy = sum(e['y'] for e in group) / len(group)
        step = max(length_of(catalog['models'][e['asset']]) for e in group) + GAP
        keep = []
        for i, e in enumerate(group):
            k = (i - (len(group) - 1) / 2.0) * step
            x, y = cx + ax * k, cy + ay * k
            turn = yaw + (e['yaw'] - yaw + 180.0) % 360.0 - 180.0
            turn = yaw + max(-3.0, min(3.0, turn - yaw))
            foot = parcels.footprint_poly(catalog['models'][e['asset']], x, y, turn)
            if hits(foot.buffer(0.05)):
                continue
            e['x'], e['y'], e['yaw'] = round(x, 3), round(y, 3), round(turn % 360.0, 2)
            placed.append(foot)
            keep.append(e['id'])
            moved += 1
        dropped = {e['id'] for e in group} - set(keep)
        for cone in near:
            m = catalog['models'].get(cone['asset'])
            if m and any(parcels.footprint_poly(m, cone['x'], cone['y'], cone['yaw']).intersects(f.buffer(0.1)) for f in placed):
                dropped.add(cone['id'])
        if dropped:
            scene.data['entities'] = [e for e in scene.data['entities'] if e.get('id') not in dropped]
    return moved
