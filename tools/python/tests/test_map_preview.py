"""Real shared MapBuilder, projection, draw ordering and owned page output."""
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


class MapPreviewTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="cgl-map-view-")
        self.root = Path(self.directory.name)
        (self.root/"bin/pal").mkdir(parents=True)
        (self.root/"fixture.dat").write_bytes(b"MAP"+bytes(9)+struct.pack("<IIHHH", 1, 1, 5000, 5001, 0xc000))
        entries = b"".join(struct.pack("<IIIiiiiBB6sI", serial, 0, 20, -1, -2, 2, 2, 1, 1, bytes(6), image)
                           for serial, image in [(100, 5000), (101, 5001)])
        (self.root/"bin/GraphicInfo_66.bin").write_bytes(entries)
        (self.root/"bin/Graphic_66.bin").write_bytes(struct.pack("<2sBBiiI", b"RD", 0, 0, 2, 2, 20)+bytes([16,17,18,0]))
        palette = bytearray(708)
        palette[:9] = bytes([10,20,30,40,50,60,70,80,90])
        (self.root/"bin/pal/palet_00.cgp").write_bytes(palette)
        self.session = cgl.Session(engine_root_path=str(self.root))

    def tearDown(self):
        self.session.close()
        self.directory.cleanup()

    def test_draw_order_layers_projection_offsets_and_page_color(self):
        view = self.session.load_map_preview("fixture.dat")
        self.assertEqual((view.width, view.height, view.page_count), (1,1,1))
        draws = view.draw_list(0,0,64,64)
        self.assertEqual(len(draws),2)
        self.assertEqual((draws[0]["x"], draws[0]["y"]), (31,30))
        self.assertEqual(len(view.draw_list(0,0,64,64,1,True,False)),1)
        self.assertEqual(len(view.draw_list(0,0,64,64,1,False,True)),1)
        self.assertEqual(view.draw_list(0,0,64,64,1,False,False),[])
        page = self.session.acquire_map_page(view,0)
        x,y,w,h = draws[0]["source"]
        self.assertEqual((w,h),(2,2))
        np.testing.assert_array_equal(page[y:y+2,x:x+2], [[[90,80,70,255],[0,0,0,0]],[[30,20,10,255],[60,50,40,255]]])

    def test_input_budget_finite_values_and_page_bounds(self):
        view = self.session.load_map_preview("fixture.dat")
        for args in [(float("nan"),0,64,64,1),(0,0,1281,64,1),(0,0,64,961,1),(0,0,64,64,.1),(1,0,64,64,1)]:
            with self.assertRaises(ValueError):view.draw_list(*args)
        with self.assertRaises(IndexError):self.session.acquire_map_page(view,1)
        with self.assertRaises(ValueError):view.pan(0,0,0,float("inf"),1)
        self.assertEqual(view.pan(0,0,64,47,1),(0,0))

    def test_preview_and_page_lifetimes_and_repeated_release(self):
        for _ in range(20):
            view = self.session.load_map_preview("fixture.dat")
            page = self.session.acquire_map_page(view,0)
            ref = weakref.ref(view)
            pixels = weakref.ref(page)
            del view, page
            gc.collect()
            self.assertIsNone(ref());self.assertIsNone(pixels())
        view = self.session.load_map_preview("fixture.dat")
        page = self.session.acquire_map_page(view,0)
        self.session.close()
        self.assertTrue(view.draw_list(0,0,64,64))
        self.assertEqual(page.shape[2],4)
        with self.assertRaisesRegex(RuntimeError,"closed"):self.session.acquire_map_page(view,0)

    def test_pan_uses_classic_projection_and_scale(self):
        (self.root/"fixture.dat").write_bytes(b"MAP"+bytes(9)+struct.pack("<II",10,10)+bytes(100*6))
        view=self.session.load_map_preview("fixture.dat")
        self.assertEqual(view.pan(5,5,64,0,1),(4,4))
        self.assertEqual(view.pan(5,5,0,47,1),(6,4))
        self.assertEqual(view.pan(5,5,64,0,2),(4.5,4.5))

    def test_objects_follow_native_anchor_order_before_offsets(self):
        (self.root/"fixture.dat").write_bytes(b"MAP"+bytes(9)+struct.pack("<II",2,2)+
            struct.pack("<4H",0,0,0,0)+struct.pack("<4H",5001,5001,5001,5001)+bytes(8))
        view=self.session.load_map_preview("fixture.dat")
        draws=view.draw_list(.5,.5,128,128)
        self.assertEqual([(d["x"],d["y"]) for d in draws],[(63,38.5),(31,62),(95,62),(63,85.5)])

    def test_missing_images_and_bad_map_fail_without_poisoning_session(self):
        with self.assertRaises(ValueError):self.session.load_map_preview("missing.dat")
        (self.root/"fixture.dat").write_bytes(b"MAP"+bytes(9)+struct.pack("<IIHHH",1,1,9999,0,0))
        with self.assertRaises(RuntimeError):self.session.load_map_preview("fixture.dat")
        (self.root/"fixture.dat").write_bytes(b"MAP"+bytes(9)+struct.pack("<IIHHH",1,1,5000,0,0))
        self.assertEqual(self.session.load_map_preview("fixture.dat").page_count,1)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *remaining])
