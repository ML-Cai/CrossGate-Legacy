"""Run repeated scene/resource lifetimes in ONE native process/device.

The test pauses at each quiescent checkpoint, allowing an optional RSS sample
before the next reload. RSS is process memory, not physical VRAM or a leak proof.
"""
import argparse
import json
import math
import os
from pathlib import Path
import queue
import re
import statistics
import subprocess
import threading
import time
import uuid

TEST_NAME = "DesktopSceneTest.ResourceLifecycleReturnsToQuiescentBaseline"
PREFIX = "CGL_SOAK_CHECKPOINT "
DIAGNOSTIC = re.compile(r"VUID-|Validation Error|(?:ERROR|SUMMARY): (?:Address|Leak)Sanitizer")


def memory_summary(samples, warmup):
    steady = samples[warmup:]
    if len(steady) < 4 or any(v is None for v in steady):
        return {"rss_first_window_bytes": None, "rss_last_window_bytes": None, "rss_growth_bytes": None}
    window = min(10, len(steady)//2)
    first, last = statistics.median(steady[:window]), statistics.median(steady[-window:])
    return {"rss_first_window_bytes": first, "rss_last_window_bytes": last,
            "rss_growth_bytes": last-first}


def run(command, cwd, output, cycles=4, timeout=1200, warmup=5,
        require_memory=False, max_growth_mib=None):
    if not 1 <= cycles <= 1000 or not math.isfinite(timeout) or timeout <= 0 or warmup < 0:
        raise ValueError("Invalid cycle/timeout/warmup limits")
    if max_growth_mib is not None and (not math.isfinite(max_growth_mib) or max_growth_mib < 0 or cycles < warmup+4):
        raise ValueError("A growth gate needs a nonnegative limit and at least four post-warmup cycles")
    output = Path(output).resolve(); output.mkdir(parents=True, exist_ok=True)
    log_path = output / ("lifecycle-"+uuid.uuid4().hex[:8]+".log")
    result = {"status": "failed", "cycles_requested": cycles, "cycles_completed": 0,
              "rss_checkpoints_bytes": [], "peak_sampled_rss_bytes": None,
              "warmup_cycles": warmup, "log": str(log_path), "error": None}
    env = dict(os.environ, CGL_SOAK_CYCLES=str(cycles), CGL_SOAK_HANDSHAKE="1")
    proc = None; reader = None; stop = threading.Event()
    events = queue.Queue(maxsize=8); diagnostic = threading.Event(); passed = threading.Event(); read_errors = []
    def emit(value):
        while not stop.is_set():
            try: events.put(value, timeout=.1); return
            except queue.Full: pass
    try:
        try: import psutil
        except ImportError:
            if require_memory or max_growth_mib is not None: raise RuntimeError("Install psutil for required RSS measurement")
            psutil = None
        proc = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, encoding="utf-8", errors="replace", bufsize=1)
        monitor = psutil.Process(proc.pid) if psutil else None
        def rss():
            if monitor is None: return None
            try: value = monitor.memory_info().rss
            except psutil.NoSuchProcess: return None
            result["peak_sampled_rss_bytes"] = max(result["peak_sampled_rss_bytes"] or 0, value)
            return value
        def read():
            try:
                with log_path.open("w", encoding="utf-8") as log:
                    for line in proc.stdout:
                        log.write(line)
                        if DIAGNOSTIC.search(line): diagnostic.set()
                        if "[       OK ] "+TEST_NAME in line: passed.set()
                        if line.startswith(PREFIX): emit(line.rstrip())
            except Exception as error: read_errors.append(str(error))
            finally: emit(None)
        reader = threading.Thread(target=read, daemon=True); reader.start()
        deadline = time.monotonic()+timeout
        while True:
            if time.monotonic() >= deadline: raise TimeoutError("Lifecycle process timed out")
            rss()
            try: event = events.get(timeout=min(.1, max(.001, deadline-time.monotonic())))
            except queue.Empty: continue
            if event is None: break
            expected = result["cycles_completed"]+1
            if event != PREFIX+str(expected) or expected > cycles:
                raise ValueError("Missing, duplicate or out-of-order lifecycle checkpoint")
            value = rss()
            if monitor is not None and value is None: raise RuntimeError("Process exited before quiescent RSS sample")
            result["rss_checkpoints_bytes"].append(value)
            result["cycles_completed"] = expected
            proc.stdin.write("continue\n"); proc.stdin.flush()
        if read_errors: raise RuntimeError("Native log reader failed: "+read_errors[0])
        code = proc.wait(timeout=max(.001, deadline-time.monotonic()))
        if code != 0: raise RuntimeError(f"Native lifecycle test exited {code}")
        if result["cycles_completed"] != cycles or not passed.is_set():
            raise ValueError("Native lifecycle test did not finish every requested cycle")
        if diagnostic.is_set(): raise ValueError("Vulkan validation or sanitizer diagnostic in native log")
        result.update(memory_summary(result["rss_checkpoints_bytes"], warmup))
        if max_growth_mib is not None:
            if result["rss_growth_bytes"] is None: raise ValueError("Insufficient RSS samples for growth gate")
            if result["rss_growth_bytes"] > max_growth_mib*1024*1024: raise ValueError("RSS growth exceeds configured gate")
        result["status"] = "complete"
    except Exception as error:
        result["error"] = str(error)
    finally:
        if proc is not None:
            if proc.poll() is None:
                proc.terminate()
                try: proc.wait(timeout=5)
                except subprocess.TimeoutExpired: proc.kill(); proc.wait(timeout=5)
            stop.set()
            if reader is not None: reader.join(timeout=5)
            for pipe in (proc.stdin, proc.stdout):
                if pipe is not None:
                    try: pipe.close()
                    except OSError: pass  # A failed acknowledgement may fail again while closing.
        result.setdefault("rss_growth_bytes", None)
        (output/"summary.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-exe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cycles", type=int, default=os.environ.get("CGL_SOAK_CYCLES", "4"))
    parser.add_argument("--timeout", type=float, default=1200)
    parser.add_argument("--warmup-cycles", type=int, default=os.environ.get("CGL_SOAK_WARMUP_CYCLES", "5"))
    parser.add_argument("--require-memory", action="store_true")
    parser.add_argument("--max-growth-mib", type=float)
    args = parser.parse_args(); exe = args.test_exe.resolve()
    if not exe.is_file(): parser.error("Native test executable does not exist")
    try:
        result = run([str(exe), "--gtest_filter="+TEST_NAME], exe.parent, args.output,
                     args.cycles, args.timeout, args.warmup_cycles, args.require_memory, args.max_growth_mib)
    except ValueError as error: parser.error(str(error))
    print(json.dumps(result), flush=True)
    return 0 if result["status"] == "complete" else 1


if __name__ == "__main__":
    raise SystemExit(main())
