"""All-direction plans/version isolation without native code or full-clip pixels."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import weakref
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from preview import Preview
from test_preview import Session,Value,Frames

class Plan:
    def __init__(self,metadata):self.metadata=metadata

class GroupSession(Session):
    def __init__(self):
        super().__init__()
        self.keys=[(Value(i,"DIRECTION_"+str(i)),Value(0,"IDLE")) for i in range(8)]
        self.plan_calls=self.frame_calls=self.invalidations=0
        self.bad_version=self.bad_direction=None
        self.plan_refs=[];self.frame_ref=None
    def acquire_animation_pixels(self,*args,**kwargs):raise AssertionError("Whole-clip decode is forbidden")
    def acquire_available_anime_serial_nums(self,version):
        if version==self.bad_version:raise RuntimeError("Version files are missing")
        return [100000,100001]
    def invalidate_resources(self):self.invalidations+=1
    def plan_animation(self,version,serial,d,m):
        if int(d)==self.bad_direction:raise RuntimeError("Direction data missing")
        self.plan_calls+=1
        result=Plan(dict(width=self.width,height=self.height,frame_count=self.count,duration_ms=self.duration,
            offset_x=-1,offset_y=-2,clip_bytes=self.width*self.height*4*self.count,frame_bytes=self.width*self.height*4))
        self.plan_refs.append(weakref.ref(result));return result
    def decode_animation_frame(self,plan,index,max_bytes):
        self.frame_calls+=1;frame=Frames([index]);self.frame_ref=weakref.ref(frame);return frame

class DirectionTests(unittest.TestCase):
    def setUp(self):
        self.session=GroupSession();self.preview=Preview(self.session,version=0)
        self.preview.version_options={0:0,1:1,2:2}
        self.encoder=patch("preview.encode_frame",return_value=b"png").start()
        self.addCleanup(patch.stopall);self.addCleanup(self.preview.close)

    def test_eight_plans_no_pixels_same_selection_reuse_and_no_rgba_owner(self):
        self.session.count=16;self.session.width=self.session.height=512
        group=self.preview.select_group(100000)
        self.assertEqual(len(group["directions"]),8);self.assertEqual(self.session.frame_calls,0)
        self.assertGreater(group["directions"][0]["clip_bytes"],8*1024*1024)
        self.assertTrue(self.preview.select_group(100000)["cache_hit"])
        self.assertEqual(self.session.plan_calls,8)
        token=group["token"]
        self.assertEqual(self.preview.group_frame(token,"0",0),(b"png",False))
        self.assertEqual(self.preview.group_frame(token,"0",0),(b"png",True))
        self.assertEqual(self.session.frame_calls,1)
        self.encoder.reset_mock()  # Mock call history itself otherwise retains the transient input.
        self.assertIsNone(self.session.frame_ref())

    def test_version_switch_expires_tokens_plans_and_failed_version_recovers(self):
        old=self.preview.select_group(100000,version=0)
        refs=list(self.session.plan_refs)
        self.preview.catalog(1,client="tab",revision=2)
        self.assertEqual(self.session.invalidations,1)
        self.assertTrue(all(ref() is None for ref in refs))
        with self.assertRaises(LookupError):self.preview.group_frame(old["token"],"0",0)
        self.session.bad_version=2
        with self.assertRaisesRegex(RuntimeError,"missing"):self.preview.catalog(2)
        self.assertEqual(self.preview.serials,())
        self.assertEqual(self.preview.catalog(0)["version"],"0")
        with self.assertRaises(ValueError):self.preview.catalog(99)

    def test_partial_direction_error_and_per_frame_limit(self):
        self.session.bad_direction=3
        group=self.preview.select_group(100000)
        self.assertIn("error",group["directions"][3])
        self.assertNotIn("error",group["directions"][0])
        with self.assertRaises(ValueError):self.preview.group_frame(group["token"],"3",0)
        self.session.width=2048;self.session.height=1024
        group=self.preview.select_group(100001)
        self.assertTrue(all("error" in d for d in group["directions"]))

    def test_stale_revision_png_bounds_and_close_release(self):
        self.session.count=40
        group=self.preview.select_group(100000,client="tab",revision=5)
        for i in range(40):self.preview.group_frame(group["token"],"0",i)
        self.assertEqual(len(self.preview.pngs),24)
        with self.assertRaises(LookupError):self.preview.catalog(1,client="tab",revision=4)
        for index in [-1,40]:
            with self.assertRaises(ValueError):self.preview.group_frame(group["token"],"0",index)
        refs=list(self.session.plan_refs);self.preview.close()
        self.assertTrue(all(ref() is None for ref in refs));self.assertEqual(self.preview.png_bytes,0)

if __name__=="__main__":unittest.main()
