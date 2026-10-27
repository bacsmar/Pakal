import importlib.util
from pathlib import Path
import struct
import unittest

path = Path(__file__).resolve().parents[1] / "tools/zombies_convert/extract_collision.py"
spec = importlib.util.spec_from_file_location("collision_extract", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def payload(kind=-1):
    mesh = b"zombiesdemo_col.bdae"
    return (b"prefix\x01\x01" + struct.pack(">H", len(mesh)) + mesh + struct.pack(">II", 1, 4)
            + struct.pack(">8fb", 0, 0, 100, 0, 100, 100, 0, 100, kind))


class CollisionExtractionTests(unittest.TestCase):
    def test_source_types_and_coordinates(self):
        for kind, layer in [(-1, "solid"), (0, "player"), (6, "water")]:
            result = module.extract(payload(kind))
            self.assertEqual(result["segments"][0]["layer"], layer)
            self.assertEqual(result["segments"][0]["points"][2], (100, 100))

    def test_ambiguous_payload_rejected(self):
        with self.assertRaises(ValueError):
            module.extract(payload() * 2)

    def test_unknown_type_rejected(self):
        with self.assertRaises(ValueError):
            module.extract(payload(99))

    def test_truncated_payload_rejected(self):
        with self.assertRaises(struct.error):
            module.extract(payload()[:-2])


if __name__ == "__main__":
    unittest.main()
