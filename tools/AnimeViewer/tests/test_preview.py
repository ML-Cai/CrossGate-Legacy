"""CPU selection/cache checks: no game files, NumPy, Pillow, Gradio or GPU."""
import gc
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import patch
import weakref
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import preview

class Value(int):
    def __new__(cls,value,name):
        obj=super().__new__(cls,value);obj.name=name;return obj
class Frames(list): pass
class Session:
    def __init__(self):
        self.keys=[(Value(2,"EAST"),Value(1,"WALK")),(Value(0,"NORTH"),Value(0,"IDLE"))]
        self.count=2;self.duration=101;self.width=2;self.height=3
        self.close_calls=self.decode_calls=0;self.error=None;self.last_output=None;self.missing=False
    def acquire_available_anime_serial_nums(self,version): return [100000,100001]
    def acquire_anime_resource(self,version,serial):
        if self.missing:return None
        return types.SimpleNamespace(keys=lambda:self.keys,acquire=lambda d,m:types.SimpleNamespace(motionGraphicsSerialNums=list(range(self.count)),duration=self.duration))
    def acquire_animation_pixels(self,version,serial,d,m,max_bytes):
        self.decode_calls+=1
        if self.error:raise self.error
        frames=Frames(range(self.count));self.last_output=weakref.ref(frames)
        return dict(width=self.width,height=self.height,frame_count=self.count,duration_ms=self.duration,offset_x=-1,offset_y=-2,rgba=frames)
    def close(self):self.close_calls+=1

class PreviewTests(unittest.TestCase):
    def setUp(self):
        self.encoder=patch.object(preview,"encode_frame",return_value=b"png").start()
        self.addCleanup(patch.stopall)
        self.session=Session();self.viewer=preview.Preview(self.session,version=0)
        self.addCleanup(self.viewer.close)
    def test_metadata_select_does_not_encode_or_preload_images(self):
        first=self.viewer.select("100000")
        self.assertEqual((first["motion"],first["direction"]),("0","0"))
        self.assertNotIn("rgba",first);self.assertNotIn("html",first);self.encoder.assert_not_called()
        walk=self.viewer.select("100001","1","0")
        self.assertEqual((walk["motion"],walk["direction"]),("1","2"));self.assertEqual(walk["directions"],[("EAST","2")])
        self.assertEqual(walk["duration_ms"],101)
    def test_same_selection_reuses_pixels_and_pngs(self):
        first=self.viewer.select("100000");same=self.viewer.select("100000")
        self.assertEqual(first["token"],same["token"]);self.assertTrue(same["cache_hit"])
        self.assertEqual(self.session.decode_calls,1)
        self.assertEqual(self.viewer.frame(first["token"],0),(b"png",False))
        self.assertEqual(self.viewer.frame(first["token"],0),(b"png",True));self.encoder.assert_called_once()
    def test_invalid_serial_description_and_frame_limits(self):
        with self.assertRaises(ValueError):self.viewer.select("999")
        self.session.missing=True
        with self.assertRaises(ValueError):self.viewer.select("100000")
        self.session.missing=False
        for count,duration in [(0,100),(257,100),(2,0)]:
            self.session.count,self.session.duration=count,duration
            with self.assertRaises(ValueError):self.viewer.select("100000")
        self.assertEqual(self.session.decode_calls,0)
    def test_old_clip_released_before_new_decode_failure_and_retry(self):
        first=self.viewer.select("100000");old=self.session.last_output
        self.session.error=RuntimeError("decode failed")
        with self.assertRaises(RuntimeError):self.viewer.select("100001")
        self.assertIsNone(old());self.assertIsNone(self.viewer.clip)
        with self.assertRaises(LookupError):self.viewer.frame(first["token"],0)
        self.session.error=None;self.assertEqual(self.viewer.select("100001")["frame_count"],2)
    def test_frame_bounds_canvas_and_encoded_budget(self):
        first=self.viewer.select("100000")
        for index in [-1,2]:
            with self.assertRaises(ValueError):self.viewer.frame(first["token"],index)
        with patch.object(preview,"PNG_CACHE_BYTES",1):
            with self.assertRaises(ValueError):self.viewer.frame(first["token"],0)
        self.assertEqual(self.viewer.png_bytes,0)
        self.session.width=2048;self.session.height=1024
        with self.assertRaises(ValueError):self.viewer.select("100001")
        self.assertIsNone(self.viewer.clip);self.assertIsNone(self.session.last_output())
    def test_png_cache_evicts_by_count_and_bytes_and_clears_on_switch(self):
        self.session.count=20;first=self.viewer.select("100000")
        for index in range(20):self.viewer.frame(first["token"],index)
        self.assertEqual(len(self.viewer.pngs),12)
        with patch.object(preview,"PNG_CACHE_BYTES",6):
            second=self.viewer.select("100001")
            for index in range(3):self.viewer.frame(second["token"],index)
            self.assertEqual(list(self.viewer.pngs),[1,2]);self.assertEqual(self.viewer.png_bytes,6)
        with self.assertRaises(LookupError):self.viewer.frame(first["token"],0)
    def test_late_selection_rejected_and_revision_map_bounded(self):
        newer=self.viewer.select("100001",client="tab",revision=2)
        with self.assertRaises(LookupError):self.viewer.select("100000",client="tab",revision=1)
        self.assertEqual(self.viewer.description["token"],newer["token"])
        for i in range(30):self.viewer.select("100001",client=str(i),revision=1)
        self.assertEqual(len(self.viewer.requests),16)
    def test_repeated_switches_hold_only_one_clip_and_close_frees_it(self):
        old=None
        for i in range(100):
            self.viewer.select(str(100000+i%2))
            if old:self.assertIsNone(old())
            old=self.session.last_output
        self.viewer.close();self.viewer.close();gc.collect()
        self.assertIsNone(old());self.assertEqual(self.session.close_calls,1);self.assertEqual(self.viewer.png_bytes,0)
        with self.assertRaises(RuntimeError):self.viewer.select("100000")
    def test_empty_catalog_and_invalid_payload_bounds(self):
        with self.assertRaises(ValueError):preview.Preview(self.session,0,version=0)
        with self.assertRaises(ValueError):preview.Preview(self.session,33*1024*1024,version=0)
        with patch.object(self.session,"acquire_available_anime_serial_nums",return_value=[]):
            with self.assertRaises(ValueError):preview.Preview(self.session,version=0)

if __name__=="__main__":unittest.main()
