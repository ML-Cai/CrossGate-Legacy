"""Owned frame plans, native pixel equivalence and all version path variants."""
import argparse
import gc
from pathlib import Path
import struct
import sys
import unittest
import weakref

parser=argparse.ArgumentParser()
parser.add_argument("--module-dir",required=True)
args,remaining=parser.parse_known_args()
sys.path.insert(0,args.module_dir)
import test_session_cache as fixtures
cgl,np,V=fixtures.cgl,fixtures.np,fixtures.V


class AnimationStreamTests(unittest.TestCase):
    setUp=fixtures.SessionCacheTest.setUp
    tearDown=fixtures.SessionCacheTest.tearDown
    write=fixtures.SessionCacheTest.write
    def test_stream_matches_full_clip_and_plan_does_not_retain_pixels(self):
        plan=self.session.plan_animation(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
        self.assertEqual(plan.metadata["frame_count"],1)
        self.assertEqual(plan.metadata["frame_bytes"],16)
        frame=self.session.decode_animation_frame(plan,0)
        full=self.session.acquire_animation_pixels(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
        np.testing.assert_array_equal(frame,full["rgba"][0])
        owner=weakref.ref(frame);del frame;gc.collect();self.assertIsNone(owner())
        self.session.close()
        self.assertEqual(plan.metadata["frame_bytes"],16)

    def test_budget_bounds_epoch_and_session_identity(self):
        plan=self.session.plan_animation(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
        with self.assertRaises(RuntimeError):self.session.decode_animation_frame(plan,0,max_bytes=15)
        with self.assertRaises(IndexError):self.session.decode_animation_frame(plan,1)
        with cgl.Session(engine_root_path=str(self.root)) as other:
            with self.assertRaisesRegex(RuntimeError,"another Session"):other.decode_animation_frame(plan,0)
        self.session.invalidate_resources()
        with self.assertRaisesRegex(RuntimeError,"expired"):self.session.decode_animation_frame(plan,0)
        fresh=self.session.plan_animation(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
        self.assertEqual(self.session.decode_animation_frame(fresh,0).shape,(2,2,4))

    def test_all_puk_versions_use_their_native_paths_and_owned_plans(self):
        paths=[("GraphicInfoEx_5.bin","GraphicEx_5.bin","AnimeInfoEx_1.Bin","AnimeEx_1.Bin"),
            ("GraphicInfoV3_19.bin","GraphicV3_19.bin","AnimeInfoV3_8.bin","AnimeV3_8.bin"),
            ("Puk2/GraphicInfo_PUK2_2.bin","Puk2/Graphic_PUK2_2.bin","Puk2/AnimeInfo_PUK2_4.bin","Puk2/Anime_PUK2_4.bin"),
            ("Puk3/GraphicInfo_PUK3_1.bin","Puk3/Graphic_PUK3_1.bin","Puk3/AnimeInfo_PUK3_2.bin","Puk3/Anime_PUK3_2.bin"),
            ("GraphicInfo_Joy_125.bin","Graphic_Joy_125.bin","AnimeInfo_Joy_91.bin","Anime_Joy_91.bin"),
            ("GraphicInfo_Joy_CH1.bin","Graphic_Joy_CH1.bin","AnimeInfo_Joy_CH1.Bin","Anime_Joy_CH1.bin"),
            ("GraphicInfo_Joy_EX_9.bin","Graphic_Joy_EX_9.bin","AnimeInfo_Joy_EX_9.bin","Anime_Joy_EX_9.bin")]
        source=[(self.root/"bin"/name).read_bytes() for name in ["GraphicInfo_66.bin","Graphic_66.bin","AnimeInfo_4.bin","Anime_4.bin"]]
        for version, names in zip(cgl.CrossGateVersionList()[1:],paths):
            contents=list(source)
            if int(version)>=2:
                contents[2]=struct.pack("<IIHH",100000,0,1,1)
                contents[3]=source[3][:12]+struct.pack("<II",0x00040033,0xffffffff)+source[3][12:]
            for name,data in zip(names,contents):
                path=self.root/"bin"/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
            plan=self.session.plan_animation(version,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
            frame=self.session.decode_animation_frame(plan,0)
            self.assertEqual(frame.shape,(2,2,4))
            desc=self.session.acquire_anime_resource(version,100000).acquire(cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
            self.assertEqual(list(desc.motionGraphicsSerialNums),[100])
            self.assertEqual(desc.extended_flags,0x00040033 if int(version)>=2 else 0)
            self.session.invalidate_resources()

    def test_hidden_palette_and_short_palette_safety(self):
        version=cgl.CrossGateVersion.CG_VERSION_PUK2_V1
        for size in [768,3]:
            self.session.invalidate_resources()
            palette=bytearray(size)
            if size==768:palette[18*3:18*3+3]=bytes([7,8,9])
            marker=struct.pack("<2sBBiiII",b"RD",2,0,4,1,24+size,size)+bytes(4)+palette
            self.write("GraphicInfoV3_19.bin",self.index+struct.pack("<IIIiiiiBB6sI",101,len(self.graphics),len(marker),0,0,4,1,1,1,bytes(6),100000))
            self.write("GraphicV3_19.bin",self.graphics+marker)
            self.write("AnimeInfoV3_8.bin",struct.pack("<IIHH",100000,0,1,1))
            self.write("AnimeV3_8.bin",struct.pack("<HHIIII",0,0,101,1,0,0xffffffff)+struct.pack("<I6s",100,bytes(6)))
            plan=self.session.plan_animation(version,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
            if size==768:
                np.testing.assert_array_equal(self.session.decode_animation_frame(plan,0)[0,0],[9,8,7,255])
            else:
                with self.assertRaisesRegex(RuntimeError,"too short"):self.session.decode_animation_frame(plan,0)

    def test_extended_header_truncation_and_sentinel_failure_recover(self):
        data=struct.pack("<HHIIII",0,0,101,1,0,0xffffffff)+struct.pack("<I6s",100,bytes(6))
        self.write("AnimeInfo_4.bin",struct.pack("<IIHH",100000,0,1,1))
        for bad in [data[:n] for n in range(len(data))]+[data[:16]+bytes(4)+data[20:]]:
            self.session.invalidate_resources();self.write("Anime_4.bin",bad)
            with self.assertRaises(RuntimeError):self.session.plan_animation(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD)
        self.session.invalidate_resources();self.write("Anime_4.bin",data)
        self.assertEqual(self.session.plan_animation(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD).metadata["frame_count"],1)
        # Standard headers must not mistake signed frame payload bytes for the extension sentinel.
        self.session.invalidate_resources()
        self.write("AnimeInfo_4.bin",struct.pack("<IIHH",100000,0,1,64))
        self.write("Anime_4.bin",struct.pack("<HHII",0,0,101,1)+struct.pack("<I6s",100,bytes([255])*6))
        self.assertEqual(self.session.plan_animation(V,100000,cgl.DirectionTypes.DIRECTION_NORTH,cgl.MotionTypes.STANDARD).metadata["frame_count"],1)


if __name__=="__main__":
    unittest.main(argv=[sys.argv[0],*remaining])
