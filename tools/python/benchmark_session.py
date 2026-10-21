"""Measure a real Session lifecycle; optional psutil adds process RSS samples."""
import argparse
import gc
import json
import sys
import time
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--module-dir", required=True)
parser.add_argument("--root", required=True)
parser.add_argument("--anime-serial", type=int, default=100000)
parser.add_argument("--iterations", type=int, default=20)
parser.add_argument("--cache-budget-bytes", type=int, default=4 * 1024 * 1024)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
if args.iterations < 1 or args.cache_budget_bytes < 0:
    parser.error("iterations must be positive and the cache budget nonnegative")
sys.path.insert(0, args.module_dir)
import cgl
import numpy  # Initialize array runtime before sampling the empty Session.
try:
    import psutil
    process = psutil.Process()
except ImportError:
    process = None

def rss():
    return process.memory_info().rss if process else None

session = cgl.Session(args.root, args.cache_budget_bytes)
version = cgl.CrossGateVersion.CG_VERSION_Classic
report = {
    "scope": "Classic CPU graphics/animation/palette Session, no map scene or GPU",
    "rss_available": process is not None,
    "rss_note": "RSS includes Python, NumPy and allocator retention; a plateau is not proof of leak freedom.",
    "phases": [], "cycles": [],
}

def sample(name, start):
    report["phases"].append({"name": name, "elapsed_seconds": time.perf_counter() - start,
                              "rss_bytes": rss(), "cache": session.cache_stats()})

def load():
    anime = session.acquire_anime_resource(version, args.anime_serial)
    if anime is None:
        raise RuntimeError("Animation was not found")
    serials = next((anime.acquire(*key).motionGraphicsSerialNums
                    for key in anime.keys() if anime.acquire(*key).motionGraphicsSerialNums), None)
    if not serials:
        raise RuntimeError("Animation has no frames")
    graphics = session.acquire_graphics_resource(version, serials[0])
    palette = session.acquire_env_palette(cgl.EnvironmentPaletteTypes.Daytime)
    if graphics is None or palette is None:
        raise RuntimeError("Graphics or palette was not found")
    image = session.apply_palette(graphics, palette)
    return graphics, anime, palette, image

try:
    start = time.perf_counter(); sample("empty", start)
    start = time.perf_counter(); held = load(); sample("loaded", start)
    checksum = int(held[3].sum())
    start = time.perf_counter()
    for _ in range(args.iterations):
        load()
    sample("hot", start)
    start = time.perf_counter(); session.trim_cache(); sample("trimmed_with_output_held", start)
    start = time.perf_counter(); session.invalidate_resources(); sample("invalidated_with_output_held", start)
    assert int(held[3].sum()) == checksum
    start = time.perf_counter(); held = None; gc.collect(); sample("output_released", start)
    for i in range(args.iterations):
        cycle_start = time.perf_counter()
        held = load()
        session.invalidate_resources()
        held = None
        gc.collect()
        stats = session.cache_stats()
        assert stats["readers"] == [] and stats["graphics_index_readers"] == 0 and stats["anime_index_readers"] == 0
        report["cycles"].append({"iteration": i + 1, "elapsed_seconds": time.perf_counter() - cycle_start,
                                  "rss_bytes": rss(), "retained_reader_count": len(stats["readers"])})
    start = time.perf_counter(); held = load(); sample("reloaded", start)
finally:
    close_start = time.perf_counter()
    session.close()
    held = None
    gc.collect()
sample("closed", close_start)
text = json.dumps(report, indent=2)
if args.output:
    args.output.write_text(text + "\n", encoding="utf-8")
else:
    print(text)
