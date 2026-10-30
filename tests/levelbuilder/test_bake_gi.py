"""The baked ambient light (tools/zombies_convert/bake_gi.py): the GPU bake gives the CPU's light (skipped without a GPU).

    python -m unittest discover tests/levelbuilder
"""
import os
import random
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / 'tools' / 'zombies_convert'))

import numpy as np  # noqa: E402

import bake_gi  # noqa: E402


def gpu_available():
    try:
        import bake_gi_gpu  # noqa: F401
        return True
    except ImportError:
        return Path(bake_gi.GPU_PYTHON).is_file()


class BakeGiTest(unittest.TestCase):
    @unittest.skipUnless(gpu_available(), 'no CuPy and no PAKAL_GPU_PYTHON')
    def test_gpu_matches_cpu(self):
        rng = random.Random(7)
        boxes = []
        for _ in range(40):
            x, y = rng.uniform(0, 40), rng.uniform(0, 40)
            boxes.append((x, y, x + rng.uniform(3, 10), y + rng.uniform(3, 10), rng.uniform(3, 12)))
        args = dict(sun_travel=(0.62, 0.15, -0.77), sun_color=(1.35, 1.27, 1.12), sky_top=(0.3, 0.5, 0.86),
                    sky_horizon=(0.76, 0.86, 0.96), bounds=(0, 0, 40, 40), cell=2.0)
        before = os.environ.get('PAKAL_GI_GPU')
        try:
            os.environ['PAKAL_GI_GPU'] = '1'
            gpu, dims, _, _ = bake_gi.bake(boxes, **args)
            os.environ['PAKAL_GI_GPU'] = '0'
            cpu, cpu_dims, _, _ = bake_gi.bake(boxes, **args)
        finally:
            if before is None:
                os.environ.pop('PAKAL_GI_GPU', None)
            else:
                os.environ['PAKAL_GI_GPU'] = before
        self.assertEqual(dims, cpu_dims)
        self.assertLess(float(np.abs(gpu - cpu).max()), 1e-3)


if __name__ == '__main__':
    unittest.main()
