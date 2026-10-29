"""Repeatable CPU/HTTP acceptance; never opens a browser or initializes Vulkan."""
import argparse
import gc
import io
import json
from pathlib import Path
import sys
import threading
import time
import urllib.error
import urllib.request
from urllib.parse import urlencode
import weakref

from preview import Preview, PNG_CACHE_BYTES, PNG_CACHE_FRAMES
from web_viewer import PreviewServer


def verify(preview, serials, cycles=32):
    """Own and close preview, including on failure. Return metadata only, no assets."""
    import numpy as np
    from PIL import Image
    report = dict(cycles=cycles, checked_frames=0, released_clips=0,
                  max_png_bytes=0, max_png_frames=0, samples=[],
                  browser_visual="not_run", independent_renderer_comparison="not_run")
    server = thread = None
    try:
        if not 2 <= cycles <= 1000:
            raise ValueError("cycles must be between 2 and 1000")
        if not serials or any(serial not in preview.available for serial in serials):
            raise ValueError("Every requested serial must exist in the Classic catalog")
        # Use the real available keys; do not silently replace an unavailable motion/direction.
        selections = []
        for serial in serials:
            bundle = preview.session.acquire_anime_resource(preview.version, serial)
            keys = sorted(bundle.keys(), key=lambda pair: (int(pair[1]), int(pair[0])))
            selections.append([(serial, int(m), int(d)) for d, m in keys])
        if any(not choices for choices in selections):
            raise ValueError("Requested character has no animation keys")
        server = PreviewServer(("127.0.0.1", 0), preview, serials[0])
        worker = threading.Thread(target=server.serve_forever)
        worker.start()
        thread = worker
        base = f"http://127.0.0.1:{server.server_port}"

        def get(path, expected=200):
            try:
                response = urllib.request.urlopen(base + path, timeout=30)
            except urllib.error.HTTPError as error:
                response = error
            with response:
                data = response.read()
                if response.status != expected:
                    raise AssertionError(f"HTTP {response.status}, expected {expected}: {data[:300]!r}")
                return data

        get("/")  # Exactly one page request; every selection below uses the API.
        catalog = json.loads(get("/api/catalog"))
        if not set(serials).issubset(catalog["serials"]):
            raise AssertionError("HTTP catalog differs from the supplied Session")
        old_pixels = old_token = old_key = None
        for i in range(cycles):
            group = selections[i % len(selections)]
            serial, motion, direction = key = group[(i // len(selections)) % len(group)]
            started = time.perf_counter()
            query = urlencode(dict(serial=serial, motion=motion, direction=direction,
                                   client="acceptance", revision=i + 1))
            selected = json.loads(get("/api/select?" + query))
            if tuple(map(int, (selected["serial"], selected["motion"], selected["direction"]))) != key:
                raise AssertionError("HTTP selection silently changed the requested key")
            if old_key is not None and old_key != key:
                gc.collect()
                if old_pixels() is not None:
                    raise AssertionError("Previous decoded pixel owner survived selection")
                report["released_clips"] += 1
                get(f"/api/frame/{old_token}/0.png", 410)
            # Verify metadata and every transmitted frame against the shared CPU result.
            # This checks transport/color/alpha, not an independent decoder/renderer oracle.
            for name in ("width", "height", "frame_count", "duration_ms", "offset_x", "offset_y"):
                if selected[name] != preview.clip[name]:
                    raise AssertionError(f"Metadata mismatch: {name}")
            for frame in range(selected["frame_count"]):
                encoded = get(f"/api/frame/{selected['token']}/{frame}.png")
                with io.BytesIO(encoded) as buffer, Image.open(buffer) as image:
                    np.testing.assert_array_equal(np.array(image.convert("RGBA")), preview.clip["rgba"][frame])
                report["checked_frames"] += 1
                if len(preview.pngs) > PNG_CACHE_FRAMES or preview.png_bytes > PNG_CACHE_BYTES:
                    raise AssertionError("PNG cache exceeded its bounds")
                report["max_png_frames"] = max(report["max_png_frames"], len(preview.pngs))
                report["max_png_bytes"] = max(report["max_png_bytes"], preview.png_bytes)
            old_pixels = weakref.ref(preview.clip["rgba"])
            old_token, old_key = selected["token"], key
            if i < len(serials):
                report["samples"].append({name: selected[name] for name in
                    ("serial", "motion", "direction", "width", "height", "frame_count", "duration_ms", "offset_x", "offset_y")})
            report["last_cycle_http_and_png_ms"] = round((time.perf_counter() - started) * 1000, 2)
            get("/api/select?serial=-1", 400)
            # Error must not prevent retrying the same valid selection.
            retried = json.loads(get("/api/select?" + query))
            if not retried["cache_hit"] or retried["token"] != old_token:
                raise AssertionError("Failed request broke same-selection recovery")
        get(f"/api/frame/{old_token}/{selected['frame_count']}.png", 400)
    finally:
        try:
            if thread is not None:
                server.shutdown()
            if server is not None:
                server.server_close()
            if thread is not None:
                thread.join(timeout=5)
                if thread.is_alive():
                    raise AssertionError("HTTP thread did not stop")
        finally:
            preview.close()
    gc.collect()
    if old_pixels is not None and old_pixels() is not None:
        raise AssertionError("Final decoded pixels survived close")
    if preview.clip is not None or preview.pngs or preview.png_bytes or preview.requests:
        raise AssertionError("Preview retained resource owners after close")
    if preview.session.cache_stats()["readers"]:
        raise AssertionError("Session retained readers after close")
    report.update(status="passed", closed_readers=0, final_pixel_owner_released=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--module-dir", type=Path, default=Path(__file__).resolve().parents[2]/"build/dev/bin")
    parser.add_argument("--serials", type=int, nargs="+", default=[100000, 100001])
    parser.add_argument("--cycles", type=int, default=32)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        if not (args.root/"bin").is_dir():
            raise ValueError("--root must contain installed bin resources")
        sys.path.insert(0, str(args.module_dir.resolve()))
        import cgl
        with cgl.Session(engine_root_path=str(args.root.resolve())) as session:
            report = verify(Preview(session), args.serials, args.cycles)
        text = json.dumps(report, indent=2)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(text + "\n", encoding="utf-8")
        print(text)
    except Exception as error:
        parser.exit(1, f"Preview acceptance failed: {error}\n")


if __name__ == "__main__":
    main()
