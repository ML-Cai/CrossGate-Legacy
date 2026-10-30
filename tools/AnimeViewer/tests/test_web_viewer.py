"""Actual local HTTP routes, worker bounds and shutdown with tiny fake resources."""
import json
import io
from pathlib import Path
import sys
import threading
import tempfile
import subprocess
import unittest
from unittest.mock import patch
import urllib.error
import urllib.request
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from preview import Preview, encode_frame
from web_viewer import PreviewServer
from test_preview import Session

class WebViewerTests(unittest.TestCase):
    def setUp(self):
        self.session=Session();self.preview=Preview(self.session,version=0)
        self.encoder=patch("preview.encode_frame",return_value=b"\x89PNG\r\n\x1a\n").start()
        self.server=PreviewServer(("127.0.0.1",0),self.preview,100000)
        self.thread=threading.Thread(target=self.server.serve_forever);self.thread.start()
        self.base=f"http://127.0.0.1:{self.server.server_port}"
    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join(timeout=5)
        self.preview.close();patch.stopall()
        self.assertFalse(self.thread.is_alive());self.assertEqual(self.session.close_calls,1)
    def get(self,path):
        try:response=urllib.request.urlopen(self.base+path,timeout=5)
        except urllib.error.HTTPError as error:response=error
        with response:return response.status,response.headers,response.read()
    def selection(self,suffix=""):
        status,headers,body=self.get("/api/select?serial=100000"+suffix)
        self.assertEqual(status,200);return json.loads(body)
    def test_fixed_page_and_catalog_need_no_images(self):
        status,headers,body=self.get("/")
        self.assertEqual(status,200);self.assertIn(b'/viewer.js',body);self.assertNotIn(b'iframe',body)
        self.assertEqual(self.get("/viewer.js")[0],200);self.assertEqual(self.get("/playback.js")[0],200)
        status,headers,body=self.get("/api/catalog");self.assertEqual(json.loads(body)["initial"],100000)
        self.assertEqual(self.session.decode_calls,0);self.encoder.assert_not_called()
    def test_switches_work_in_same_http_session_and_only_fetch_requested_frame(self):
        first=self.selection();self.encoder.assert_not_called()
        status,headers,body=self.get(f"/api/frame/{first['token']}/0.png")
        self.assertEqual(status,200);self.assertEqual(headers["Content-Type"],"image/png")
        self.assertEqual(headers["X-Frame-Cache"],"miss");self.assertEqual(len(body),8)
        self.assertEqual(self.get(f"/api/frame/{first['token']}/0.png")[1]["X-Frame-Cache"],"hit")
        second=self.selection("&motion=1&direction=2");self.assertEqual(second["direction"],"2")
        self.assertEqual(self.get(f"/api/frame/{first['token']}/0.png")[0],410)
        self.assertEqual(self.get(f"/api/frame/{second['token']}/0.png")[0],200)
    def test_errors_can_retry_and_routes_cannot_read_arbitrary_files(self):
        for path,status in [("/api/select",400),("/api/select?serial=999",400),("/../preview.py",404),("/api/select?serial=100000&revision=-1",400)]:
            self.assertEqual(self.get(path)[0],status)
        self.session.error=RuntimeError("bad pixels")
        self.assertEqual(self.get("/api/select?serial=100000")[0],422)
        self.session.error=None;self.selection()
    def test_stale_revision_does_not_replace_newer_selection(self):
        latest=self.selection("&client=test&revision=2")
        self.assertEqual(self.get("/api/select?serial=100001&client=test&revision=1")[0],410)
        self.assertEqual(self.preview.description["token"],latest["token"])
    def test_real_png_roundtrip_preserves_rgba_without_native_or_gpu(self):
        import numpy as np
        from PIL import Image
        pixels=np.zeros((2,2,2,4),dtype=np.uint8)
        pixels[0,0,0]=[10,20,30,255];pixels[0,1,1]=[60,70,80,0]
        clip=dict(width=2,height=2,frame_count=2,duration_ms=101,offset_x=-1,offset_y=-2,rgba=pixels)
        with patch.object(self.session,"acquire_animation_pixels",return_value=clip),patch("preview.encode_frame",encode_frame):
            selected=self.selection()
            status,headers,body=self.get(f"/api/frame/{selected['token']}/0.png")
            self.assertEqual(status,200)
            with io.BytesIO(body) as buffer,Image.open(buffer) as image:
                np.testing.assert_array_equal(np.array(image.convert("RGBA")),pixels[0])
    def test_worker_overload_rejected_without_extra_decode(self):
        held=0
        try:
            for _ in range(8):self.assertTrue(self.server.slots.acquire(False));held+=1
            self.assertEqual(self.get("/api/select?serial=100000")[0],503)
            self.assertEqual(self.session.decode_calls,0)
        finally:
            for _ in range(held):self.server.slots.release()
        self.selection()

    def test_direction_group_routes_version_change_and_expired_tokens(self):
        from test_direction_preview import GroupSession
        self.preview.close();self.session=GroupSession();self.preview=Preview(self.session,version=0)
        self.preview.version_options={0:0,1:1};self.server.preview=self.preview
        status,_,body=self.get("/api/group?serial=100000&version=0&client=tab&revision=1")
        self.assertEqual(status,200);group=json.loads(body)
        self.assertEqual(len(group["directions"]),8);self.assertEqual(self.session.frame_calls,0)
        frame=f"/api/direction/{group['token']}/0/0.png"
        self.assertEqual(self.get(frame)[0],200);self.assertEqual(self.get(frame)[1]["X-Frame-Cache"],"hit")
        self.assertEqual(self.get("/api/catalog?version=1&client=tab&revision=2")[0],200)
        self.assertEqual(self.get(frame)[0],410)
        self.assertEqual(self.get("/api/group?serial=100000&version=0&client=tab&revision=1")[0],410)
        self.assertEqual(self.get("/api/catalog?version=99")[0],400)
        self.assertEqual(self.get("/api/catalog?client=x&revision=-1")[0],400)

class StartupTests(unittest.TestCase):
    def test_outdated_binding_reports_actionable_error_without_starting_server(self):
        with tempfile.TemporaryDirectory(prefix="cgl-viewer-startup-") as directory:
            folder=Path(directory).resolve()
            self.assertTrue(folder.is_relative_to(Path(tempfile.gettempdir()).resolve()))
            (folder/"bin").mkdir()
            (folder/"cgl.py").write_text("class Session: pass",encoding="utf-8")
            result=subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/"web_viewer.py"),
                "--root",str(folder),"--module-dir",str(folder)],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,1);self.assertIn("binding is outdated",result.stderr)
            self.assertNotIn("Traceback",result.stderr)
    def test_invalid_root_rejected_before_importing_native_module(self):
        with tempfile.TemporaryDirectory(prefix="cgl-viewer-root-") as directory:
            self.assertTrue(Path(directory).resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()))
            result=subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/"web_viewer.py"),
                "--root",directory],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,2);self.assertIn("installed bin resources",result.stderr)

if __name__=="__main__":unittest.main()
