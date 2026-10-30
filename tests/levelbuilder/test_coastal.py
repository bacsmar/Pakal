"""Tests of the level builder's coastal generator (no Blender, no game): the street network, the meshes, the samplers.

    python -m unittest discover tests/levelbuilder
"""
import math
import random
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / 'tools' / 'levelbuilder'))

import numpy as np  # noqa: E402
from shapely.geometry import Point, box  # noqa: E402

import geom  # noqa: E402
import parcels  # noqa: E402
import roadmesh  # noqa: E402
import spawners  # noqa: E402
import streetnet  # noqa: E402


class StreetNetwork(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.net = streetnet.generate(None, 3)

    def test_same_seed_same_network(self):
        again = streetnet.generate(None, 3)
        self.assertEqual(streetnet.generate(None, 3).to_json(), again.to_json())
        self.assertNotEqual(streetnet.generate(None, 4).to_json()['edges'], again.to_json()['edges'])

    def test_blocks_are_off_the_streets_and_the_water(self):
        road = self.net.areas['road']
        for b in self.net.blocks:
            self.assertLess(b['poly'].intersection(road).area, 0.5, b['id'])
            self.assertLess(b['poly'].intersection(self.net.areas['sea']).area, 0.5, b['id'])
            self.assertLess(b['poly'].intersection(self.net.areas['river']).area, 0.5, b['id'])

    def test_not_a_grid(self):
        # the cross streets point in more than one direction (they fan out with the coast and turn per district)
        angles = set()
        for e in self.net.edges:
            if e['kind'] == 'street':
                (x0, y0), (x1, y1) = e['line'].coords[0], e['line'].coords[-1]
                angles.add(round(math.degrees(math.atan2(y1 - y0, x1 - x0)) / 3))
        self.assertGreater(len(angles), 3)

    def test_there_are_plazas_bridges_and_entrances(self):
        self.assertTrue(any(b['kind'] == 'plaza' for b in self.net.blocks))
        self.assertTrue(any(e.get('bridge') for e in self.net.edges))
        self.assertGreater(len(self.net.entrances), 2)


class Meshes(unittest.TestCase):
    def test_flat_triangles_face_up_and_cover_the_polygon(self):
        mesh = roadmesh.Mesh()
        shape = box(0, 0, 50, 20).difference(Point(25, 10).buffer(4))
        mesh.flat(shape, 0.0, 'asphalt')
        area = 0.0
        for chunk in mesh.chunks.values():
            prim = chunk['asphalt']
            pos = np.array(prim['pos'])
            for i in range(0, len(prim['idx']), 3):
                a, b, c = (pos[prim['idx'][i + k]] for k in range(3))
                n = np.cross(b - a, c - a)
                self.assertGreater(n[1], 0.0)   # counterclockwise seen from +y: the front faces up
                area += np.linalg.norm(n) / 2
        self.assertAlmostEqual(area, shape.area, delta=0.5)

    def test_walls_face_their_side(self):
        mesh = roadmesh.Mesh()
        mesh.wall((0, 0), (10, 0), 0.0, 1.0, 'concrete', facing=(5, -3))
        prim = next(iter(mesh.chunks.values()))['concrete']
        pos = np.array(prim['pos'])
        a, b, c = (pos[prim['idx'][k]] for k in range(3))
        n = np.cross(b - a, c - a)
        self.assertGreater(n[2], 0.0)   # level south is glTF +z


class Samplers(unittest.TestCase):
    def test_poisson_keeps_its_spacing(self):
        region = box(0, 0, 60, 40).difference(Point(30, 20).buffer(8))
        points = spawners.poisson(region, 3.0, random.Random(1))
        self.assertGreater(len(points), 100)
        for i, p in enumerate(points):
            self.assertTrue(region.contains(Point(p)))
            for q in points[i + 1:]:
                self.assertGreaterEqual(math.hypot(p[0] - q[0], p[1] - q[1]), 3.0 - 1e-6)

    def test_masks(self):
        regions = {'sea': box(-10, 0, 0, 10)}
        pts = np.array([[1.0, 5.0], [9.0, 5.0]])
        fit = spawners.evaluate([{'type': 'distance', 'to': 'sea', 'range': [2, 6]}], pts, regions, 1)
        self.assertAlmostEqual(float(fit[0]), 1.0)
        self.assertAlmostEqual(float(fit[1]), 0.0)


class Parcels(unittest.TestCase):
    def test_buildings_stay_in_the_block_face_out_and_do_not_overlap(self):
        catalog = {'models': {
            'small': {'low': [-3, 0, -4], 'high': [3, 5, 4], 'size': [6, 8, 5]},
            'wide': {'low': [-6, 0, -5], 'high': [6, 8, 5], 'size': [12, 10, 8]},
        }}
        block = geom.Polygon([(0, 0), (60, 0), (55, 35), (5, 30)])
        placed, yard = parcels.fill_block(block, catalog, [('small', 1), ('wide', 1)], random.Random(2))
        self.assertGreater(len(placed), 6)
        for i, p in enumerate(placed):
            self.assertTrue(block.buffer(0.01).contains(p['poly']))
            # its front: the footprint's front edge is the one nearest the street (the block's edge)
            fx, fy = p['front']
            c = p['poly'].centroid
            ahead = Point(c.x + fx * 30, c.y + fy * 30)
            self.assertFalse(block.contains(ahead))
            for q in placed[i + 1:]:
                self.assertLess(p['poly'].intersection(q['poly']).area, 0.05)
        self.assertGreater(yard.area, 0)


if __name__ == '__main__':
    unittest.main()


class LotsAndStreets(unittest.TestCase):
    """What dresses a lot (dress.py), what hangs over the streets (roadmesh.cable, roadmesh.bunting), set pieces."""

    @staticmethod
    def catalog():
        # a 10 x 8 m building 6 m tall (its front faces +Z, level south when unrotated), a 1 x 0.6 m wall piece, a crate
        return {'models': {
            'bld': {'low': [-5.0, 0.0, -4.0], 'high': [5.0, 6.0, 4.0], 'size': [10.0, 8.0, 6.0], 'tags': ['building'], 'roles': ['shop']},
            'ac': {'low': [-0.5, 0.0, 0.0], 'high': [0.5, 0.8, 0.6], 'size': [1.0, 0.6, 0.8], 'tags': ['small'], 'roles': ['aircon']},
            'crate': {'low': [-0.4, 0.0, -0.4], 'high': [0.4, 0.8, 0.4], 'size': [0.8, 0.8, 0.8], 'tags': ['clutter'], 'roles': ['shopfront']},
        }}

    def test_wall_pieces_sit_on_the_facade_and_ground_pieces_in_front(self):
        import dress
        catalog = self.catalog()
        building = {'asset': 'bld', 'x': 0.0, 'y': 0.0, 'yaw': 0.0, 'scale': 1.0, 'role': 'shop'}
        recipe = [{'role': 'aircon', 'at': 'wall', 'height': [2.0, 4.0], 'count': [3, 3]},
                  {'role': 'shopfront', 'at': 'front', 'out': [0.3, 0.8], 'count': [2, 2]}]
        roles = {'aircon': ['ac'], 'shopfront': ['crate']}
        walk = box(-20, -12, 20, -4.0)
        got = dress.dress(building, recipe, catalog, lambda r: roles.get(r, []), spawners.Occupancy(), {'walk': walk},
                          random.Random(2))
        walls = [g for g in got if g['role'] == 'aircon']
        ground = [g for g in got if g['role'] == 'shopfront']
        self.assertTrue(walls and ground)
        for g in walls:
            # the facade is y = -4 (the front, south); the piece's back is on it, inside the facade's width, above 2 m
            self.assertAlmostEqual(g['y'], -4.0 - 0.03, places=2)
            self.assertLessEqual(abs(g['x']) + 0.5, 5.0 + 1e-6)
            self.assertGreaterEqual(g['z'], 2.0)
        for g in ground:
            self.assertTrue(walk.contains(Point(g['x'], g['y'])))

    def test_a_cable_sags_and_bunting_hangs_flags(self):
        mesh = roadmesh.Mesh()
        pts = mesh.sag_points((0, 0, 6), (10, 0, 6), 0.5, 4)
        self.assertAlmostEqual(pts[2][2], 5.5)
        mesh.bunting((0, 0, 5), (10, 0, 5), random.Random(1))
        flags = sum(len(p['idx']) for chunk in mesh.chunks.values() for name, p in chunk.items() if name.startswith('flag_'))
        self.assertGreater(flags // 6, 10)   # two triangles per flag, one every ~0.55 m

    def test_the_corner_lot_takes_the_corner_role(self):
        catalog = self.catalog()
        catalog['models']['corner'] = dict(catalog['models']['bld'], roles=['shop'])
        placed, _ = parcels.fill_block(box(0, 0, 40, 30), catalog, [('bld', 1.0)], random.Random(4), corner=[('corner', 1.0)])
        self.assertTrue(any(p['corner'] and p['asset'] == 'corner' for p in placed))
        self.assertTrue(all(p['asset'] == 'bld' for p in placed if not p['corner']))


class BodyHeightCollision(unittest.TestCase):
    """collision_shapes: what a model has at body height, seen from above."""

    def test_a_roof_overhead_does_not_collide_but_its_posts_do(self):
        sys.path.insert(0, str(HERE.parents[1] / 'tools' / 'zombies_convert'))
        import collision_shapes as cs
        roof = {'type': 'box', 'center': [0, 2.1, 0], 'size': [2.5, 0.4, 2.5], 'rot': [0, 0, 0, 1]}
        posts = [{'type': 'box', 'center': [sx * 1.2, 1.1, sz * 1.2], 'size': [0.1, 2.3, 0.1], 'rot': [0, 0, 0, 1]}
                 for sx in (-1, 1) for sz in (-1, 1)]
        polys, mesh = cs._collider_polygons([roof] + posts, cs.BAND)
        self.assertFalse(mesh)
        self.assertEqual(len(polys), 4)
        # placed at (10, 20), turned 90 degrees: model +X goes north, model +Z (level south) goes east
        placed = cs.place([[(1.0, 0.0), (1.0, 1.0)]], 10.0, 20.0, 90.0)[0]
        self.assertAlmostEqual(placed[0][0], 10.0, places=6)
        self.assertAlmostEqual(placed[0][1], 21.0, places=6)
        self.assertAlmostEqual(placed[1][0], 11.0, places=6)

    def test_a_slice_keeps_what_crosses_the_band(self):
        sys.path.insert(0, str(HERE.parents[1] / 'tools' / 'zombies_convert'))
        import collision_shapes as cs
        # a vertical quad (a post's face) from the floor to 3 m, and a flat roof at 2.5 m
        post = np.array([[[0, 0, 0], [0.2, 0, 0], [0.2, 3, 0]], [[0, 0, 0], [0.2, 3, 0], [0, 3, 0]]], float)
        roof = np.array([[[-2, 2.5, -2], [2, 2.5, -2], [2, 2.5, 2]]], float)
        polys = cs.slice_polygons(np.concatenate([post, roof]))
        self.assertEqual(len(polys), 1)
        xs = [x for x, _ in polys[0]]
        self.assertLess(max(xs) - min(xs), 0.5)


class OneBlockAgain(unittest.TestCase):
    def test_only_the_block_comes_from_the_new_run(self):
        import coastal
        square = [[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0]]
        old = {'entities': [
            {'id': 'house_01', 'asset': 'old_house', 'x': 5.0, 'y': 5.0, 'zone': 'block_01', 'tags': []},
            {'id': 'plant_01', 'asset': 'old_plant', 'x': 2.0, 'y': 2.0, 'zone': None, 'tags': []},
            {'id': 'lamp_01', 'asset': 'hand_placed_lamp', 'x': 20.0, 'y': 5.0, 'zone': None, 'tags': []},
            {'id': 'ground_0_0', 'asset': 'old_mesh', 'x': 0.0, 'y': 0.0, 'zone': None, 'tags': ['generated_mesh']}]}
        new = {'zones': {'block_01': {'poly': square}}, 'entities': [
            {'id': 'house_01', 'asset': 'new_house', 'x': 4.0, 'y': 6.0, 'zone': 'block_01', 'tags': []},
            {'id': 'car_01', 'asset': 'new_car', 'x': 12.0, 'y': 5.0, 'zone': 'block_01', 'tags': []},
            {'id': 'lamp_01', 'asset': 'moved_lamp', 'x': 25.0, 'y': 5.0, 'zone': None, 'tags': []},
            {'id': 'ground_0_0', 'asset': 'new_mesh', 'x': 0.0, 'y': 0.0, 'zone': None, 'tags': ['generated_mesh']}]}
        before, after = coastal.keep_outside(old, new, 'block_01')
        assets = sorted(e['asset'] for e in new['entities'])
        self.assertEqual((before, after), (2, 2))
        # the block's things are the new run's (by zone, even out on the street), the rest the old scene's, the meshes new
        self.assertEqual(assets, ['hand_placed_lamp', 'new_car', 'new_house', 'new_mesh'])
        self.assertEqual(len({e['id'] for e in new['entities']}), len(new['entities']))
