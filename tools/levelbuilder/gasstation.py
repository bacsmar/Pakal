"""A plazuela comercial with a gas station (a strip mall as Mexico builds them): an L of shops along the block's two back
sides, every shop facing in, onto a forecourt open on the corner that faces the game's camera (two streets); in the
forecourt a canopy over the pump islands, a tall sign at the street corner; in the L's inner corner the convenience
store, turned to the forecourt.

    plan(block, camera, depth)                  -> {forecourt, front, backs, inner, ...}: where each part goes
    shops(plan, catalog, choices, rng)          -> the L's shops ([{asset, x, y, yaw, poly}], the store first)
    furnish(plan, catalog, models_for, rng)     -> entities (canopy, islands, pumps) and the sign's place

The canopy is walked under: only its posts collide (an entity's "posts": circles, export.py). The pumps are the theme's
breakables (they blow up). Roles: gas_canopy, gas_island, gas_pump (a theme names them).

In the brief:
    "gas_stations": {"blocks": ["block_05"], "camera": [-1, -1], "depth": 8.0, "store": "E-OZZO", "sign": "PEMEJ"}
"""
import json
import math
from pathlib import Path

import numpy as np
from shapely.geometry import LineString, MultiPoint, Point, Polygon
from shapely.affinity import affine_transform as shapely_affine
from shapely.ops import unary_union

import citykit_path  # noqa: F401
import citykit  # noqa: E402
import parcels  # noqa: E402
from geom import polygons  # noqa: E402


def plan(block, camera=(-1.0, -1.0), depth=8.0):
    """The block's corner that faces the camera most (its two sides' outward normals toward it): the forecourt is the
    block but a band 'depth' deep along the two other sides, where the shops go."""
    k = math.hypot(*camera) or 1.0
    cam = (camera[0] / k, camera[1] / k)
    rect = block.minimum_rotated_rectangle
    corners = list(rect.exterior.coords)[:4]
    if not Polygon(corners).exterior.is_ccw:
        corners.reverse()
    c = rect.centroid
    sides = []
    for i in range(4):
        a, b = corners[i], corners[(i + 1) % 4]
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        nx, ny = mx - c.x, my - c.y
        n = math.hypot(nx, ny) or 1.0
        sides.append({'a': a, 'b': b, 'n': (nx / n, ny / n), 'facing': (nx * cam[0] + ny * cam[1]) / n})
    # the corner between two sides that together face the camera most
    best = max(range(4), key=lambda i: sides[i]['facing'] + sides[(i + 1) % 4]['facing'])
    front = [sides[best], sides[(best + 1) % 4]]
    backs = [sides[(best + 2) % 4], sides[(best + 3) % 4]]
    bands = []
    for s in backs:
        a, b, n = np.array(s['a']), np.array(s['b']), np.array(s['n'])
        bands.append(Polygon([tuple(a + n), tuple(b + n), tuple(b - n * depth), tuple(a - n * depth)]))
    back_band = unary_union(bands)
    forecourt = block.difference(back_band).buffer(-0.3, join_style=2)
    main = max(front, key=lambda s: s['facing'])
    # the L's inner corner: where the two back sides meet (backs[0] ends where backs[1] starts), 'depth' in from both
    inner = backs[0]['b']
    return {'forecourt': forecourt, 'front': main, 'backs': backs, 'inner': inner, 'depth': depth, 'block': block,
            'corner': front[0]['b'], 'camera': cam}


