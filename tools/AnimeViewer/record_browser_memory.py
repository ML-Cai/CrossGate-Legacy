"""Read-only browser process-tree RSS sampling during manual Resource Preview use."""
import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import statistics
import time


BROWSER_NAMES = {"chrome", "chrome.exe", "chromium", "chromium-browser", "google chrome",
                 "msedge", "msedge.exe", "firefox", "firefox.exe"}


class BrowserTree:
    def __init__(self, pid, psutil):
        self.api = psutil
        root = psutil.Process(pid)
        self.name = root.name()
        if self.name.lower() not in BROWSER_NAMES:
            raise ValueError("PID must identify a Chrome/Chromium, Edge or Firefox browser main process")
        parent = root.parent()
        if parent is not None and parent.name().lower() in BROWSER_NAMES:
            raise ValueError("PID identifies a browser child; select the browser main process PID")
        self.pid, self.created = root.pid, root.create_time()

    def root(self):
        root = self.api.Process(self.pid)
        if root.create_time() != self.created or root.name() != self.name:
            raise LookupError("Browser main process identity changed")
        return root

    def sample(self):
        result = dict(rss_sum_bytes=None, root_rss_bytes=None, process_count=None,
                      sampled_process_count=0, unavailable_processes=0, complete=False,
                      root_state="running")
        try:
            root = self.root()
            processes = {p.pid: p for p in [root, *root.children(recursive=True)]}
            result["process_count"] = len(processes)
            rss = 0
            for process in processes.values():
                try:
                    # psutil's identity check catches children whose PID was reused after discovery.
                    if not process.is_running():
                        raise self.api.NoSuchProcess(process.pid)
                    value = process.memory_info().rss
                    if not process.is_running():
                        raise self.api.NoSuchProcess(process.pid)
                    rss += value
                    result["sampled_process_count"] += 1
                    if process.pid == self.pid:
                        result["root_rss_bytes"] = value
                except (self.api.NoSuchProcess, self.api.AccessDenied):
                    result["unavailable_processes"] += 1
            self.root()  # Do not publish a total if the root exited/changed during collection.
            result["complete"] = not result["unavailable_processes"]
            if result["complete"]:
                result["rss_sum_bytes"] = rss
        except self.api.NoSuchProcess:
            result["root_state"] = "exited"
        except self.api.AccessDenied:
            result["root_state"] = "access_denied"
        except LookupError:
            result["root_state"] = "identity_changed"
        return result


def summarize(samples, warmup, window):
    values = [s["rss_sum_bytes"] for s in samples if s["complete"]]
    steady = samples[warmup:]
    result = dict(complete_samples=len(values), incomplete_samples=len(samples)-len(values),
                  rss_sum_peak_sampled_bytes=max(values) if values else None,
                  rss_sum_first_window_median_bytes=None, rss_sum_last_window_median_bytes=None,
                  rss_sum_growth_bytes=None)
    if len(steady) >= 2*window and all(s["complete"] for s in steady):
        first = statistics.median(s["rss_sum_bytes"] for s in steady[:window])
        last = statistics.median(s["rss_sum_bytes"] for s in steady[-window:])
        result.update(rss_sum_first_window_median_bytes=first,
                      rss_sum_last_window_median_bytes=last, rss_sum_growth_bytes=last-first)
    return result


def record(tree, count=60, interval=1, warmup=10, window=5, label="manual",
           now=time.monotonic, sleep=time.sleep):
    if not 1 <= count <= 600 or not math.isfinite(interval) or not .1 <= interval <= 10:
        raise ValueError("samples must be 1..600 and interval must be finite, 0.1..10 seconds")
    if not 0 <= warmup < count or not 1 <= window <= 120:
        raise ValueError("warmup must be 0..samples-1 and window must be 1..120 samples")
    if len(label) > 80:
        raise ValueError("label must be at most 80 characters")
    started = now()
    started_utc = datetime.now(timezone.utc).isoformat()
    samples = []
    for index in range(count):
        if index:
            sleep(max(0, started + index*interval - now()))
        item = tree.sample()
        item["elapsed_s"] = round(now()-started, 3)
        samples.append(item)
        if item["root_state"] != "running":
            break
    return dict(schema_version=1, status="recorded" if all(s["complete"] for s in samples) else "incomplete",
                label=label, started_utc=started_utc,
                root_pid=tree.pid, root_create_time=tree.created, root_name=tree.name,
                requested_samples=count, interval_s=interval, warmup_samples=warmup, window_samples=window,
                scope="Root and discovered descendants; sum of resident sets (shared pages can be counted more than once). Not unique physical memory, JS heap, bitmap payload or VRAM. Other tabs/extensions in this browser tree are included. No browser interaction is performed.",
                samples=samples, **summarize(samples, warmup, window))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", required=True, type=int, help="Browser main process PID, not a renderer PID")
    parser.add_argument("--samples", type=int, default=60)
    parser.add_argument("--interval", type=float, default=1, help="Seconds between sample starts")
    parser.add_argument("--warmup", type=int, default=10, help="Initial samples excluded from window comparison")
    parser.add_argument("--window", type=int, default=5, help="Samples in each disjoint median window")
    parser.add_argument("--label", default="manual", help="Short test phase label; do not include private data")
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    try:
        import psutil
    except ImportError:
        parser.exit(1, "Install optional tools/AnimeViewer/requirements-memory.txt\n")
    try:
        report = record(BrowserTree(args.pid, psutil), args.samples, args.interval,
                        args.warmup, args.window, args.label)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
        print(f"Browser memory {report['status']}: {len(report['samples'])} samples, "
              f"{report['complete_samples']} complete; report: {args.report}")
        if report["status"] != "recorded":
            parser.exit(2)
    except (RuntimeError, ValueError, OSError, psutil.Error) as error:
        parser.exit(1, f"Browser memory recording failed: {error}\n")


if __name__ == "__main__":
    main()
