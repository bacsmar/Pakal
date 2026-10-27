"""The base of the Downtown levels: a city made of the Quaternius Downtown City pieces, at their natural size.

A 3x3 grid of crossings joined by four-lane streets (12 m of asphalt and a 3 m sidewalk on each side), four blocks with
buildings in a pinwheel (5.5 m alleys lead to a courtyard), a ring of buildings along the outer streets, and barricades
closing every street that leaves the map. Each building, barricade and street light gets a collider from its own
footprint. Missions add props, spawners and scripts on top (see mission_survival.py), so this base never changes.

Everything here is in meters (x east, y north); Level converts to the file's centimeters.
"""
import json
import math
import os
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
import citykit  # noqa: E402

CROSSING = 24.66626          # Street_4WayIntersection, both sides, with its crosswalks
CROSSING_ASPHALT = 18.0      # the same without them: the crosswalks reach 3.3 m past its asphalt, over the street
SEGMENT = 6.0                # Street_4Lane, along the street
ARM_SEGMENTS = 6
PITCH = CROSSING + SEGMENT * ARM_SEGMENTS   # distance between crossings
STREET_HALF = 9.0            # half of the 18 m of a four-lane street with its sidewalks
BLOCK = PITCH - 2 * STREET_HALF
HALF_CROSSING = CROSSING / 2
FILLER = HALF_CROSSING - CROSSING_ASPHALT / 2   # Street_4Lane_Short: the street under the crosswalks (own_assets.py)


def crossing_fillers(x, y):
    """(x, y, yaw) of the four short street pieces around a crossing: between its asphalt and the first street piece,
    under the crosswalks (without them the sky shows between the stripes)."""
    reach = CROSSING_ASPHALT / 2 + FILLER / 2
    return [(x + reach, y, 0.0), (x - reach, y, 0.0), (x, y + reach, 90.0), (x, y - reach, 90.0)]


# The look of the picture (see BgfxGraphicsSystem::PostSettings): warm grading, a tilt-shift blur toward the top and bottom
# edges (the city looks like a model), and ambient occlusion
POST = {
    'enabled': True, 'exposure': 1.14, 'contrast': 1.12, 'saturation': 1.18, 'gain': [1.03, 1.0, 0.95], 'vignette': 0.38,
    'tiltBlur': 7.0, 'tiltFocus': 0.52, 'tiltBand': 0.2,
    'aoStrength': 1.0, 'aoRadius': 0.9, 'aoBias': 0.06, 'aoRange': 2.5,
}


# A late afternoon: warm low sun, a cool sky as ambient light and a warm bounce from the ground (render space: x east, y up, z south)
LIGHTING = {
    'sunDirection': [0.35, -0.9, -0.3], 'sunColor': [1.12, 0.97, 0.8],
    'ambient': [0.45, 0.47, 0.54], 'ambientGround': [0.3, 0.26, 0.21],
    'skyTop': [0.26, 0.46, 0.78], 'skyHorizon': [0.92, 0.84, 0.74],
    'shadows': {'enabled': True, 'mapSize': 2048, 'extent': 38.0, 'strength': 0.75, 'bias': 0.0012, 'normalOffset': 0.08},
}