def shops(p, catalog, choices, rng, store=None, gap=0.15):
    """The L: along each back side, shops shoulder to shoulder from the street corner in, each facing in (its back to
    the side); the square where the two sides meet is the store's ('store': its model; the choices' widest that fits
    when None), turned to the forecourt's middle. Returns [{asset, x, y, yaw, poly}], the store first."""
    depth = p['depth']
    fits = [(m, w) for m, w in choices if catalog['models'][m]['size'][1] <= depth + 0.5]
    out = []
    inside = p['block'].buffer(0.05)
    fc = p['forecourt'].centroid
    ix, iy = p['inner']
    # the store in the inner corner, its front toward the forecourt's middle
    # ('store': a model, or a list of them: the widest of those that fits)
    pool = ([store] if isinstance(store, str) else list(store)) if store else [m for m, _ in fits]
    candidates = sorted(pool, key=lambda m: -catalog['models'][m]['size'][0])
    for key in candidates:
        m = catalog['models'][key]
        if max(m['size'][0], m['size'][1]) > depth * 1.05:   # (in its square: wider, it takes the sides' first shops)
            continue
        nx, ny = fc.x - ix, fc.y - iy
        k = math.hypot(nx, ny) or 1.0
        yaw = parcels.yaw_facing(nx, ny)
        # as far into the corner as it fits inside the block
        for reach in np.arange(depth * 0.5, depth * 1.4, 0.25):
            x, y, foot = parcels.centered(m, ix + nx / k * reach, iy + ny / k * reach, yaw)
            if inside.contains(foot):
                out.append({'asset': key, 'x': round(x, 3), 'y': round(y, 3), 'yaw': round(yaw, 2), 'poly': foot})
                break
        if out:
            break
    for side in p['backs']:
        a, b, n = np.array(side['a']), np.array(side['b']), np.array(side['n'])
        length = float(np.linalg.norm(b - a))
        t = (b - a) / length
        # from the street end to the inner corner's square: backs[0] runs to it (a -> b), backs[1] from it (a -> b)
        toward_inner = np.allclose(side['b'], p['inner'])
        s, stop = 0.0, length - depth
        while s < stop - 3.0 and fits:
            key = rng.choices([m for m, _ in fits], [w for _, w in fits])[0]
            m = catalog['models'][key]
            width = m['size'][0]
            if s + width > stop:
                smaller = [(mm, ww) for mm, ww in fits if catalog['models'][mm]['size'][0] <= stop - s]
                if not smaller:
                    break
                key = max(smaller, key=lambda f: catalog['models'][f[0]]['size'][0])[0]
                m = catalog['models'][key]
                width = m['size'][0]
            along = s + width / 2 if toward_inner else length - s - width / 2
            c = a + t * along - n * (m['size'][1] / 2 + 0.3)
            yaw = parcels.yaw_facing(-n[0], -n[1])
            # inside the block: its sides are not straight (a curved street, a cut corner): stepped in until it fits
            for push in np.arange(0.0, 2.75, 0.25):
                x, y, foot = parcels.centered(m, float(c[0] - n[0] * push), float(c[1] - n[1] * push), yaw)
                if inside.contains(foot):
                    break
            if inside.contains(foot) and not any(foot.intersects(o['poly'].buffer(-0.05)) for o in out):
                out.append({'asset': key, 'x': round(x, 3), 'y': round(y, 3), 'yaw': round(yaw, 2), 'poly': foot})
            s += width + gap
    return out


def posts_of(path, below=2.5, above=0.3):
    """The posts of a canopy model (its parts between 'above' and 'below' meters up): [(x, z, radius)] in its frame."""
    path = Path(path)
    doc = json.loads(path.read_text())
    blobs, points = {}, []

    def accessor(i):
        a = doc['accessors'][i]
        view = doc['bufferViews'][a['bufferView']]
        if view['buffer'] not in blobs:
            blobs[view['buffer']] = (path.parent / doc['buffers'][view['buffer']]['uri']).read_bytes()
        start = view.get('byteOffset', 0) + a.get('byteOffset', 0)
        return np.frombuffer(blobs[view['buffer']], np.float32, a['count'] * 3, start).reshape(-1, 3)

    def visit(index, parent):
        node = doc['nodes'][index]
        world = parent @ citykit._local_matrix(node)
        if 'mesh' in node:
            for prim in doc['meshes'][node['mesh']]['primitives']:
                p = accessor(prim['attributes']['POSITION'])
                points.append((np.c_[p, np.ones(len(p))] @ world.T)[:, :3])
        for child in node.get('children', []):
            visit(child, world)

    for root in doc['scenes'][doc.get('scene', 0)]['nodes']:
        visit(root, np.eye(4))
    v = np.concatenate(points)
    low = v[(v[:, 1] > above) & (v[:, 1] < below)]
    if not len(low):
        return []
    # one post of the parts near each other (a post is several pieces: its column, its trims)
    found = unary_union(MultiPoint([(x, z) for x, _, z in low]).buffer(0.4))
    out = []
    for g in polygons(found):
        x0, z0, x1, z1 = g.bounds
        out.append((round((x0 + x1) / 2, 3), round((z0 + z1) / 2, 3), round(max(x1 - x0, z1 - z0) / 2 - 0.35, 3)))
    return out


def _local(x, y, yaw, dx, dz):
    """A point of a model's frame (its x, its z) placed at (x, y) turned 'yaw' (degrees; its front, +z, at yaw 0 faces
    south): the level's (x, y)."""
    a = math.radians(yaw)
    # its +x goes to (cos, sin), its +z (its front) to (sin, -cos): parcels.yaw_facing's turn
    return (x + dx * math.cos(a) + dz * math.sin(a), y + dx * math.sin(a) - dz * math.cos(a))


