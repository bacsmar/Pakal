"""Survival mission 1 on the Downtown base: what a mission adds is props, spawners, the gift box and a script.

Props that break take their rules (health, drops, blast) from the original game's object of the same kind, copied from
the converted Survival level; the rest are plain colliders. Everything is in meters.
"""
import math
import random

import citykit
import downtown
from downtown import PITCH, HALF_CROSSING, STREET_HALF, BLOCK

# What breaks, by the original object whose rules it uses
TEMPLATE_OF = {
    'TrafficCone_1': 'Traffic Cone', 'TrafficCone_2': 'Traffic Cone',
    'FireHydrant': 'Fire Hydrant',
    'TrashBag_1': 'Trash Can', 'TrashBag_2': 'Trash Can', 'Barrel': 'Trash Can',
    'Vehicle_Pickup': 'Explosive_Gazelle_Blue', 'Vehicle_Sports': 'Explosive_Stang_red', 'Vehicle_Truck': 'Explosive_CopCar',
    'Vehicle_Pickup_Armored': 'Explosive_Gazelle_Brown', 'Vehicle_Sports_Armored': 'Explosive_Stang_purple',
}
ROUND = {'TrafficCone_1', 'TrafficCone_2', 'FireHydrant', 'Barrel', 'TrashBag_1'}
# A model shown in a breakable's place once it is broken (a burnt gas pump): a part of the same object, role "broken"
BROKEN_OF = {}

GIFT_ID = 45035
START_POINT_ID = 45036
FOCUS_ID = 45038


class Props:
    def __init__(self, level, templates):
        self.level = level
        self.templates = templates

    def add(self, model, x, y, yaw=0.0, scale=1.0, breakable=True):
        """A prop with a collider: it breaks when the original game's object of its kind would. Returns its object id (None when
        it does not break, or was not placed)."""
        level = self.level
        if any(math.hypot(x - cx, y - cy) < radius + 0.4 for cx, cy, radius in level.keep_clear):
            return None  # a tree stands there
        template = TEMPLATE_OF.get(model) if breakable else None
        low, high = level.model(model)
        corners = citykit.footprint(low, high, x, y, yaw, scale, 0.0)
        xs = [c[0] for c in corners]
        ys = [c[1] for c in corners]
        box = (min(xs), min(ys), max(xs), max(ys))
        level.add_box(corners, high[1] * scale)
        if template is None:
            level.place(model, x, y, yaw, scale=scale)
            level.solid(citykit.footprint(low, high, x, y, yaw, scale, 0.03))
            return None
        object_id = level.next_id
        level.next_id += 1
        level.place(model, x, y, yaw, scale=scale, object_id=object_id, label=template)
        if BROKEN_OF.get(model):
            level.place(BROKEN_OF[model], x, y, yaw, scale=scale, object_id=object_id, label=template)
            level.objects[-1]['role'] = 'broken'
        cx, cy = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
        if model in ROUND:
            wall = {'shape': 'circle', 'x': round(cx * 100, 2), 'y': round(cy * 100, 2), 'd': round(max(box[2] - box[0], box[3] - box[1]) * 100, 2)}
        else:
            wall = citykit.box_wall(low, high, x, y, yaw, scale)
        wall.update({'id': object_id, 'source': 'prop'})
        level.walls.append(wall)
        level.destructibles.append({'id': object_id, 'name': template, 'x': round(x * 100, 2), 'y': round(y * 100, 2),
                                    'components': self.templates[template]})
        return object_id


def street_cars(props, rng):
    """Cars left across the streets, as cover: long axis along the street, staggered across the lanes."""
    cars = ['Vehicle_Pickup', 'Vehicle_Sports', 'Vehicle_Truck', 'Vehicle_Pickup_Armored', 'Vehicle_Sports_Armored']
    lanes = (-4.5, -1.5, 1.5, 4.5)
    for j in (-1, 0, 1):          # east-west streets
        for i in (-1, 0):
            for k in (1, 4):
                x = i * PITCH + HALF_CROSSING + 6 * k + rng.uniform(-1.5, 1.5)
                y = j * PITCH + rng.choice(lanes)
                props.add(rng.choice(cars), x, y, 90.0 + rng.choice((0, 180)) + rng.uniform(-8, 8) * (1 if abs(y - j * PITCH) > 3 else 0))
    for i in (-1, 0, 1):          # north-south streets
        for j in (-1, 0):
            for k in (1, 4):
                y = j * PITCH + HALF_CROSSING + 6 * k + rng.uniform(-1.5, 1.5)
                x = i * PITCH + rng.choice(lanes)
                props.add(rng.choice(cars), x, y, 0.0 + rng.choice((0, 180)))


