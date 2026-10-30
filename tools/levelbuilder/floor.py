"""The height of the floor as it is drawn, for what walks on it: the game is flat (its physics at height 0), but the
asphalt is drawn under the sidewalks (roadmesh.ROAD_Y), the sand a little under the promenade and the pier's deck over
it. A character drawn at 0 sinks into a raised floor or floats over a sunken one; with this grid the game draws it on
the floor it stands on.

    floor.build(scene_json, level_dir) -> <level_dir>/floor.bin, and the level's "floor":
        {"file": "floor.bin", "bounds": [x0, y0, x1, y1], "cell": meters, "columns": n, "rows": m}
The file holds one byte per cell, row after row from the south edge (y0) north, west to east: the height in
centimeters plus 128.
"""
import json
from pathlib import Path

from PIL import Image, ImageDraw

import roadmesh
import streetnet

CELL = 0.5
# the places regions.bin tells apart, by their index (the level's floor "regions")
REGIONS = ['street', 'beach', 'malecon', 'green', 'water']
# the drawn floors, in the order they are laid (the later over the earlier). The scene keeps an area's outer rings only:
# the road's holes (the river, the blocks) are laid again at 0 over it, with the sidewalks, the promenade and the alleys
HEIGHTS = [('beach', -0.04), ('road', roadmesh.ROAD_Y), ('river', 0.0), ('bank', 0.0), ('bridges', roadmesh.ROAD_Y), ('blocks', 0.0),
           ('sidewalk', 0.0), ('promenade', 0.0),
           ('alley', 0.0), ('pier', 0.05)]


def build(scene_path, level_dir, cell=CELL):
    scene = json.loads(Path(scene_path).read_text(encoding='utf-8'))
    areas = scene['network']['areas']
    x0, y0, x1, y1 = scene.get('bounds', [-230, -210, 210, 210])
    columns, rows = int((x1 - x0) / cell) + 1, int((y1 - y0) / cell) + 1
    image = Image.new('L', (columns, rows), 128)
    draw = ImageDraw.Draw(image)

    def px(p):
        return ((p[0] - x0) / cell, (p[1] - y0) / cell)   # row 0 at the south edge

    rings = dict(areas, blocks=[block['points'] for block in scene['network'].get('blocks', [])])
    # a seawall's steps down to the sea (streetnet: steps_0 the outer, lowest)
    steps = sum(1 for name in areas if name.startswith('steps_'))
    for name, height in HEIGHTS + [(f'steps_{k}', -streetnet.STEP * (steps - k)) for k in range(steps)]:
        for ring in rings.get(name, []):
            if len(ring) >= 3:
                draw.polygon([px(p) for p in ring], fill=max(0, min(255, round(height * 100) + 128)))
    level_dir = Path(level_dir)
    (level_dir / 'floor.bin').write_bytes(image.tobytes())
    # where the floor is water (the sea, the river; not the bridges, the pier): a byte a cell, 255 water, on the same grid.
    # What lands there sinks (a body's pieces) and leaves no mark (blood, cracks)
    water = Image.new('L', (columns, rows), 0)
    wdraw = ImageDraw.Draw(water)
    for name, value in (('sea', 255), ('river', 255), ('bridges', 0), ('pier', 0), ('beach', 0), ('promenade', 0)):
        for ring in rings.get(name, []):
            if len(ring) >= 3:
                wdraw.polygon([px(p) for p in ring], fill=value)
    for k in range(steps):
        for ring in rings.get(f'steps_{k}', []):
            if len(ring) >= 3:
                wdraw.polygon([px(p) for p in ring], fill=0)
    (level_dir / 'water.bin').write_bytes(water.tobytes())
    # the kind of place each cell is (a byte a cell, an index into REGIONS): what the game's sounds of the place go by
    region = Image.new('L', (columns, rows), 0)
    rdraw = ImageDraw.Draw(region)
    green_zones = [z['poly'] for z in scene.get('zones', {}).values() if z.get('kind') in ('park', 'plaza') and z.get('poly')]
    layers = [('water', [rings.get('sea', []), rings.get('river', [])]),
              ('green', [rings.get('bank', []), rings.get('hill', []), rings.get('verges', [])] + [[p for p in z] for z in green_zones]),
              ('beach', [rings.get('beach', [])]),
              ('malecon', [rings.get('promenade', []), rings.get('malecon', []), rings.get('pier', [])] +
               [rings.get(f'steps_{k}', []) for k in range(steps)])]
    for name, groups in layers:
        value = REGIONS.index(name)
        for group in groups:
            for ring in group:
                if len(ring) >= 3:
                    rdraw.polygon([px(p) for p in ring], fill=value)
    (level_dir / 'regions.bin').write_bytes(region.tobytes())
    level_path = level_dir / 'level.json'
    level = json.loads(level_path.read_text(encoding='utf-8'))
    level['floor'] = {'file': 'floor.bin', 'water': 'water.bin', 'regions': {'file': 'regions.bin', 'names': REGIONS},
                      'bounds': [x0, y0, x0 + columns * cell, y0 + rows * cell], 'cell': cell, 'columns': columns, 'rows': rows}
    level_path.write_text(json.dumps(level, indent=1), encoding='utf-8')
    return level_dir / 'floor.bin'
