"""Single active CPU clip and bounded PNG cache for a local resource preview."""
from collections import OrderedDict
import io
import threading
import time
import secrets

MAX_FRAMES = 256
MAX_FRAME_BYTES = 4 * 1024 * 1024
PNG_CACHE_BYTES = 8 * 1024 * 1024
PNG_CACHE_FRAMES = 12


def encode_frame(frame):
    from PIL import Image
    with io.BytesIO() as output, Image.fromarray(frame) as image:
        image.save(output, format="PNG", compress_level=1)
        return output.getvalue()


class Preview:
    def __init__(self, session, max_bytes=8 * 1024 * 1024, version=None):
        if not 1 <= max_bytes <= 32 * 1024 * 1024:
            raise ValueError("max_bytes must be between 1 and 33554432")
        self.session = session
        self.max_bytes = max_bytes
        self.lock = threading.RLock()
        self.closed = False
        if version is None:
            import cgl
            version = cgl.CrossGateVersion.CG_VERSION_Classic
        self.version = version
        self.serials = tuple(session.acquire_available_anime_serial_nums(version))
        self.available = set(self.serials)
        self.clip = None
        self.key = None
        self.description = None
        self.requests = OrderedDict()
        self.pngs = OrderedDict()
        self.png_bytes = 0
        if not self.serials:
            raise ValueError("No Classic animation IDs found in this asset root")

    def _clear(self):
        self.clip = self.key = self.description = None
        self.pngs.clear()
        self.png_bytes = 0

    def select(self, serial, motion=None, direction=None, client="", revision=0):
        with self.lock:
            if self.closed:
                raise RuntimeError("Preview is closed")
            if client:
                if revision < self.requests.get(client,-1):
                    raise LookupError("Selection was replaced by a newer request")
                self.requests[client] = revision; self.requests.move_to_end(client)
                while len(self.requests)>16: self.requests.popitem(last=False)
            started = time.perf_counter()
            serial = int(serial)
            if serial not in self.available:
                raise ValueError("Select a character from the loaded catalog")
            bundle = self.session.acquire_anime_resource(self.version, serial)
            if bundle is None:
                raise ValueError("Animation description is missing")
            keys = sorted(bundle.keys(), key=lambda k: (int(k[1]), int(k[0])))
            if not keys:
                raise ValueError("Character has no available motions/directions")
            motions = {int(m): m for d, m in keys}
            chosen_motion = int(motion) if motion is not None and int(motion) in motions else min(motions)
            directions = {int(d): d for d, m in keys if int(m) == chosen_motion}
            chosen_direction = int(direction) if direction is not None and int(direction) in directions else min(directions)
            m, d = motions[chosen_motion], directions[chosen_direction]
            key = (serial, chosen_motion, chosen_direction)
            if key == self.key:
                return dict(self.description, cache_hit=True, load_ms=0)
            self._clear()  # Release old pixels before admitting another clip.
            desc = bundle.acquire(d, m)
            if not 1 <= len(desc.motionGraphicsSerialNums) <= MAX_FRAMES or desc.duration <= 0:
                raise ValueError("Animation needs 1..256 frames and a positive duration")
            clip = self.session.acquire_animation_pixels(self.version, serial, d, m, max_bytes=self.max_bytes)
            if clip["width"] * clip["height"] * 4 > MAX_FRAME_BYTES:
                raise ValueError("Frame canvas exceeds the 4 MiB browser limit")
            metadata = {k: clip[k] for k in ("width", "height", "frame_count", "duration_ms", "offset_x", "offset_y")}
            metadata.update(serial=str(serial), motion=str(chosen_motion), direction=str(chosen_direction),
                            motions=[(v.name,str(k)) for k,v in sorted(motions.items())],
                            directions=[(v.name,str(k)) for k,v in sorted(directions.items())],
                            label=f"{serial} / {m.name} / {d.name}", token=secrets.token_urlsafe(12),
                            cache_hit=False, load_ms=round((time.perf_counter()-started)*1000,2))
            self.clip, self.key, self.description = clip, key, metadata
            return dict(metadata)

    def frame(self, token, index):
        with self.lock:
            if self.closed or self.clip is None or token != self.description["token"]:
                raise LookupError("Animation changed; select it again")
            if not 0 <= index < self.clip["frame_count"]:
                raise ValueError("Frame index is outside the animation")
            if index in self.pngs:
                data = self.pngs.pop(index); self.pngs[index] = data
                return data, True
            data = encode_frame(self.clip["rgba"][index])
            if len(data) > PNG_CACHE_BYTES:
                raise ValueError("PNG frame exceeds the browser transfer limit")
            while self.pngs and (len(self.pngs) >= PNG_CACHE_FRAMES or self.png_bytes+len(data) > PNG_CACHE_BYTES):
                self.png_bytes -= len(self.pngs.popitem(last=False)[1])
            self.pngs[index] = data; self.png_bytes += len(data)
            return data, False

    def close(self):
        with self.lock:
            if not self.closed:
                self.closed = True
                self._clear()
                self.requests.clear()
                self.session.close()
