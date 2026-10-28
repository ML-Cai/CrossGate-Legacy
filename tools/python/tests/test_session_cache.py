"""Synthetic, CPU-only binding tests. Requires the built cgl module and NumPy."""
import argparse
import gc
import json
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

V = cgl.CrossGateVersion.CG_VERSION_Classic
P = cgl.EnvironmentPaletteTypes.Daytime

class SessionCacheTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cgl-session-")
        self.root = Path(self.temp.name)
        (self.root / "bin/pal").mkdir(parents=True)
        self.index = struct.pack("<IIIiiiiBB6sI", 100, 0, 20, 0, 0, 2, 2, 1, 1, bytes(6), 0)
        self.graphics = struct.pack("<2sBBiiI", b"RD", 0, 0, 2, 2, 20) + bytes([16,17,18,0])
        self.write("GraphicInfo_66.bin", self.index)
        self.write("Graphic_66.bin", self.graphics)
        self.write("AnimeInfo_4.bin", struct.pack("<IIHH", 100000, 0, 1, 0))
        self.write("Anime_4.bin", struct.pack("<HHII", 0, 0, 100, 1) + struct.pack("<I6s",100,bytes(6)))
        self.palette = bytearray(708)
        self.palette[:9] = bytes([10,20,30,40,50,60,70,80,90])
        self.write("pal/palet_00.cgp", self.palette)
        self.session = cgl.Session(engine_root_path=str(self.root))

    def write(self, name, data):
        (self.root / "bin" / name).write_bytes(data)

    def tearDown(self):
        self.session.close()
        # Windows deletion also checks reader handle closure.
        self.temp.cleanup()

    def load(self):
        graphics = self.session.acquire_graphics_resource(V, 100)
        anime = self.session.acquire_anime_resource(V, 100000)
        palette = self.session.acquire_env_palette(P)
        self.assertIsNotNone(graphics)
        self.assertIsNotNone(anime)
        self.assertIsNotNone(palette)
        return graphics, anime, palette

    def test_stats_hot_reads_snapshot_and_budget(self):
        initial = self.session.cache_stats()
        self.assertEqual(initial["readers"], [])
        self.load()
        cold = self.session.cache_stats()
        self.load()
        hot = self.session.cache_stats()
        self.assertEqual(hot["totals"]["file_read_calls"], cold["totals"]["file_read_calls"])
        self.assertEqual(len(hot["readers"]), 2)
        self.assertGreater(sum(r["hits"] for r in hot["readers"]), sum(r["hits"] for r in cold["readers"]))
        json.dumps(hot)
        hot["readers"][0]["resident_bytes"] = -1
        self.assertGreaterEqual(self.session.cache_stats()["readers"][0]["resident_bytes"], 0)
        self.assertEqual(initial["readers"], [])
        self.session.set_cache_budget(0)
        self.assertEqual(self.session.cache_stats()["totals"]["resident_bytes"], 0)
        self.session.reset_cache_stats()
        self.load()
        zero = self.session.cache_stats()
        self.assertGreater(zero["totals"]["file_read_calls"], 0)
        self.assertTrue(all(r["resident_bytes"] == 0 and r["bypass_reads"] > 0 for r in zero["readers"]))

    def test_large_budget_totals_do_not_wrap(self):
        maximum = (1 << (struct.calcsize("P") * 8)) - 1
        self.session.set_cache_budget(maximum)
        self.load()
        self.assertEqual(self.session.cache_stats()["totals"]["budget_bytes"], 2 * maximum)

    def test_array_shape_follows_decoded_pixels_not_mismatched_index(self):
        self.write("Graphic_66.bin", struct.pack("<2sBBiiI", b"RD", 0, 0, 1, 2, 18) + bytes([16,17]))
        graphics = self.session.acquire_graphics_resource(V, 100)
        palette = self.session.acquire_env_palette(P)
        image = self.session.apply_palette(graphics, palette)
        self.assertEqual(image.shape, (2,1,4))
        np.testing.assert_array_equal(image[1,0], [40,50,60,255])

    def test_trim_invalidation_and_returned_ownership(self):
        graphics, anime, palette = self.load()
        array = self.session.apply_palette(graphics, palette)
        expected = array.copy()
        self.assertEqual(array.shape, (2,2,4))
        np.testing.assert_array_equal(array[0,0], [10,20,30,255])
        self.session.trim_cache()
        trimmed = self.session.cache_stats()
        self.assertEqual(trimmed["totals"]["resident_bytes"], 0)
        self.assertEqual(trimmed["totals"]["scratch_bytes"], 0)
        self.assertEqual(trimmed["graphics_index_readers"], 1)
        self.session.invalidate_resources()
        invalid = self.session.cache_stats()
        self.assertEqual(invalid["readers"], [])
        self.assertEqual(invalid["graphics_index_readers"], 0)
        self.assertEqual(invalid["anime_index_readers"], 0)
        self.assertFalse(invalid["palette_reader_active"])
        self.assertEqual(len(anime.keys()), 1)
        self.session.close()
        np.testing.assert_array_equal(self.session.apply_palette(graphics, palette), expected)
        del graphics, anime, palette
        gc.collect()
        np.testing.assert_array_equal(array, expected)
        view = array[0]
        ref = weakref.ref(array)
        del array
        gc.collect()
        np.testing.assert_array_equal(view, expected[0])
        del view
        gc.collect()
        self.assertIsNone(ref())

    def test_palette_and_graphic_edits_reload_explicitly(self):
        graphics, _, palette = self.load()
        self.palette[:3] = bytes([99,88,77])
        self.write("pal/palet_00.cgp", self.palette)
        cached = self.session.acquire_env_palette(P)
        self.assertEqual(self.session.apply_palette(graphics, cached)[0,0,0], 10)
        self.session.clear_palette_cache()
        refreshed = self.session.acquire_env_palette(P)
        self.assertEqual(self.session.apply_palette(graphics, refreshed)[0,0,0], 99)
        self.session.invalidate_resources()
        self.write("Graphic_66.bin", self.graphics[:16] + bytes([17,17,18,0]))
        new = self.session.acquire_graphics_resource(V, 100)
        self.assertEqual(self.session.apply_palette(new, refreshed)[0,0,0], 40)
        self.assertEqual(self.session.apply_palette(graphics, palette)[0,0,0], 10)

    def test_failed_load_retry_does_not_publish_failed_reader(self):
        (self.root / "bin/GraphicInfo_66.bin").unlink()
        with self.assertRaises(ValueError): self.session.acquire_graphics_data_count(V)
        self.assertEqual(self.session.cache_stats()["graphics_index_readers"], 0)
        self.write("GraphicInfo_66.bin", self.index)
        self.write("Graphic_66.bin", b"XX!")
        with self.assertRaises(ValueError): self.session.acquire_graphics_resource(V, 100)
        self.assertEqual(self.session.cache_stats()["readers"], [])
        self.write("Graphic_66.bin", self.graphics)
        self.assertIsNotNone(self.session.acquire_graphics_resource(V, 100))

    def test_close_context_and_invalid_arguments(self):
        with cgl.Session(str(self.root), 0) as session:
            graphics = session.acquire_graphics_resource(V, 100)
            palette = session.acquire_env_palette(P)
            self.assertEqual(session.cache_stats()["totals"]["resident_bytes"], 0)
            with self.assertRaises(ValueError): session.apply_palette(None, palette)
            with self.assertRaises(ValueError): session.apply_palette(graphics, None)
            with self.assertRaises((TypeError, OverflowError)): session.set_cache_budget(-1)
        self.assertTrue(session.cache_stats()["closed"])
        session.close(); session.trim_cache(); session.invalidate_resources()
        with self.assertRaises(RuntimeError): session.acquire_graphics_data_count(V)
        with self.assertRaises(RuntimeError): session.acquire_env_palette(P)
        with self.assertRaises(RuntimeError): session.set_cache_budget(4096)
        with self.assertRaises(ValueError):
            with cgl.Session(str(self.root)) as failing:
                raise ValueError("must not suppress")
        self.assertTrue(failing.cache_stats()["closed"])

    def test_repeated_invalidation_releases_readers_and_preserves_budget(self):
        self.session.set_cache_budget(4096)
        for _ in range(100):
            graphics, anime, palette = self.load()
            array = self.session.apply_palette(graphics, palette)
            self.session.invalidate_resources()
            self.assertEqual(self.session.cache_stats()["readers"], [])
            self.assertEqual(array[0,0,0], 10)
            del graphics, anime, palette, array
        self.load()
        self.assertTrue(all(r["budget_bytes"] == 4096 for r in self.session.cache_stats()["readers"]))

    def test_animation_catalog_is_sorted_unique_and_reloads(self):
        record = lambda serial: struct.pack("<IIHH", serial, 0, 1, 0)
        self.write("AnimeInfo_4.bin", record(100001) + record(100000) + record(100001))
        self.assertEqual(self.session.acquire_available_anime_serial_nums(V), [100000, 100001])
        self.write("AnimeInfo_4.bin", record(100002))
        self.session.invalidate_resources()
        self.assertEqual(self.session.acquire_available_anime_serial_nums(V), [100002])
        self.session.close()
        with self.assertRaises(RuntimeError): self.session.acquire_available_anime_serial_nums(V)

    def animation(self, **kwargs):
        return self.session.acquire_animation_pixels(V, 100000, cgl.DirectionTypes.DIRECTION_NORTH,
                                                     cgl.MotionTypes.STANDARD, **kwargs)

    def test_animation_rgba_alignment_and_exact_duration(self):
        second = struct.pack("<IIIiiiiBB6sI", 101, 20, 20, 1, 0, 2, 2, 1, 1, bytes(6), 0)
        first = struct.pack("<IIIiiiiBB6sI", 100, 0, 20, -1, -2, 2, 2, 1, 1, bytes(6), 0)
        self.write("GraphicInfo_66.bin", first + second)
        self.write("Graphic_66.bin", self.graphics * 2)
        self.write("Anime_4.bin", struct.pack("<HHII", 0, 0, 101, 2) +
                   struct.pack("<I6s",100,bytes(6)) + struct.pack("<I6s",101,bytes(6)))
        clip = self.animation(max_bytes=128)
        self.assertEqual({k:clip[k] for k in clip if k != "rgba"},
                         dict(width=4,height=4,frame_count=2,duration_ms=101,offset_x=-1,offset_y=-2))
        rgba = clip["rgba"]
        self.assertEqual(rgba.shape, (2,4,4,4)); self.assertEqual(rgba.dtype, np.uint8)
        self.assertTrue(rgba.flags.c_contiguous)
        np.testing.assert_array_equal(rgba[0,0,0], [90,80,70,255])
        self.assertEqual(rgba[0,0,1,3], 0)  # Index-zero source pixel.
        self.assertEqual(rgba[0,0,2,3], 0)  # Transparent canvas padding.
        np.testing.assert_array_equal(rgba[1,2,2], [90,80,70,255])

    def test_animation_budget_errors_and_retry_preserve_existing_output(self):
        held = self.animation(); expected = held["rgba"].copy()
        for limit in (0, 268435457):
            with self.assertRaises(ValueError): self.animation(max_bytes=limit)
        with self.assertRaises((TypeError, OverflowError)): self.animation(max_bytes=-1)
        self.session.invalidate_resources()
        with self.assertRaises(RuntimeError): self.animation(max_bytes=15)
        # Opening a graphics reader may read its file header. Once open, repeated
        # rejected admission must not read frame payloads or warm their cache.
        self.session.reset_cache_stats()
        with self.assertRaises(RuntimeError): self.animation(max_bytes=15)
        stats = self.session.cache_stats()
        self.assertTrue(all(r["file_read_calls"] == 0 for r in stats["readers"] if r["kind"] == "graphics"))
        with self.assertRaises(ValueError):
            self.session.acquire_animation_pixels(V,100000,cgl.DirectionTypes.UNKNOWN,cgl.MotionTypes.STANDARD)
        with self.assertRaises(RuntimeError):
            self.session.acquire_animation_pixels(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.WALK)
        self.assertEqual(self.animation()["rgba"].shape, (1,2,2,4))
        np.testing.assert_array_equal(held["rgba"], expected)

    def test_animation_output_and_views_outlive_close_and_release(self):
        clip = self.animation(); array = clip["rgba"]; expected = array.copy()
        self.session.trim_cache(); self.session.invalidate_resources(); self.session.close()
        np.testing.assert_array_equal(array, expected)
        with self.assertRaises(RuntimeError): self.animation()
        view = array[0]; ref = weakref.ref(array)
        del clip, array; gc.collect()
        self.assertIsNotNone(ref()); np.testing.assert_array_equal(view, expected[0])
        del view; gc.collect(); self.assertIsNone(ref())

    def test_animation_repeated_decode_invalidate_does_not_retain_outputs(self):
        for _ in range(100):
            clip = self.animation(); ref = weakref.ref(clip["rgba"])
            self.session.invalidate_resources()
            self.assertEqual(self.session.cache_stats()["readers"], [])
            del clip
            self.assertIsNone(ref())
        self.assertEqual(self.animation()["duration_ms"], 100)

if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + remaining)
