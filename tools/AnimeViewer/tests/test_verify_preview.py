"""Exercise the acceptance command with real HTTP/PNG and tiny owned CPU arrays."""
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import numpy as np
from PIL import UnidentifiedImageError
from preview import Preview, encode_frame
from verify_preview import verify
from test_preview import Session


class PixelSession(Session):
    def acquire_animation_pixels(self, version, serial, direction, motion, max_bytes):
        rgba = np.zeros((2, 3, 2, 4), dtype=np.uint8)
        rgba[0, 0, 0] = [10, 20, 30, 255]
        rgba[1, 2, 1] = [60, 70, 80, 0]
        return dict(width=2, height=3, frame_count=2, duration_ms=101,
                    offset_x=-1, offset_y=-2, rgba=rgba)

    def cache_stats(self):
        return {"readers": [] if self.close_calls else ["open"]}


class AcceptanceTests(unittest.TestCase):
    def test_switch_pixel_equality_failure_recovery_and_shutdown(self):
        session = PixelSession()
        viewer = Preview(session, version=0)
        result = verify(viewer, [100000, 100001], cycles=8)
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["checked_frames"], 16)
        self.assertEqual(result["released_clips"], 7)
        self.assertEqual(result["closed_readers"], 0)
        self.assertEqual(result["browser_visual"], "not_run")
        self.assertEqual(session.close_calls, 1)

    def test_corrupt_frame_fails_and_still_closes_session(self):
        session = PixelSession()
        viewer = Preview(session, version=0)
        with patch("preview.encode_frame", return_value=b"broken"):
            with self.assertRaises(UnidentifiedImageError):
                verify(viewer, [100000], cycles=2)
        self.assertEqual(session.close_calls, 1)
        self.assertIsNone(viewer.clip)
        self.assertFalse(viewer.pngs)

    def test_valid_png_with_wrong_pixels_is_rejected(self):
        session = PixelSession()
        with patch("preview.encode_frame", side_effect=lambda frame: encode_frame(np.zeros_like(frame))):
            with self.assertRaises(AssertionError):
                verify(Preview(session, version=0), [100000], cycles=2)
        self.assertEqual(session.close_calls, 1)

    def test_retained_old_pixels_are_detected(self):
        session = PixelSession()
        acquire = session.acquire_animation_pixels
        retained = []
        def leaking(*args, **kwargs):
            clip = acquire(*args, **kwargs)
            retained.append(clip["rgba"])
            return clip
        session.acquire_animation_pixels = leaking
        with self.assertRaisesRegex(AssertionError, "Previous decoded pixel owner"):
            verify(Preview(session, version=0), [100000, 100001], cycles=2)
        self.assertEqual(session.close_calls, 1)

    def test_invalid_input_closes_before_any_server_starts(self):
        for serials, cycles in [([], 2), ([999], 2), ([100000], 1)]:
            session = PixelSession()
            with self.assertRaises(ValueError):
                verify(Preview(session, version=0), serials, cycles)
            self.assertEqual(session.close_calls, 1)


if __name__ == "__main__":
    unittest.main()