def furnish(p, catalog, models_for, rng, root=None):
    """The canopy at the forecourt's middle, its long side to the main street; an island under each of its posts, along
    its depth, a pump on each side of the post. Returns (entities, sign) with sign = (x, y, facing) for the tall sign."""
    canopies, islands, pumps = models_for('gas_canopy'), models_for('gas_island'), models_for('gas_pump')
    out = []
    if not canopies:
        return out, None
    fc = p['forecourt']
    n = p['front']['n']
    yaw = parcels.yaw_facing(*n)
    cx, cy = fc.centroid.x, fc.centroid.y
    canopy = canopies[0]
    model = catalog['models'][canopy]
    posts = posts_of(Path(root) / model['path']) if root else []
    out.append({'asset': canopy, 'x': round(cx, 3), 'y': round(cy, 3), 'yaw': round(yaw, 2), 'role': 'gas_canopy',
                'posts': [[*map(lambda v: round(v, 3), _local(cx, cy, yaw, px, pz)), r] for px, pz, r in posts]})
    for px, pz, _ in posts or [(-3.65, 0.0, 0.3), (3.65, 0.0, 0.3)]:
        ix, iy = _local(cx, cy, yaw, px, pz)
        if islands:
            out.append({'asset': islands[0], 'x': round(ix, 3), 'y': round(iy, 3), 'yaw': round(yaw + 90.0, 2), 'role': 'gas_island'})
        for side in (-1, 1):
            if not pumps:
                break
            # along the island (its x, the canopy's depth), between its end post and the canopy's post
            qx, qy = _local(ix, iy, yaw + 90.0, side * 1.45, 0.0)
            out.append({'asset': rng.choice(pumps), 'x': round(qx, 3), 'y': round(qy, 3), 'z': 0.12,
                        'yaw': round(yaw + 90.0 + (0.0 if side > 0 else 180.0), 2), 'role': 'gas_pump'})
    # the tall sign at the forecourt's outer corner, a step in from the curb, facing the camera
    corner = Point(p['corner'])
    near = fc.boundary.interpolate(fc.boundary.project(corner))
    inward = (fc.centroid.x - near.x, fc.centroid.y - near.y)
    k = math.hypot(*inward) or 1.0
    sx, sy = near.x + inward[0] / k * 1.2, near.y + inward[1] / k * 1.2
    return out, (sx, sy, p['camera'])



def parking_stripes(p, avoid, spacing=2.7, length=4.2, width=0.16, least=3):
    """Parking bays painted in front of the L's shops: a stripe every 'spacing' meters along each shop band's inner edge,
    'length' long into the forecourt. Only the stripes wholly on the forecourt and clear of 'avoid' (the canopy, its
    islands and pumps), and only runs of 'least' or more side by side (two lone stripes read as stray lines). Returns
    their polygons."""
    out = []
    for s in p['backs']:
        a, b, n = np.array(s['a']), np.array(s['b']), np.array(s['n'])
        a, b = a - n * p['depth'], b - n * p['depth']
        along = float(np.hypot(*(b - a)))
        if along < spacing:
            continue
        t = (b - a) / along
        k = spacing
        run = []
        while k < along - spacing * 0.5:
            foot = a + t * k - n * 0.5
            stripe = LineString([tuple(foot), tuple(foot - n * length)]).buffer(width / 2, cap_style=2)
            # (the L's inner corner: one band's bays run into the other's)
            if p['forecourt'].contains(stripe) and (avoid is None or not stripe.intersects(avoid)) \
                    and not any(stripe.distance(o) < 1.5 for o in out):
                run.append(stripe)
            else:
                out += run if len(run) >= least else []
                run = []
            k += spacing
        out += run if len(run) >= least else []
    return out


def model_footprint(model, x, y, yaw, cell=1.0, band=(0.5, 4.0), scale=1.0):
    """A model's real footprint on the floor (an L is not its box): its vertices between 'band' heights (knee to head by
    default) in cells of 'cell' meters, joined, placed at (x, y) turned 'yaw', scaled 'scale'."""
    v = citykit.model_vertices(citykit.REPO / model['path']) * scale
    v = v[(v[:, 1] > band[0]) & (v[:, 1] < band[1])]
    if not len(v):
        return parcels.footprint_poly(model, x, y, yaw)
    cells = {(int(math.floor(px / cell)), int(math.floor(pz / cell))) for px, _, pz in v}
    local = unary_union([Polygon([(i * cell, j * cell), ((i + 1) * cell, j * cell), ((i + 1) * cell, (j + 1) * cell),
                                  (i * cell, (j + 1) * cell)]) for i, j in cells]).buffer(cell, join_style=2).buffer(-cell, join_style=2)
    a = math.radians(yaw)
    ca, sa = math.cos(a), math.sin(a)
    # its +x to (cos, sin), its +z to (sin, -cos)
    return shapely_affine(local, [ca, sa, sa, -ca, x, y])


