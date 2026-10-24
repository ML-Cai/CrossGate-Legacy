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


DEMAND_FIELDS=(
    'demand_started',
    'demand_completed',
    'demand_withdrawn',
    'demand_failed',
    'demand_samples',
    'demand_pending',
    'demand_visible_pending',
    'demand_oldest_pending_ms',
    'demand_oldest_visible_ms',
    'demand_peak_pending_ms',
    'demand_peak_visible_ms',
    'demand_mean_ms',
    'demand_p95_ms',
    'demand_p99_ms',
    'demand_max_ms',
    'demand_queued_mean_ms',
    'demand_deferred_mean_ms',
    'demand_loading_mean_ms',
    'demand_visible_samples',
    'demand_visible_completed',
    'demand_visible_mean_ms',
    'demand_visible_p95_ms',
    'demand_visible_p99_ms',
    'demand_visible_max_ms',
)


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
    deferred_keys=("stream_budget_deferred_appearances", "stream_peak_budget_deferred_appearances", "stream_budget_deferred_frames")
    if any(k in report for k in deferred_keys):
        if not all(k in report for k in deferred_keys):
            raise ValueError("Incomplete deferred demand counters")
        current,peak,updates=(report[k] for k in deferred_keys)
        if not all(type(v) is int for v in (current,peak,updates)) or not 0 <= current <= peak <= 1024 or not 0 <= updates <= frames:
            raise ValueError("Invalid deferred demand counters")
    if any(k in report for k in DEMAND_FIELDS):
        if not all(k in report for k in DEMAND_FIELDS):
            raise ValueError("Incomplete demand latency counters")
        d={k[len("demand_"):]:report[k] for k in DEMAND_FIELDS}
        for k in ("started","completed","withdrawn","failed","samples","pending","visible_pending","visible_samples","visible_completed"):
            if type(d[k]) is not int or d[k]<0:raise ValueError("Invalid demand count")
        if d["started"]!=d["completed"]+d["withdrawn"]+d["failed"]+d["pending"] or not 0<=d["visible_pending"]<=d["pending"]<=1024:
            raise ValueError("Inconsistent demand outcomes")
        if not d["samples"]<=min(256,d["completed"]) or not d["visible_samples"]<=min(256,d["visible_completed"]) or d["visible_completed"]>d["completed"]:
            raise ValueError("Invalid demand sample window")
        for k,v in d.items():
            if k.endswith("_ms") and (not math.isfinite(v) or v<0):raise ValueError("Invalid demand timing")
        if not d["oldest_pending_ms"]<=d["peak_pending_ms"] or not d["oldest_visible_ms"]<=d["peak_visible_ms"]<=d["peak_pending_ms"]:
            raise ValueError("Invalid demand age high water marks")
        for prefix in ("", "visible_"):
            if not d[prefix+"p95_ms"]<=d[prefix+"p99_ms"]<=d[prefix+"max_ms"] or d[prefix+"mean_ms"]>d[prefix+"max_ms"]:
                raise ValueError("Invalid demand timing order")
        if d["oldest_visible_ms"]>d["oldest_pending_ms"] or not math.isclose(d["mean_ms"],sum(d[k] for k in ("queued_mean_ms","deferred_mean_ms","loading_mean_ms")),rel_tol=1e-4,abs_tol=.02):
            raise ValueError("Invalid demand phase partition")
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
            **{key: report[key] for key in DEMAND_FIELDS if key in report},
            **{key: report[key] for key in ("stream_budget_deferred_appearances",
                "stream_peak_budget_deferred_appearances", "stream_budget_deferred_frames") if key in report},
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
