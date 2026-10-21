"""Run the native map/crowd benchmark in visible windows; no game files are modified."""
import argparse
import hashlib
import json
import math
import platform
import re
from pathlib import Path
import statistics
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--exe", type=Path, required=True)
parser.add_argument("--root", type=Path, required=True)
parser.add_argument("--map", default="map/0/2100.dat")
parser.add_argument("--output-dir", type=Path, required=True)
parser.add_argument("--densities", default="1,5,10")
parser.add_argument("--unique", type=int, default=512)
parser.add_argument("--budget-mib", type=int, default=512)
parser.add_argument("--width", type=int, default=1280)
parser.add_argument("--height", type=int, default=720)
parser.add_argument("--warmup", type=int, default=120)
parser.add_argument("--samples", type=int, default=600)
parser.add_argument("--repeats", type=int, default=3)
parser.add_argument("--timeout", type=float, default=120)
parser.add_argument("--load-during-playback", action="store_true")
parser.add_argument("--switch-motions", action="store_true")
parser.add_argument("--validation", action="store_true")
parser.add_argument("--vsync", action="store_true")
args = parser.parse_args()
densities = [int(value) for value in args.densities.split(",")]
if (not densities or any(d < 1 or d > 10 for d in densities) or args.repeats < 1
        or not math.isfinite(args.timeout) or args.timeout <= 0 or args.unique < 1 or args.unique > 20000
        or not 16 <= args.budget_mib <= 1024 or not 1 <= args.samples <= 100000
        or not 0 <= args.warmup <= 100000 or not 320 <= args.width <= 3840 or not 240 <= args.height <= 2160):
    parser.error("invalid density, resource count, viewport, budget or sampling limits")
if args.load_during_playback and args.samples < args.unique - 1:
    parser.error("cold-load mode requires at least unique-1 sample frames")
exe = args.exe.resolve(strict=True)
root = args.root.resolve(strict=True)
out = args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=True)
try:
    import psutil
except ImportError:
    psutil = None
runs = []
for density in densities:
    for repeat in range(args.repeats):
        name = f"density{density}-unique{args.unique}-switch{int(args.switch_motions)}-run{repeat + 1}"
        report_path = out / (name + ".json")
        # Avoid accepting a stale successful report if a later execution fails.
        if report_path.exists():
            parser.error(f"report already exists; choose a fresh output directory: {report_path}")
        cfg = out / (name + ".ini")
        cfg.write_text(f"""[General]
EngineRootPath={root.as_posix()}
[Map]
FilePath={args.map}
[Display]
VSync={str(args.vsync).lower()}
Validation={str(args.validation).lower()}
ViewportScale=1.0
[Benchmark]
ActorsPerCell={density}
UniqueCharacters={args.unique}
AnimationBudgetMiB={args.budget_mib}
SwitchMotions={str(args.switch_motions).lower()}
LoadDuringPlayback={str(args.load_during_playback).lower()}
Width={args.width}
Height={args.height}
WarmupFrames={args.warmup}
SampleFrames={args.samples}
ReportPath={report_path.as_posix()}
""", encoding="utf-8")
        started = time.perf_counter()
        peak_rss = None
        timed_out = False
        log_path = out / (name + ".log")
        with log_path.open("w", encoding="utf-8") as log:
            child = subprocess.Popen([str(exe), "--settings", str(cfg)], stdout=log, stderr=subprocess.STDOUT)
            try:
                while child.poll() is None:
                    if psutil:
                        try:
                            value = psutil.Process(child.pid).memory_info().rss
                            peak_rss = max(peak_rss or 0, value)
                        except psutil.Error:
                            pass
                    if time.perf_counter() - started > args.timeout:
                        timed_out = True
                        child.kill()
                        break
                    time.sleep(0.02)
                child.wait()
            finally:
                # Only terminate the child launched by this run, including Ctrl+C.
                if child.poll() is None:
                    child.kill()
                    child.wait()
        data = json.loads(report_path.read_text()) if report_path.exists() else {"status": "no_report"}
        data.update(exit_code=child.returncode, timeout=timed_out, process_peak_rss_bytes=peak_rss,
                    wall_seconds=time.perf_counter() - started, report=str(report_path))
        gpu = re.search(r"Select physical device\[[^\]]+\]:(.+)", log_path.read_text(encoding="utf-8", errors="replace"))
        data["gpu_name"] = gpu.group(1).strip() if gpu else None
        data["shared_pipeline_creations"] = log_path.read_text(encoding="utf-8",errors="replace").count("Shared sprite pipeline created")
        data["frames_over_16ms"] = sum(ms > 1000/60 for ms in data.get("frame_times_ms", []))
        data["uncapped_verified"] = data.get("present_mode") == "immediate"
        data["valid_sample"] = (not timed_out and child.returncode == 0 and data.get("status") == "complete"
                                and data.get("unique_resources") == args.unique
                                and data.get("actors") == data.get("drawn_actors_last_frame")
                                and (args.vsync or data["uncapped_verified"])
                                and (not args.load_during_playback or data.get("cold_load_count")==args.unique-1)
                                and (not args.switch_motions or data.get("motion_switches",0)>0))
        runs.append(data)
        print(name, {key: data.get(key) for key in
                     ("status", "actors", "unique_resources", "present_mode", "fps", "frame_p99_ms", "frame_max_ms", "valid_sample")}, flush=True)
summary = {"binary_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "platform": platform.platform(), "processor": platform.processor(), "executable": str(exe),
           "notes": "CPU-observed frame-loop timing including GPU backpressure; not GPU timestamps or displayed FPS. Peak RSS is not VRAM.",
           "runs": runs}
summary["by_density"] = []
for density in densities:
    valid = [run for run in runs if run.get("actors_per_cell") == density and run["valid_sample"]]
    summary["by_density"].append({"density": density, "successful_runs": len(valid),
                                  "median_fps": statistics.median(r["fps"] for r in valid) if valid else None,
                                  "worst_run_p99_ms": max((r["frame_p99_ms"] for r in valid), default=None)})
(out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
if any(not run["valid_sample"] for run in runs):
    raise SystemExit(1)