class Level:
    """A level being built: models placed, colliders, and the data the game reads."""

    def __init__(self, name, asset_dir, shared_dir):
        self.name = name
        self.asset_dir = Path(asset_dir)      # where this level's level.json goes
        self.shared = Path(shared_dir)        # where the models are exported (shared by every Downtown level)
        self.objects = []
        self.walls = []                       # props with an id: axis aligned boxes and circles (bullets break them)
        self.polygons = []                    # solid colliders (convex, up to 8 corners)
        self.destructibles = []
        self.footprints = []                  # (name, AABB) of the big things, to catch overlaps
        self.boxes = []                       # (x0, y0, x1, y1, height) of what blocks light: the baked global illumination
        self.bounds = {}                      # model name -> (low, high)
        self.next_id = 1000
        self.keep_clear = []                  # (x, y, radius): spots taken (trees) where props are not put
        self.static = True                    # what is placed now stays put: it counts for the baked light

    def model(self, name):
        if name not in self.bounds:
            self.bounds[name] = citykit.model_bounds(citykit.find_model(name))
        return self.bounds[name]

    def mesh_path(self, name):
        citykit.export_model(name, self.shared)
        relative = os.path.relpath(self.shared / 'meshes' / name / f'{name}.gltf', self.asset_dir)
        return Path(relative).as_posix()

    def place(self, name, x, y, yaw=0.0, z=0.0, scale=1.0, object_id=-1, label=None):
        low, high = self.model(name)
        self.objects.append({
            'name': label or name, 'id': object_id, 'mesh': self.mesh_path(name),
            'x': round(x * 100, 2), 'y': round(y * 100, 2), 'z': round(z * 100, 2), 'rot': [0, 0, yaw], 'scale': scale,
        })

    def solid(self, corners, label=''):
        """A solid collider (convex polygon of up to 8 corners, meters)."""
        self.polygons.append([[round(cx * 100, 2), round(cy * 100, 2)] for cx, cy in corners])

    def collide(self, name, x, y, yaw=0.0, scale=1.0, shrink=0.1):
        low, high = self.model(name)
        corners = citykit.footprint(low, high, x, y, yaw, scale, shrink)
        self.solid(corners, name)
        self.add_box(corners, high[1] * scale)
        return corners

    def add_box(self, corners, height):
        """Something that casts shadow and bounces light in the baked illumination (low things do not matter)."""
        if height < 0.8 or not self.static:
            return
        xs = [c[0] for c in corners]
        ys = [c[1] for c in corners]
        self.boxes.append((min(xs), min(ys), max(xs), max(ys), height))

    def aabb(self, corners):
        xs = [c[0] for c in corners]
        ys = [c[1] for c in corners]
        return min(xs), min(ys), max(xs), max(ys)

    def overlaps(self, box, margin=0.0):
        for _, other in self.footprints:
            if box[0] < other[2] + margin and box[2] > other[0] - margin and box[1] < other[3] + margin and box[3] > other[1] - margin:
                return True
        return False

    def building(self, name, x, y, yaw):
        corners = self.collide(name, x, y, yaw, shrink=0.15)
        box = self.aabb(corners)
        if self.overlaps(box):
            print(f'warning: {name} at ({x:.1f}, {y:.1f}) overlaps another building')
        self.place(name, x, y, yaw)
        self.footprints.append((f'{name}@({x:.0f},{y:.0f})', box))
        return box

    def to_json(self, base_level, roles, visuals, script, spawn, spawners, anchors, points, mode='Survival'):
        return {
            'format': 'pakal-level', 'version': 2, 'name': self.name, 'mode': mode, 'units_per_meter': 100,
            'roles': roles,
            'spawn': [round(spawn[0] * 100, 2), round(spawn[1] * 100, 2)],
            'walls': self.walls,
            'script': {'order': script['order'], 'points': {k: [round(v[0] * 100, 2), round(v[1] * 100, 2)] for k, v in points.items()},
                       'anchors': {k: [round(v[0] * 100, 2), round(v[1] * 100, 2), round(v[2] * 100, 2)] for k, v in anchors.items()},
                       'objectives': script['objectives']},
            'spawners': [{'x': round(sx * 100, 2), 'y': round(sy * 100, 2)} for sx, sy in spawners],
            'collision': {'segments': [{'layer': 'solid', 'filled': True, 'points': poly} for poly in self.polygons]},
            'ambientActors': [], 'pickupSources': [],
            'destructibles': self.destructibles,
            'rules': base_level['rules'],
            'enemies': [], 'pickups': [],
            'visuals': visuals,
            'post': POST,
            'lighting': LIGHTING,
            'objects': self.objects,
        }


def building_front_corner(low, high, yaw):
    """Offsets (from the model origin) of the front-left corner rectangle: returns the AABB of the footprint at yaw."""
    corners = citykit.footprint(low, high, 0.0, 0.0, yaw)
    xs = [c[0] for c in corners]
    ys = [c[1] for c in corners]
    return min(xs), min(ys), max(xs), max(ys)


SIDE_YAW = {'S': 0.0, 'E': 90.0, 'N': 180.0, 'W': 270.0}   # the front of a model faces level south at yaw 0


