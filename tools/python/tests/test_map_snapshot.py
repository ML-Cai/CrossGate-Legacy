"""CPU-only real-binding MAP snapshots, bounds and independent ownership."""
import argparse
import gc
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import weakref

parser = argparse.ArgumentParser()
parser.add_argument("--module-dir", required=True)
args, remaining = parser.parse_known_args()
sys.path.insert(0, args.module_dir)
import cgl
import numpy as np


class MapSnapshotTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="cgl-map-snapshot-")
        self.root = Path(self.directory.name)
        self.path = self.root / "rectangular.dat"
        self.write()
        self.session = cgl.Session(engine_root_path=str(self.root))

    def tearDown(self):
        self.session.close()
        self.directory.cleanup()  # Windows also checks that no file handle survived.

    def write(self, width=2, height=3, bias=0):
        count = width * height
        header = b"MAP" + bytes(9) + struct.pack("<II", width, height)
        values = [(0xc000 + bias + i) % 65536 for i in range(count * 3)]
        self.path.write_bytes(header + struct.pack("<" + "H" * len(values), *values))

    def test_rectangular_planes_raw_flags_row_major_and_subregion(self):
        snapshot = self.session.load_map("rectangular.dat")
        self.assertEqual((snapshot.width, snapshot.height), (2, 3))
        self.assertEqual(snapshot.cell(1, 2), dict(ground_id=0xc005, object_id=0xc00b, flags=0xc011))
        region = snapshot.region(1, 1, 1, 2)
        self.assertEqual(region.dtype, np.uint16)
        self.assertEqual(region.shape, (2, 1, 3))
        np.testing.assert_array_equal(region, [[[0xc003, 0xc009, 0xc00f]], [[0xc005, 0xc00b, 0xc011]]])
        self.assertTrue(region.flags.owndata)
        self.assertEqual(self.session.cache_stats()["readers"], [])
        with self.assertRaises(AttributeError):
            snapshot.width = 9

    def test_snapshots_and_copies_survive_close_without_mutating_each_other(self):
        snapshot = self.session.load_map(str(self.path))
        region = snapshot.region(0, 0, 2, 3)
        region[0, 0] = 0
        value = snapshot.cell(0, 0)
        value["ground_id"] = 0
        self.assertEqual(snapshot.cell(0, 0)["ground_id"], 0xc000)
        self.session.trim_cache()
        self.session.invalidate_resources()
        self.session.close()
        self.path.unlink()
        self.assertEqual(snapshot.cell(0, 0)["ground_id"], 0xc000)
        del snapshot
        gc.collect()
        self.assertEqual(int(region[2, 1, 2]), 0xc011)
        with self.assertRaisesRegex(RuntimeError, "closed"):
            self.session.load_map(str(self.path))

    def test_reload_has_no_session_map_cache_or_old_snapshot_mutation(self):
        old = self.session.load_map("rectangular.dat")
        self.write(bias=9)
        fresh = self.session.load_map("rectangular.dat")
        self.assertEqual(old.cell(0, 0)["ground_id"], 0xc000)
        self.assertEqual(fresh.cell(0, 0)["ground_id"], 0xc009)

    def test_negative_zero_outside_and_extreme_regions_rejected(self):
        snapshot = self.session.load_map("rectangular.dat")
        for x, y in [(-1, 0), (0, -1), (2, 0), (0, 3), (2**63 - 1, 0)]:
            with self.assertRaises(IndexError):
                snapshot.cell(x, y)
        for region in [(0, 0, 0, 1), (0, 0, 1, -1), (1, 2, 2, 1), (0, 0, 2**63-1, 2**63-1)]:
            with self.assertRaises(IndexError):
                snapshot.region(*region)

    def test_region_budget_exact_boundary_and_excess(self):
        self.write(width=257, height=256)
        snapshot = self.session.load_map("rectangular.dat")
        self.assertEqual(snapshot.region(0, 0, 256, 256).nbytes, 65536 * 6)
        with self.assertRaisesRegex(ValueError, "65536"):
            snapshot.region(0, 0, 257, 256)

    def test_missing_malformed_oversized_and_failure_recovery(self):
        valid = self.path.read_bytes()
        for contents in [b"", valid[:-1], valid + b"x", b"BAD" + valid[3:],
                         b"MAP" + bytes(9) + struct.pack("<II", 65536, 65536)]:
            self.path.write_bytes(contents)
            with self.assertRaises(ValueError):
                self.session.load_map("rectangular.dat")
        for path in ["missing.dat", ""]:
            with self.assertRaises(ValueError):
                self.session.load_map(path)
        self.path.write_bytes(valid)
        self.assertEqual(self.session.load_map("rectangular.dat").height, 3)

    def test_repeated_loads_release_snapshots_and_region_buffers(self):
        for _ in range(100):
            snapshot = self.session.load_map("rectangular.dat")
            region = snapshot.region(0, 0, 2, 3)
            owner = weakref.ref(snapshot)
            pixels = weakref.ref(region)
            del snapshot, region
            gc.collect()
            self.assertIsNone(owner())
            self.assertIsNone(pixels())
        self.assertEqual(self.session.cache_stats()["readers"], [])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *remaining])
