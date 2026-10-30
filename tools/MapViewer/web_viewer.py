"""Read-only Classic map viewport, composed on CPU with shared native map rules."""
import argparse
from collections import OrderedDict
from http.server import HTTPServer, BaseHTTPRequestHandler
import io
import json
import math
from pathlib import Path
import sys
from urllib.parse import parse_qs, urlsplit

from PIL import Image


class MapView:
    PAGE_BYTES = 64 * 1024 * 1024

    def __init__(self, session, file_path):
        self.session = session
        self.map = session.load_map_preview(file_path)
        self.pages = OrderedDict()
        self.bytes = 0

    def page(self, index):
        if index in self.pages:
            image = self.pages.pop(index)
            self.pages[index] = image
            return image
        # Evict before native decode; a page is bounded to 32 MiB by MapBuilder.
        while self.pages and (len(self.pages) >= 2 or self.bytes > self.PAGE_BYTES - 32*1024*1024):
            _, image = self.pages.popitem(last=False)
            self.bytes -= image.width * image.height * 4
            image.close()
        pixels = self.session.acquire_map_page(self.map, index)
        image = Image.fromarray(pixels)
        self.pages[index] = image
        self.bytes += image.width * image.height * 4
        return image

    def render(self, x, y, width, height, scale, ground=True, objects=True):
        draws = self.map.draw_list(x, y, width, height, scale, ground, objects)
        # Collect unique visible sprites page-by-page before painter-order composition.
        # Otherwise interleaved atlas IDs can repeatedly decode evicted 32 MiB pages.
        sources = {(draw["page"], tuple(draw["source"])) for draw in draws}
        if sum(source[2]*source[3]*4 for _, source in sources) > 16*1024*1024:
            raise ValueError("Visible image payload exceeds 16 MiB; zoom in or disable objects")
        sprites = {}
        try:
            for index, source in sorted(sources):
                sx, sy, sw, sh = source
                sprites[index, source] = self.page(index).crop((sx, sy, sx+sw, sy+sh))
            return self.compose(draws, sprites, width, height, scale)
        finally:
            for image in sprites.values():
                image.close()

    def compose(self, draws, sprites, width, height, scale):
        with Image.new("RGBA", (width, height), (32, 38, 43, 255)) as canvas:
            for draw in draws:
                sx, sy, sw, sh = draw["source"]
                sprite = sprites[draw["page"], tuple(draw["source"])]
                dx, dy = draw["x"], draw["y"]
                # Crop to the viewport before scaling; never allocate a huge zoomed sprite.
                left = max(0, math.floor(-dx / scale))
                top = max(0, math.floor(-dy / scale))
                right = min(sw, math.ceil((width-dx) / scale))
                bottom = min(sh, math.ceil((height-dy) / scale))
                if right <= left or bottom <= top:
                    continue
                with sprite.crop((left, top, right, bottom)) as crop:
                    size = (max(1, round(crop.width*scale)), max(1, round(crop.height*scale)))
                    with crop.resize(size, Image.Resampling.NEAREST) as scaled:
                        canvas.alpha_composite(scaled, (round(dx+left*scale), round(dy+top*scale)))
            with io.BytesIO() as output:
                canvas.save(output, format="PNG", compress_level=1)
                return output.getvalue()

    def close(self):
        for image in self.pages.values():
            image.close()
        self.pages.clear()
        self.bytes = 0
        self.map = None
        self.session.close()


class Server(HTTPServer):
    # One request at a time deliberately serializes the Session and bounds CPU work.
    request_queue_size = 2

    def __init__(self, address, view):
        self.view = view
        super().__init__(address, Handler)


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(10)

    def send(self, status, data, kind):
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        try:
            url = urlsplit(self.path)
            if url.path in ("/", "/viewer.js"):
                filename, kind = ("viewer.html", "text/html; charset=utf-8") if url.path == "/" else ("viewer.js", "text/javascript; charset=utf-8")
                self.send(200, Path(__file__).with_name(filename).read_bytes(), kind)
            elif url.path == "/api/map":
                world = self.server.view.map
                self.send(200, json.dumps(dict(width=world.width, height=world.height, version="Classic", readonly=True)).encode(), "application/json")
            elif url.path == "/api/pan":
                q = parse_qs(url.query)
                x, y = self.server.view.map.pan(*(float(q[key][0]) for key in ("x", "y", "dx", "dy", "scale")))
                self.send(200, json.dumps(dict(x=x, y=y)).encode(), "application/json")
            elif url.path == "/api/view.png":
                q = parse_qs(url.query)
                x, y = float(q["x"][0]), float(q["y"][0])
                width, height = int(q["width"][0]), int(q["height"][0])
                scale = float(q.get("scale", ["1"])[0])
                # Validate independently before asking the native API or allocating a canvas.
                world = self.server.view.map
                if not all(map(math.isfinite, (x, y, scale))) or not 0 <= x <= world.width-1 or not 0 <= y <= world.height-1 or not 1 <= width <= 1280 or not 1 <= height <= 960 or not .5 <= scale <= 2:
                    raise ValueError("View requires an in-map center, 1..1280 x 1..960 pixels and scale 0.5..2")
                data = self.server.view.render(x, y, width, height, scale, q.get("ground", ["1"])[0] == "1", q.get("objects", ["1"])[0] == "1")
                self.send(200, data, "image/png")
            else:
                self.send(404, b'{"error":"Unknown route"}', "application/json")
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            pass
        except Exception as error:
            try:
                self.send(400 if isinstance(error, (KeyError, ValueError)) else 422,
                          json.dumps({"error": str(error)}).encode(), "application/json")
            except (BrokenPipeError, ConnectionResetError, TimeoutError):
                pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--map", default="map/0/100.dat")
    parser.add_argument("--port", type=int, default=7875)
    parser.add_argument("--module-dir", type=Path, default=Path(__file__).resolve().parents[2]/"build/dev/bin")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    sys.path.insert(0, str(args.module_dir.resolve()))
    try:
        import cgl
        if not hasattr(cgl.Session, "load_map_preview"):
            raise RuntimeError(f"Rebuild cgl_pybind: binding lacks map preview API ({cgl.__file__})")
        with cgl.Session(engine_root_path=str(args.root.resolve())) as session:
            view = MapView(session, args.map)
            try:
                with Server(("127.0.0.1", args.port), view) as server:
                    print(f"Read-only map preview: http://127.0.0.1:{args.port}/", flush=True)
                    server.serve_forever()
            except KeyboardInterrupt:
                pass
            finally:
                view.close()
    except (ImportError, RuntimeError, ValueError, OSError) as error:
        parser.exit(1, f"Map preview could not start: {error}\n")


if __name__ == "__main__":
    main()