def block_building(level, name, block_x0, block_y0, side, offset):
    """A building on a side of a block, its front on the block edge, at least 'offset' meters from that side's start corner.

    Pinwheel: the S side starts at its west end, E at its south end, N at its east end and W at its north end. If the
    spot is taken by a building already placed, it slides along the side (0.25 m at a time) to the first free one.
    """
    low, high = level.model(name)
    yaw = SIDE_YAW[side]
    ax0, ay0, ax1, ay1 = building_front_corner(low, high, yaw)
    x1, y1 = block_x0 + BLOCK, block_y0 + BLOCK
    gap = 0.4
    step = 0.25
    while offset < BLOCK:
        if side == 'S':
            x, y = block_x0 + offset - ax0, block_y0 - ay0
        elif side == 'E':
            x, y = x1 - ax1, block_y0 + offset - ay0
        elif side == 'N':
            x, y = x1 - offset - ax1, y1 - ay1
        else:
            x, y = block_x0 - ax0, y1 - offset - ay1
        box = (x + ax0, y + ay0, x + ax1, y + ay1)
        if not level.overlaps(box, gap):
            return level.building(name, x, y, yaw)
        offset += step
    print(f'warning: no room for {name} on the {side} side of the block at ({block_x0:.0f}, {block_y0:.0f})')
    return None


def build_streets(level):
    nodes = [(i * PITCH, j * PITCH) for i in (-1, 0, 1) for j in (-1, 0, 1)]
    for x, y in nodes:
        level.place('Street_4WayIntersection', x, y)
        for fx, fy, yaw in crossing_fillers(x, y):
            level.place('Street_4Lane_Short', fx, fy, yaw)
    # four-lane streets between neighbouring crossings: horizontal ones run along x (yaw 0), vertical ones along y (yaw 90)
    for j in (-1, 0, 1):
        for i in (-1, 0):
            for k in range(ARM_SEGMENTS):
                x = i * PITCH + HALF_CROSSING + SEGMENT * (k + 0.5)
                level.place('Street_4Lane', x, j * PITCH, 0.0)
    for i in (-1, 0, 1):
        for j in (-1, 0):
            for k in range(ARM_SEGMENTS):
                y = j * PITCH + HALF_CROSSING + SEGMENT * (k + 0.5)
                level.place('Street_4Lane', i * PITCH, y, 90.0)
    # lane markings: a double yellow line down the middle of each street piece
    for j in (-1, 0, 1):
        for i in (-1, 0):
            for k in range(ARM_SEGMENTS):
                level.place('Decal_DoubleYellow_Straight', i * PITCH + HALF_CROSSING + SEGMENT * (k + 0.5), j * PITCH, 0.0, z=0.09, label='lane')
    for i in (-1, 0, 1):
        for j in (-1, 0):
            for k in range(ARM_SEGMENTS):
                level.place('Decal_DoubleYellow_Straight', i * PITCH, j * PITCH + HALF_CROSSING + SEGMENT * (k + 0.5), 90.0, z=0.09, label='lane')
    return nodes


def build_blocks(level):
    """Four blocks, each a pinwheel of buildings with 5.5 m alleys to a courtyard."""
    plans = {
        (0, 0): [('S', 'Building_Large_2', 0), ('E', 'Building_Large_2', 0), ('N', 'Building_Large_2', 0), ('W', 'Building_Large_2', 0)],
        (1, 0): [('S', 'Building_Medium_2_001', 0), ('S', 'Building_Small_1', 19.5), ('E', 'Building_Large_2', 0),
                 ('N', 'Building_Medium_2_001', 0), ('W', 'Building_Medium_2_001', 0)],
        (0, 1): [('S', 'Building_Large_2', 0), ('E', 'Building_Medium_2_001', 0), ('N', 'Building_Large_2', 0),
                 ('W', 'Building_Medium_2_001', 0)],
        (1, 1): [('S', 'Building_Medium_2_001', 0), ('E', 'Building_Small_1', 0), ('N', 'Building_Medium_2_001', 0),
                 ('W', 'Building_Small_1', 0)],
    }
    for (bi, bj), plan in plans.items():
        x0 = (bi - 1) * PITCH + STREET_HALF
        y0 = (bj - 1) * PITCH + STREET_HALF
        for side, name, offset in plan:
            block_building(level, name, x0, y0, side, offset)