def street_clutter(props, rng):
    """Cones, hydrants and trash along the sidewalks and around the crossings."""
    for j in (-1, 0, 1):
        for i in (-1, 0):
            for k in range(6):
                x = i * PITCH + HALF_CROSSING + 6 * k + 3
                if rng.random() < 0.55:
                    props.add('TrashBag_1', x + rng.uniform(-1, 1), j * PITCH + rng.choice((-1, 1)) * rng.uniform(7.0, 8.4), rng.uniform(0, 360))
                if k in (2, 5):
                    props.add('FireHydrant', x, j * PITCH + rng.choice((-1, 1)) * 6.4, 0.0)
    for i in (-1, 0, 1):
        for j in (-1, 0):
            for k in range(6):
                y = j * PITCH + HALF_CROSSING + 6 * k + 3
                if rng.random() < 0.55:
                    props.add('TrashBag_1', i * PITCH + rng.choice((-1, 1)) * rng.uniform(7.0, 8.4), y + rng.uniform(-1, 1), rng.uniform(0, 360))
                if k in (1, 4):
                    props.add('FireHydrant', i * PITCH + rng.choice((-1, 1)) * 6.4, y, 0.0)
    # cones marking off lanes near every crossing
    for ni in (-1, 0, 1):
        for nj in (-1, 0, 1):
            cx, cy = ni * PITCH, nj * PITCH
            for n in range(5):
                props.add('TrafficCone_1', cx + HALF_CROSSING + 2 + n * 0.8, cy - 5.5 + n * 0.25, 0.0)
            for n in range(4):
                props.add('TrafficCone_1', cx - 5.5 + n * 0.25, cy - HALF_CROSSING - 2 - n * 0.8, 0.0)


def courtyards(props, level, rng):
    """Cover in the four courtyards, and the gift box in one of them. Returns the gift box position."""
    center_block = BLOCK / 2
    gift = None
    for bi in (0, 1):
        for bj in (0, 1):
            cx = (bi - 1) * PITCH + STREET_HALF + center_block
            cy = (bj - 1) * PITCH + STREET_HALF + center_block
            if (bi, bj) == (1, 0):
                gift = (cx, cy)
                props.add('Couch', cx - 3.2, cy + 3.0, 0.0, breakable=False)
                props.add('Barrel', cx + 3.2, cy - 3.2)
                props.add('Barrel', cx + 3.9, cy - 2.6)
                continue
            props.add('Container_Green' if (bi + bj) % 2 else 'Container_Red', cx + rng.uniform(-1.5, 1.5), cy + rng.uniform(-1.5, 1.5), 90.0 * rng.randint(0, 1), breakable=False)
            props.add('Barrel', cx + 4.0, cy + 3.0)
            props.add('Pallet', cx - 3.5, cy - 3.0, rng.uniform(0, 90), breakable=False)
            props.add('TrashBag_2', cx - 3.0, cy + 3.4, 20.0)
    return gift


def spawn_points(level):
    """Zombies come in from behind the barricades: two points just inside each, plus the mouths of the alleys."""
    points = []
    for ni in (-1, 0, 1):
        for nj in (-1, 0, 1):
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if abs(ni + dx) > 1 or abs(nj + dy) > 1:
                    cx, cy = ni * PITCH + dx * (HALF_CROSSING - 7.5), nj * PITCH + dy * (HALF_CROSSING - 7.5)
                    for side in (-4.0, 4.0):
                        points.append((cx + (side if dx == 0 else 0.0), cy + (side if dy == 0 else 0.0)))
    return points