def strip_mall(p, catalog, choices, rng, mall=None, diner=None):
    """The other anchor + L block (a pack's demo block): an L-shaped market ('mall', its wings along the
    block's two back sides, its fronts in), a diner on the street corner facing the camera, the rest of the back sides
    shops facing in, and in the middle the parking lot. 'p' is plan()'s. Returns (buildings [{asset, x, y, yaw, poly}],
    the lot's polygon).

    The market model's L is measured in its own frame: its two wings meet at its low x, low z corner, the long wing
    along +x with its front to +z, the short one along +z with its front to +x."""
    out = []
    inside = p['block'].buffer(0.05)
    corner = np.array(p['inner'])
    backs = p['backs']
    if mall and mall in catalog['models']:
        m = catalog['models'][mall]
        lo = m['low']
        for long_side in sorted(backs, key=lambda s: -math.dist(s['a'], s['b'])):
            n = np.array(long_side['n'])
            yaw = parcels.yaw_facing(-n[0], -n[1])
            a = math.radians(yaw)
            X, Z = np.array([math.cos(a), math.sin(a)]), np.array([math.sin(a), -math.cos(a)])
            # the wing along +x must run away from the corner along this side (the model cannot be mirrored)
            far = np.array(long_side['a']) if np.allclose(long_side['b'], corner) else np.array(long_side['b'])
            if np.dot(far - corner, X) <= 0:
                continue
            # from the block's own corner there (its bounding rectangle's corner may lie outside a cut block), stepped
            # in along both sides until the whole L is inside
            start = np.array(min(p['block'].exterior.coords, key=lambda q: math.dist(q, corner)))
            for inset in np.arange(0.4, 8.0, 0.2):
                origin = start + (X + Z) * inset - X * lo[0] - Z * lo[2]
                foot = parcels.footprint_poly(m, float(origin[0]), float(origin[1]), yaw)
                if inside.contains(foot):
                    out.append({'asset': mall, 'x': round(float(origin[0]), 3), 'y': round(float(origin[1]), 3),
                                'yaw': round(yaw, 2), 'poly': foot,
                                'shape': model_footprint(m, float(origin[0]), float(origin[1]), yaw)})
                    break
            if out:
                break
    taken = [o.get('shape', o['poly']) for o in out]
    # the diner on the street corner facing the camera, its front to the camera
    if diner and diner in catalog['models']:
        m = catalog['models'][diner]
        cx, cy = min(p['block'].exterior.coords, key=lambda q: math.dist(q, p['corner']))
        c = p['block'].centroid
        k = math.hypot(c.x - cx, c.y - cy) or 1.0
        yaw = parcels.yaw_facing(*p['camera'])
        for reach in np.arange(6.0, 30.0, 0.5):
            x, y, foot = parcels.centered(m, cx + (c.x - cx) / k * reach, cy + (c.y - cy) / k * reach, yaw)
            if inside.contains(foot) and not any(foot.intersects(t) for t in taken):
                out.append({'asset': diner, 'x': round(x, 3), 'y': round(y, 3), 'yaw': round(yaw, 2), 'poly': foot})
                taken.append(foot)
                break
    # what is left of the back sides: shops facing in
    fits = [(mm, w) for mm, w in choices if catalog['models'][mm]['size'][1] <= p['depth'] + 0.5]
    for side in backs:
        a, b, n = np.array(side['a']), np.array(side['b']), np.array(side['n'])
        length = float(np.linalg.norm(b - a))
        t = (b - a) / length
        s = 0.0
        while s < length - 4.0 and fits:
            key = rng.choices([mm for mm, _ in fits], [w for _, w in fits])[0]
            m = catalog['models'][key]
            c = a + t * (s + m['size'][0] / 2) - n * (m['size'][1] / 2 + 0.3)
            yaw = parcels.yaw_facing(-n[0], -n[1])
            for push in np.arange(0.0, 2.75, 0.25):
                x, y, foot = parcels.centered(m, float(c[0] - n[0] * push), float(c[1] - n[1] * push), yaw)
                if inside.contains(foot):
                    break
            if inside.contains(foot) and not any(foot.intersects(tk.buffer(-0.05)) for tk in taken):
                out.append({'asset': key, 'x': round(x, 3), 'y': round(y, 3), 'yaw': round(yaw, 2), 'poly': foot})
                taken.append(foot)
                s += m['size'][0] + 0.15
            else:
                s += 1.0
    lot = p['block'].buffer(-0.3, join_style=2).difference(unary_union(taken).buffer(0.6)) if taken else p['block']
    return out, lot