def build_ring(level):
    """Buildings along the outer sides of the outer streets, and one closing the end of every street that leaves the map."""
    edge = PITCH + STREET_HALF                       # outer edge of the outer sidewalk
    models = ['Building_Large_2', 'Building_Medium_2_001', 'Building_Small_1', 'Building_Large_2', 'Building_Small_1', 'Building_Medium_2_001']
    # each outer side has two stretches between crossings; buildings are packed along them
    for side in ('N', 'S', 'E', 'W'):
        yaw_front = {'N': 0.0, 'S': 180.0, 'E': 270.0, 'W': 90.0}[side]   # facing the street (south for the north ring...)
        for stretch in (-1, 0):
            start = stretch * PITCH + HALF_CROSSING + 1.0
            end = (stretch + 1) * PITCH - HALF_CROSSING - 1.0
            along = start
            index = (stretch + 1 + 2 * 'NSEW'.index(side)) % len(models)
            while True:
                name = models[index % len(models)]
                index += 1
                low, high = level.model(name)
                ax0, ay0, ax1, ay1 = building_front_corner(low, high, yaw_front)
                width = (ax1 - ax0) if side in ('N', 'S') else (ay1 - ay0)
                if along + width > end:
                    break
                if side == 'N':
                    x, y = along - ax0, edge - ay0
                elif side == 'S':
                    x, y = along - ax0, -edge - ay1
                elif side == 'E':
                    x, y = edge - ax0, along - ay0
                else:
                    x, y = -edge - ax1, along - ay0
                level.building(name, x, y, yaw_front)
                along += width + 1.0
    # the streets that leave the map end at a building across their axis, behind a barricade
    for ni in (-1, 0, 1):
        for nj in (-1, 0, 1):
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if abs(ni + dx) > 1 or abs(nj + dy) > 1:
                    # outward arm of the crossing (ni, nj) in direction (dx, dy)
                    cx, cy = ni * PITCH, nj * PITCH
                    ex, ey = cx + dx * HALF_CROSSING, cy + dy * HALF_CROSSING
                    barricade(level, ex, ey, dx, dy)
                    yaw = {(0, 1): 180.0, (0, -1): 0.0, (1, 0): 90.0, (-1, 0): 270.0}[(dx, dy)]
                    # the building's front faces back toward the crossing: the front direction is -(dx, dy)
                    front_yaw = {(0, 1): 0.0, (0, -1): 180.0, (1, 0): 270.0, (-1, 0): 90.0}[(dx, dy)]
                    low, high = level.model('Building_Large_2')
                    ax0, ay0, ax1, ay1 = building_front_corner(low, high, front_yaw)
                    gap = 1.5
                    tx, ty = ex + dx * gap, ey + dy * gap
                    # center the facade on the street axis, front edge at the gap
                    if dx == 0:
                        x = tx - (ax0 + ax1) / 2
                        y = ty - (ay0 if dy > 0 else ay1)
                    else:
                        x = tx - (ax0 if dx > 0 else ax1)
                        y = ty - (ay0 + ay1) / 2
                    level.building('Building_Large_2', x, y, front_yaw)


def barricade(level, ex, ey, dx, dy):
    """Three containers across the end of a street, and an armored truck behind them."""
    # along-the-street offset: containers sit just inside the end of the crossing
    cx, cy = ex - dx * 1.6, ey - dy * 1.6
    cross_x, cross_y = (1, 0) if dx == 0 else (0, 1)
    yaw = 0.0 if dx == 0 else 90.0       # container long axis along the cross direction
    for k, name in zip((-1, 0, 1), ('Container_Red', 'Container_Green', 'Container_Red')):
        px, py = cx + cross_x * k * 5.9, cy + cross_y * k * 5.9
        level.collide(name, px, py, yaw, shrink=0.05)
        level.place(name, px, py, yaw)


