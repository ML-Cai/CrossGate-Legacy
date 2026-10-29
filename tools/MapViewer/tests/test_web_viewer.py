"""CPU composition, bounded PIL page owners and real loopback HTTP routes."""
import io
import json
from pathlib import Path
import sys
import threading
import types
import unittest
import urllib.error
import urllib.request
import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from web_viewer import MapView, Server


class Session:
    def __init__(self):
        self.closed = 0
        self.decodes = 0
        self.world = types.SimpleNamespace(width=10, height=12, draw_list=self.draws,
                                          pan=lambda x,y,dx,dy,scale: (x-dx/scale,y-dy/scale))

    def draws(self, x,y,width,height,scale,ground,objects):
        return [dict(page=0, source=(0,0,2,2), x=-.5,y=-.5)] if ground or objects else []

    def load_map_preview(self, path): return self.world

    def acquire_map_page(self, view, index):
        self.decodes += 1
        pixels = np.zeros((2,2,4),dtype=np.uint8)
        pixels[:]=[240,10,20,255]
        return pixels

    def close(self): self.closed += 1


class CompositionTests(unittest.TestCase):
    def test_real_png_clips_scales_and_layers_without_full_map_canvas(self):
        view=MapView(Session(),"test.dat")
        try:
            with Image.open(io.BytesIO(view.render(0,0,8,6,2))) as image:
                self.assertEqual(image.size,(8,6))
                self.assertEqual(image.getpixel((0,0)),(240,10,20,255))
                self.assertEqual(image.getpixel((7,5)),(32,38,43,255))
            with Image.open(io.BytesIO(view.render(0,0,8,6,1,False,False))) as image:
                self.assertEqual(image.getpixel((0,0)),(32,38,43,255))
        finally: view.close()

    def test_lru_two_pages_and_explicit_image_close(self):
        session=Session();view=MapView(session,"test.dat")
        first=view.page(0)
        self.assertIs(view.page(0),first)
        view.page(1);view.page(2)
        self.assertEqual(len(view.pages),2);self.assertEqual(view.bytes,32)
        self.assertEqual(session.decodes,3)
        with self.assertRaises(ValueError):first.getpixel((0,0))
        last=view.page(2);view.close()
        with self.assertRaises(ValueError):last.getpixel((0,0))
        self.assertEqual(view.bytes,0);self.assertIsNone(view.map)
        self.assertEqual(session.closed,1)

    def test_interleaved_pages_decode_once_and_crops_close_on_failure(self):
        from unittest.mock import patch
        session=Session();view=MapView(session,"test.dat")
        session.world.draw_list=lambda *args:[dict(page=i%3,source=(0,0,2,2),x=0,y=0) for i in range(30)]
        crops=[]
        def fail(draws,sprites,*args):
            crops.extend(sprites.values())
            raise RuntimeError("composition failed")
        with patch.object(view,"compose",side_effect=fail):
            with self.assertRaisesRegex(RuntimeError,"composition failed"):view.render(0,0,8,6,1)
        self.assertEqual(session.decodes,3)
        for crop in crops:
            with self.assertRaises(ValueError):crop.getpixel((0,0))
        view.close()

    def test_visible_payload_budget_rejects_before_page_decode(self):
        view=MapView(Session(),"test.dat")
        view.map.draw_list=lambda *args:[dict(page=0,source=(0,0,4096,4096),x=0,y=0)]
        with self.assertRaisesRegex(ValueError,"16 MiB"):view.render(0,0,8,6,1)
        self.assertEqual(view.session.decodes,0)
        view.close()


class HttpTests(unittest.TestCase):
    def setUp(self):
        self.view=MapView(Session(),"test.dat")
        self.server=Server(("127.0.0.1",0),self.view)
        self.thread=threading.Thread(target=self.server.serve_forever);self.thread.start()
        self.base=f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join(timeout=5);self.view.close()
        self.assertFalse(self.thread.is_alive())

    def get(self,path):
        try:r=urllib.request.urlopen(self.base+path,timeout=5)
        except urllib.error.HTTPError as error:r=error
        with r:return r.status,r.headers,r.read()

    def test_routes_no_reload_pan_layers_and_no_arbitrary_files(self):
        self.assertEqual(self.get("/")[0],200)
        self.assertEqual(self.get("/viewer.js")[0],200)
        self.assertTrue(json.loads(self.get("/api/map")[2])["readonly"])
        query="/api/view.png?x=1&y=2&width=8&height=6"
        for suffix in ["", "&scale=2", "&ground=0&objects=0"]:
            status,headers,data=self.get(query+suffix)
            self.assertEqual(status,200);self.assertEqual(headers["Cache-Control"],"no-store")
            with Image.open(io.BytesIO(data)) as image:self.assertEqual(image.size,(8,6))
        self.assertEqual(self.get("/api/pan?x=1&y=2&dx=0&dy=1&scale=1")[0],200)
        self.assertEqual(self.get("/../web_viewer.py")[0],404)

    def test_bad_inputs_rejected_before_decode_and_retry_works(self):
        for q in ["", "x=nan&y=0&width=8&height=6", "x=0&y=0&width=1281&height=6",
                  "x=0&y=0&width=8&height=6&scale=0", "x=-1&y=0&width=8&height=6"]:
            self.assertEqual(self.get("/api/view.png?"+q)[0],400)
        self.assertEqual(self.view.session.decodes,0)
        self.assertEqual(self.get("/api/view.png?x=0&y=0&width=8&height=6")[0],200)


if __name__=="__main__":unittest.main()
