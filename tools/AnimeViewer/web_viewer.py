"""Local-only character preview: fixed page, small JSON selections and lazy PNG frames."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import sys
import threading
from urllib.parse import parse_qs, urlsplit

ASSETS = {"/": ("viewer.html", "text/html; charset=utf-8"),
          "/viewer.js": ("viewer.js", "text/javascript; charset=utf-8"),
          "/playback.js": ("playback.js", "text/javascript; charset=utf-8")}


class PreviewServer(ThreadingHTTPServer):
    # At most eight request workers; no unbounded image/selection queue.
    request_queue_size = 4
    def __init__(self, address, preview, initial_serial=None):
        self.preview = preview
        self.initial_serial = initial_serial if initial_serial is not None else (100000 if 100000 in preview.available else preview.serials[0])
        self.slots = threading.BoundedSemaphore(8)
        super().__init__(address, Handler)
    def process_request(self, request, address):
        if not self.slots.acquire(blocking=False):
            try: Handler(request,address,self,overloaded=True)
            except OSError: pass
            finally: self.shutdown_request(request)
            return
        try: super().process_request(request,address)
        except BaseException:
            self.slots.release(); raise
    def process_request_thread(self, request, address):
        try: super().process_request_thread(request,address)
        finally: self.slots.release()


class Handler(BaseHTTPRequestHandler):
    def __init__(self,*args,overloaded=False,**kwargs):
        self.overloaded=overloaded
        super().__init__(*args,**kwargs)
    def setup(self):
        super().setup(); self.connection.settimeout(0.25 if self.overloaded else 15)
    def send(self,status,data,kind,**headers):
        self.send_response(status)
        self.send_header("Content-Type",kind)
        self.send_header("Content-Length",str(len(data)))
        self.send_header("Cache-Control","no-store")
        self.send_header("X-Content-Type-Options","nosniff")
        for key,value in headers.items(): self.send_header(key,str(value))
        self.end_headers()
        self.wfile.write(data)
    def json(self,status,value):
        self.send(status,json.dumps(value).encode("utf-8"),"application/json; charset=utf-8")
    def do_GET(self):
        if self.overloaded:
            self.json(503,{"error":"Preview is busy; retry after the current load finishes"})
            return
        try: self.respond()
        except (BrokenPipeError,ConnectionResetError,TimeoutError): pass
    def respond(self):
        try:
            url = urlsplit(self.path)
            if url.path in ASSETS:
                name,kind = ASSETS[url.path]
                self.send(200,Path(__file__).with_name(name).read_bytes(),kind)
            elif url.path == "/api/catalog":
                self.json(200,{"serials":list(self.server.preview.serials),"initial":self.server.initial_serial})
            elif url.path == "/api/select":
                query = parse_qs(url.query)
                client = query.get("client",[""])[0]
                revision = int(query.get("revision",["0"])[0])
                if not re.fullmatch(r"[A-Za-z0-9_-]{0,40}",client) or not 0 <= revision <= 2**53-1:
                    raise ValueError("Invalid selection revision")
                result = self.server.preview.select(query["serial"][0],query.get("motion",[None])[0],query.get("direction",[None])[0],client,revision)
                self.json(200,result)
            elif match := re.fullmatch(r"/api/frame/([A-Za-z0-9_-]{1,32})/([0-9]{1,5})\.png",url.path):
                data,hit = self.server.preview.frame(match[1],int(match[2]))
                self.send(200,data,"image/png",**{"X-Frame-Cache":"hit" if hit else "miss"})
            else: self.json(404,{"error":"Unknown preview route"})
        except (BrokenPipeError,ConnectionResetError): pass  # User changed selection/closed page.
        except (ValueError,KeyError) as error: self.json(400,{"error":str(error)})
        except LookupError as error: self.json(410,{"error":str(error)})
        except Exception as error: self.json(422,{"error":str(error)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",required=True,type=Path)
    parser.add_argument("--module-dir",type=Path,default=Path(__file__).resolve().parents[2]/"build/dev/bin")
    parser.add_argument("--serial",type=int)
    parser.add_argument("--max-mib",type=int,default=8,choices=range(1,33),metavar="1..32")
    parser.add_argument("--port",type=int,default=7860)
    args = parser.parse_args()
    if not (args.root/"bin").is_dir(): parser.error("--root must contain installed bin resources")
    if not 1 <= args.port <= 65535: parser.error("--port must be between 1 and 65535")
    sys.path.insert(0,str(args.module_dir.resolve()))
    try:
        import cgl
        required=("acquire_available_anime_serial_nums","acquire_animation_pixels")
        if not hasattr(cgl,"Session") or any(not hasattr(cgl.Session,name) for name in required):
            raise RuntimeError(f"cgl binding is outdated; rebuild cgl_pybind from PR #91 or newer. Loaded: {cgl.__file__}")
        from preview import Preview
        with cgl.Session(engine_root_path=str(args.root.resolve())) as session:
            preview = Preview(session,max_bytes=args.max_mib*1024*1024)
            if args.serial is not None and args.serial not in preview.available:
                parser.error("--serial is not in the Classic catalog")
            try:
                with PreviewServer(("127.0.0.1",args.port),preview,args.serial) as server:
                    print(f"Character preview: http://127.0.0.1:{args.port}/",flush=True)
                    server.serve_forever()
            except KeyboardInterrupt: pass
            finally: preview.close()
    except (ImportError,RuntimeError,ValueError,OSError) as error:
        parser.exit(1,f"Character preview could not start: {error}\n")


if __name__ == "__main__": main()