def build_street_lights(level):
    """A street light every 18 m on both sidewalks of each street piece run, pointing its arm over the asphalt."""
    low, high = level.model('StreetLights')
    for j in (-1, 0, 1):
        for i in (-1, 0):
            start = i * PITCH + HALF_CROSSING + 6
            for k in range(2):
                x = start + k * 18 + 6
                for side in (-1, 1):
                    y = j * PITCH + side * 7.6
                    level.place('StreetLights', x, y, 180.0 if side < 0 else 0.0)  # the arm reaches over the asphalt
                    level.solid(circle_corners(x, y, 0.2), 'light')
    for i in (-1, 0, 1):
        for j in (-1, 0):
            start = j * PITCH + HALF_CROSSING + 6
            for k in range(2):
                y = start + k * 18 + 6
                for side in (-1, 1):
                    x = i * PITCH + side * 7.6
                    level.place('StreetLights', x, y, 90.0 if side < 0 else 270.0)
                    level.solid(circle_corners(x, y, 0.2), 'light')


# Street trees of the nature kit (7-9 m tall), two per sidewalk of each street stretch
TREES = ['CommonTree_1', 'CommonTree_2', 'CommonTree_3', 'CommonTree_5', 'CommonTree_4']


def build_trees(level):
    """Trees between the street lights of each sidewalk (the lights stand 12 and 30 m into a stretch, past the crossing), so the city is not only
    buildings. Their names start with 'Tree' so the game fades them like the buildings when they hide the player."""
    count = 0
    for j in (-1, 0, 1):
        for i in (-1, 0):
            for side in (-1, 1):
                for along in (6, 21):
                    plant_tree(level, i * PITCH + HALF_CROSSING + along, j * PITCH + side * 7.6, count)
                    count += 1
    for i in (-1, 0, 1):
        for j in (-1, 0):
            for side in (-1, 1):
                for along in (6, 21):
                    plant_tree(level, i * PITCH + side * 7.6, j * PITCH + HALF_CROSSING + along, count)
                    count += 1


def plant_tree(level, x, y, index):
    name = TREES[index % len(TREES)]
    level.place(name, x, y, (index * 137.0) % 360.0, scale=0.85 + 0.05 * (index % 4), label=f'Tree_{name}')
    level.solid(circle_corners(x, y, 0.25), 'trunk')
    level.keep_clear.append((x, y, 1.0))


def circle_corners(x, y, radius, n=6):
    return [(x + radius * math.cos(2 * math.pi * i / n), y + radius * math.sin(2 * math.pi * i / n)) for i in range(n)]


def build_ground(level):
    """Asphalt under every block (alleys and courtyards): 9 m tiles at their natural size, a little past the block edge."""
    low, high = level.model('Street_Asphalt_9x9')
    ax0, ay0, ax1, ay1 = building_front_corner(low, high, 0.0)
    size = ax1 - ax0
    count = math.ceil((BLOCK + 0.6) / size)
    for bi in (0, 1):
        for bj in (0, 1):
            x0 = (bi - 1) * PITCH + STREET_HALF - 0.3
            y0 = (bj - 1) * PITCH + STREET_HALF - 0.3
            for ix in range(count):
                for iy in range(count):
                    level.place('Street_Asphalt_9x9', x0 + ix * size - ax0, y0 + iy * size - ay0, 0.0, label='ground')


def build_corners(level):
    """Sidewalk where the crossing piece ends short of the street sidewalks: two 3.3 x 3 m gaps at each corner of a crossing."""
    reach = (STREET_HALF + HALF_CROSSING) / 2          # middle of the gap along the crossing's edge
    depth = (STREET_HALF - 6.0) / 2 + 6.0              # middle of the sidewalk strip (6 to 9 m from the street axis)
    scale = (HALF_CROSSING - STREET_HALF) / 3.0 * 1.03
    for ni in (-1, 0, 1):
        for nj in (-1, 0, 1):
            for sx in (-1, 1):
                for sy in (-1, 1):
                    for gx, gy in ((reach, depth), (depth, reach)):
                        level.place('Sidewalk_NoCurb_3m', ni * PITCH + sx * gx, nj * PITCH + sy * gy, 0.0, z=-0.04, scale=scale, label='corner')


def build_base(level):
    build_streets(level)
    build_corners(level)
    build_ground(level)
    build_blocks(level)
    build_ring(level)
    build_street_lights(level)
    build_trees(level)
