"""Acceptance report checks against tiny real RGBA/PNG resources; no native/GPU."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from preview import Preview
from verify_directions import verify,memory_summary
from test_direction_preview import GroupSession

class Pixels(GroupSession):
    def decode_animation_frame(self,plan,index,max_bytes):
        self.frame_calls+=1
        pixels=np.zeros((self.height,self.width,4),dtype=np.uint8)
        pixels[0,0]=[10,20,30,255]
        return pixels
    def cache_stats(self):return {"readers":[] if self.close_calls else ["active"]}

class VerifyDirectionTests(unittest.TestCase):
    def setup_preview(self):
        session=Pixels();preview=Preview(session,version=0);preview.version_options={0:0,1:1}
        return session,preview
    def test_matrix_compares_pixels_switches_and_releases_plans(self):
        session,preview=self.setup_preview()
        report=verify(preview,[(0,"Classic"),(1,"PUK1")],cycles=2)
        self.assertEqual(report["status"],"passed")
        self.assertEqual(report["png_frames"],32)
        self.assertEqual(report["closed_readers"],0)
        self.assertEqual(report["browser_visual"],"not_run")
        self.assertIn("excludes Session/Preview construction",report["timing_scope"])
        for row in report["versions"]:
            self.assertGreaterEqual(row["catalog_http_ms"],0)
            for sample in row["samples"]:
                self.assertGreaterEqual(sample["group_http_ms"],0)
                self.assertGreaterEqual(sample["first_png_http_ms"],0)
                self.assertGreaterEqual(sample["selection_to_first_png_ms"],sample["group_http_ms"])
                self.assertGreaterEqual(sample["selection_to_first_png_ms"],sample["first_png_http_ms"])
                self.assertEqual(sample["first_png_cache"],"miss")
                self.assertEqual(sample["first_direction"],"0")
                self.assertEqual(sample["directions"][0]["offset_x"],-1)
                self.assertEqual(len(sample["directions"]),8)
        self.assertEqual(session.close_calls,1)
    def test_missing_requested_version_is_failed_not_silently_skipped(self):
        session,preview=self.setup_preview();session.bad_version=1
        report=verify(preview,[(0,"Classic"),(1,"PUK1")],cycles=2)
        self.assertEqual(report["status"],"failed")
        self.assertEqual(report["versions"][1]["status"],"failed")
        self.assertEqual(report["versions"][1]["samples"],[])
        self.assertNotIn("catalog_http_ms",report["versions"][1])
        self.assertEqual(session.close_calls,1)
    def test_corrupt_png_rejected_and_failure_closes_session(self):
        session,preview=self.setup_preview()
        with patch("preview.encode_frame",return_value=b"bad PNG"):
            report=verify(preview,[(0,"Classic")],cycles=1)
        self.assertEqual(report["status"],"failed")
        self.assertEqual(session.close_calls,1)
    def test_invalid_cycle_count_closes_without_starting_server(self):
        session,preview=self.setup_preview()
        with self.assertRaises(ValueError):verify(preview,[(0,"Classic")],cycles=0)
        self.assertEqual(session.close_calls,1)
    def test_memory_summary_requires_two_comparable_windows_after_warmup(self):
        self.assertIsNone(memory_summary([],2)["rss_growth_bytes"])
        self.assertIsNone(memory_summary([100]*7,2)["rss_growth_bytes"])
        self.assertEqual(memory_summary([999]*4+[100,100,120,120],2)["rss_growth_bytes"],20)

if __name__=="__main__":unittest.main()