def script_for(hollywood_script):
    """Objective 5804's waves (the original's Run hook) with a shorter intro that only uses what the game implements."""
    on_activate = '''
WarpPlayer(%d);
SetGameVariable("killCountRequired", %d);
SetGameVariable("killCount", 0);
SetGameVariable("A", 0);
SetGameVariable("difficultyLevel", 0);

EnableControls(false);
CameraSetManualPan(%d, "0 -3200 2400", %d, "3000 0 1500", 7);
ShowAutoDialog("DOWNTOWN: kill %d of the infected", 7000);
CameraReturnToPlayer();
EnableControls(true);

Wait(1000);
EnableObject(5292);
SetEnemySpawn(25001, 8, 900, 0);
SetEnemySpawn(25009, 5, 2000, 0);
SetObjectiveText("SURVIVE", -1);
Wait(8000);
''' % (START_POINT_ID, KILLS, FOCUS_ID, FOCUS_ID, KILLS)
    return {'order': ['5804'], 'objectives': {'5804': {'onActivate': on_activate, 'run': HORDE_RUN, 'onDeactivate': ''}}}


# How many kills win the level, and the most zombies alive at once in its waves (the pool of models has some more: bodies stay
# a moment after dying)
KILLS = 80
ENEMY_POOL = 40

# The waves of Downtown: the original's shape (a push, a lull until the street is clear, a bigger push, berserk at the end)
# with hordes about twice as big and 80 kills to win. The special zombies join little by little: tourists in the first push,
# boomers and a dog walker in the horde, a zombinator at the end.
HORDE_RUN = '''
enemyCount = GetGameVariable("enemyCount");
killCount = GetGameVariable("killCount");
killCountRequired = GetGameVariable("killCountRequired");
A = GetGameVariable("A");

-- the first push grows
if killCount >= 12 and A == 0 then
SetGameVariable("A", 1);
SetEnemySpawn(25001, 14, 800, 0);
SetEnemySpawn(25009, 8, 1800, 0);
SetEnemySpawn(25010, 2, 5000, 0);
end;

-- a lull: no new zombies until the street is clear
if killCount >= 30 and A == 1 then
SetGameVariable("A", 2);
SetEnemySpawn(25001, 0, 4000, 0);
SetEnemySpawn(25009, 0, 3000, 0);
SetEnemySpawn(25010, 0, 3000, 0);
end;

-- the horde, and the gift box somewhere in the city
if enemyCount <= 0 and A == 2 then
Wait(4000);
SetGameVariable("A", 3);
SetEnemySpawn(25001, 18, 700, 0);
SetEnemySpawn(25009, 10, 1500, 0);
SetEnemySpawn(25010, 3, 3000, 0);
SetEnemySpawn(25016, 2, 6000, 0);
SetEnemySpawn(25012, 1, 9000, 0);
EnableRandomObject(45035, true, 16);
end;

-- berserk finish
if killCount >= 60 and A == 3 then
SetGameVariable("A", 4);
SetEnemySpawn(25001, 20, 600, 0);
SetEnemySpawn(25009, 12, 1200, 0);
SetEnemySpawn(25011, 1, 12000, 0);
EnableBerserk(true);
ShakeCamera(3500, "35");
ShowAutoDialog("BERSERK.Berserk_01.text", 3500);
end;

if killCount >= killCountRequired then
SetEnemySpawn(25001, 0, 1000, 0);
SetEnemySpawn(25009, 0, 7000, 0);
SetEnemySpawn(25010, 0, 1000, 0);
SetEnemySpawn(25016, 0, 1000, 0);
SetEnemySpawn(25012, 0, 1000, 0);
SetEnemySpawn(25011, 0, 1000, 0);
KillAllZombies();
RemoveObjectiveText();
DoneKillAll();
end;
'''


def build(level, hollywood, seed=2026):
    rng = random.Random(seed)
    templates = {d['name']: d['components'] for d in hollywood['destructibles']}
    props = Props(level, templates)
    street_cars(props, rng)
    street_clutter(props, rng)
    gift = courtyards(props, level, rng)
    # the gift box: the kit's special chest, hidden until the script asks for it
    level.place('Chest_Special', gift[0], gift[1], 0.0, scale=1.6, object_id=GIFT_ID, label='GiftBox')
    spawners = spawn_points(level)
    start = (0.0, -3.0)
    return {
        'spawn': start,
        'spawners': spawners,
        'points': {str(START_POINT_ID): start},
        'anchors': {str(FOCUS_ID): (0.0, 0.0, 0.0)},
        'script': script_for(hollywood['script']),
        'enemyPool': ENEMY_POOL,
    }
