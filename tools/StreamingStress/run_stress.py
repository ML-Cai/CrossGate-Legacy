"""Run the native client's fixed-step streaming workload at multiple budgets.

No Python engine binding/server is needed. psutil is optional for process memory.
"""
from collections import deque
import argparse
import json
import math
from pathlib import Path
import subprocess
import time
import uuid


def validate_report(report, budget, frames, replace_frames):
    if report.get("status") != "complete" or not report.get("stress_completed") or report.get("scene_loading"):
        raise ValueError("Client did not complete the scripted workload")
    if report.get("stream_frames") != frames or report.get("stress_configured_frames") != frames:
        raise ValueError("Unexpected scripted update count")
    limit = budget * 1024 * 1024
    used = sum(report[k] for k in ("animation_rgba_bytes", "animation_retired_bytes", "animation_pending_reserved_bytes"))
    if report["animation_budget_bytes"] != limit or not 0 <= used <= limit or not 0 <= report["stream_peak_accounted_bytes"] <= limit:
        raise ValueError("Animation accounting exceeds requested budget")
    if report["stream_snapshots"] < 2 or report["stream_replacements"] < 2 or report["stream_travel_cells"] <= 0:
        raise ValueError("Workload did not exercise movement and replacement")
    if replace_frames <= 0 or report["stream_replacements"] != 1 + frames // replace_frames:
        raise ValueError("Unexpected full replacement cadence")
    visible = report["stream_visible_samples"]
    ready = report["stream_ready_visible_samples"]
    if not 0 <= ready <= visible or visible <= 0:
        raise ValueError("Invalid visible coverage counters")
    for key in ("lifetime_fps", "lifetime_frame_max_ms", "frame_p99_ms"):
        if not math.isfinite(report[key]) or report[key] < 0:
            raise ValueError("Invalid timing data")
    # Low-budget quality is deliberately reported, not mistaken for load success.
    return ready / visible


def run_case(args, budget, output):
    prefix = output / ("budget-" + str(budget) + "-" + uuid.uuid4().hex[:8])
    config, report_path, log_path = (Path(str(prefix) + suffix) for suffix in (".ini", ".json", ".log"))
    config.write_text(f"""[General]
EngineRootPath={args.root.as_posix()}
[Map]
FilePath={args.map}
[Animation]
SerialNum=100000
Direction=4
[Display]
VSync=false
Validation={str(args.validation).lower()}
[ServerDemo]
Enabled=true
AnimationBudgetMiB={budget}
VisibleActors={args.visible}
PrefetchActors={args.prefetch}
CatalogLimit={args.catalog}
Seed={args.seed}
StressFrames={args.frames}
StressLegFrames={args.leg_frames}
StressReplaceFrames={args.replace_frames}
StressStepMs=16.666667
[Telemetry]
Enabled=true
ReportPath={report_path.as_posix()}
""", encoding="utf-8")
    try:
        import psutil
    except ImportError:
        psutil = None
    peak = None
    rss_tail = deque(maxlen=128)
    with log_path.open("w", encoding="utf-8") as log:
        proc = subprocess.Popen([str(args.client), "--settings", str(config)], stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        started = time.monotonic()
        try:
            monitor = None
            if psutil:
                try:
                    monitor = psutil.Process(proc.pid)
                except psutil.NoSuchProcess:
                    pass
            while proc.poll() is None:
                if time.monotonic() - started > args.timeout:
                    raise TimeoutError("Client exceeded timeout; see " + str(log_path))
                if monitor:
                    try:
                        rss = monitor.memory_info().rss
                        peak = max(peak or 0, rss)
                        rss_tail.append(rss)
                    except psutil.NoSuchProcess:
                        pass
                time.sleep(.02)
            if proc.returncode:
                raise RuntimeError(f"Client exited {proc.returncode}; see {log_path}")
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
    report = json.loads(report_path.read_text(encoding="utf-8"))
    coverage = validate_report(report, budget, args.frames, args.replace_frames)
    return {"budget_mib": budget, "report": str(report_path), "visible_coverage": coverage,
            "peak_process_bytes": peak,
            "tail_process_mean_bytes": sum(rss_tail)/len(rss_tail) if rss_tail else None,
            "tail_process_min_bytes": min(rss_tail) if rss_tail else None,
            "tail_process_max_bytes": max(rss_tail) if rss_tail else None, "workload_fingerprint": report["stream_snapshot_fingerprint"],
            **{key: report[key] for key in ("lifetime_fps", "lifetime_frame_max_ms", "lifetime_frames_over_33ms",
                "stream_budget_wait_frames", "stream_peak_accounted_bytes", "stream_missing_visible_frames", "animation_load_failed")}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--map", default="map/0/100.dat")
    parser.add_argument("--budgets", type=int, nargs="+", default=[16, 64, 512])
    parser.add_argument("--frames", type=int, default=6000)
    parser.add_argument("--visible", type=int, default=128)
    parser.add_argument("--prefetch", type=int, default=160)
    parser.add_argument("--catalog", type=int, default=1024)
    parser.add_argument("--seed", type=int, default=3169)
    parser.add_argument("--leg-frames", type=int, default=240)
    parser.add_argument("--replace-frames", type=int, default=600)
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--validation", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("build/streaming-stress"))
    args = parser.parse_args()
    args.client, args.root = args.client.resolve(), args.root.resolve()
    if not args.client.is_file() or not args.root.is_dir():
        parser.error("Client executable and asset root must exist")
    if any(b < 16 or b > 1024 for b in args.budgets) or not 1 <= args.visible <= 1024 or not 0 <= args.prefetch <= 1024-args.visible or not args.visible+args.prefetch <= args.catalog <= 4096:
        parser.error("Invalid budget/count/catalog limits")
    if not 1 <= args.frames <= 1000000 or not 1 <= args.leg_frames <= 6000 or not 1 <= args.replace_frames <= 60000 or args.frames < args.replace_frames or not 0 <= args.seed <= 0xffffffff or not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("Invalid schedule, seed or timeout")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for budget in args.budgets:
        result = run_case(args, budget, output)
        results.append(result)
        if result["workload_fingerprint"] != results[0]["workload_fingerprint"]:
            raise RuntimeError("Workload changed between budgets; compare settings, corpus or manual input")
        print(json.dumps(result), flush=True)
        # Preserve partial successful measurements if a later case fails.
        (output / "summary.json").write_text(json.dumps(results, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
