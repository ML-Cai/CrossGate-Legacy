"""Real resource matrix: all requested versions, streamed directions, HTTP PNG and cleanup."""
import argparse
import gc
import io
import json
from pathlib import Path
import statistics
import sys
import threading
import time
import urllib.error
import urllib.request
from urllib.parse import urlencode
import weakref

from preview import Preview, PNG_CACHE_BYTES
from web_viewer import PreviewServer


def memory_summary(samples, window):
    steady=samples[2*window:]
    result=dict(rss_samples_bytes=samples,rss_warmup_cycles=2*window,rss_peak_sampled_bytes=max(samples) if samples else None,
                rss_first_window_bytes=None,rss_last_window_bytes=None,rss_growth_bytes=None)
    if len(steady)>=2*window:
        first,last=statistics.median(steady[:window]),statistics.median(steady[-window:])
        result.update(rss_first_window_bytes=first,rss_last_window_bytes=last,rss_growth_bytes=last-first)
    return result


def verify(preview, versions, cycles=32, require_memory=False):
    """Own/close the supplied Preview. Compare transport against the shared CPU result."""
    import numpy as np
    from PIL import Image
    if not 1<=cycles<=1000:
        preview.close()
        raise ValueError("cycles must be 1..1000")
    report=dict(versions=[],groups=0,png_frames=0,plan_owner_checks=0,cache_peak_bytes=0,
                browser_visual="not_run",independent_renderer_comparison="not_run")
    server=thread=None
    targets=[];plan_refs=[];frame_refs=[];latencies=[];rss_samples=[];monitor=None
    try:
        try:
            import psutil
            monitor=psutil.Process()
        except ImportError:
            if require_memory:raise RuntimeError("Install optional psutil for required RSS measurement")
        server=PreviewServer(("127.0.0.1",0),preview)
        worker=threading.Thread(target=server.serve_forever);worker.start();thread=worker
        base=f"http://127.0.0.1:{server.server_port}"

        def get(path, expected=200):
            try:response=urllib.request.urlopen(base+path,timeout=30)
            except urllib.error.HTTPError as error:response=error
            with response:
                data=response.read()
                if response.status!=expected:
                    raise AssertionError(f"HTTP {response.status}, expected {expected}: {data[:300]!r}")
                return data

        get("/")
        for version,label in versions:
            row=dict(version=label,status="failed",samples=[])
            plan=expected=None
            try:
                catalog=json.loads(get("/api/catalog?"+urlencode(dict(version=version))))
                ids=catalog["serials"]
                serials=[sn for sn in (100000,101406) if version==0 and sn in ids] or [ids[0]]
                for serial in serials:
                    group=json.loads(get("/api/group?"+urlencode(dict(version=version,serial=serial))))
                    if any("error" in d for d in group["directions"]):
                        raise AssertionError(str([d for d in group["directions"] if "error" in d]))
                    row["samples"].append({k:group[k] for k in ("serial","motion","version")})
                    row["catalog_count"]=len(ids)
                    targets.append((version,serial,int(group["motion"])))
                    report["groups"]+=1
                    for meta in group["directions"]:
                        direction=meta["direction"]
                        plan=preview.plans[direction]
                        plan_refs.append(weakref.ref(plan))
                        if meta["clip_bytes"]>8*1024*1024:
                            row["oversized_clip_streamed"]=True
                        for index in range(meta["frame_count"]):
                            start=time.perf_counter()
                            encoded=get(f"/api/direction/{group['token']}/{direction}/{index}.png")
                            latencies.append((time.perf_counter()-start)*1000)
                            with preview.lock:
                                expected=preview.session.decode_animation_frame(plan,index,max_bytes=4*1024*1024)
                            frame_refs.append(weakref.ref(expected))
                            with io.BytesIO(encoded) as buffer,Image.open(buffer) as image:
                                np.testing.assert_array_equal(np.array(image.convert("RGBA")),expected)
                            del expected
                            report["png_frames"]+=1
                            if preview.png_bytes>PNG_CACHE_BYTES or len(preview.pngs)>24:
                                raise AssertionError("PNG cache exceeded bounds")
                            report["cache_peak_bytes"]=max(report["cache_peak_bytes"],preview.png_bytes)
                        del plan
                row["status"]="passed"
            except Exception as error:
                row["error"]=str(error)
            finally:
                plan=expected=None
            report["versions"].append(row)
        if not targets:
            raise AssertionError("No resources passed the requested version matrix")
        for i in range(cycles):
            version,serial,motion=targets[i%len(targets)]
            group=json.loads(get("/api/group?"+urlencode(dict(version=version,serial=serial,motion=motion))))
            for meta in group["directions"]:
                if "error" in meta:
                    raise AssertionError(meta["error"])
                plan_refs.append(weakref.ref(preview.plans[meta["direction"]]))
                get(f"/api/direction/{group['token']}/{meta['direction']}/0.png")
            get("/api/group?serial=-1",400)
            get("/api/catalog?version=99",400)
            # An invalid request must not poison the currently selected group.
            get(f"/api/direction/{group['token']}/{group['directions'][0]['direction']}/0.png")
            if monitor:rss_samples.append(monitor.memory_info().rss)
    finally:
        try:
            if thread is not None:server.shutdown()
            if server is not None:server.server_close()
            if thread is not None:
                thread.join(timeout=5)
                if thread.is_alive():raise AssertionError("HTTP server did not stop")
        finally:preview.close()
    gc.collect()
    if any(ref() is not None for ref in plan_refs+frame_refs):
        raise AssertionError("Plan/frame owners survived close")
    if preview.session.cache_stats()["readers"] or preview.png_bytes or preview.plans:
        raise AssertionError("Preview retained resources after close")
    report.update(cycles=cycles,plan_owner_checks=len(plan_refs),frame_owners_released=True,closed_readers=0,
                  http_png_median_ms=round(statistics.median(latencies),2),http_png_max_ms=round(max(latencies),2),
                  status="passed" if all(row["status"]=="passed" for row in report["versions"]) else "failed")
    report.update(memory_summary(rss_samples,len(targets)))
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",required=True,type=Path)
    parser.add_argument("--module-dir",type=Path,default=Path(__file__).resolve().parents[2]/"build/dev/bin")
    parser.add_argument("--versions",nargs="+")
    parser.add_argument("--cycles",type=int,default=32)
    parser.add_argument("--report",type=Path)
    parser.add_argument("--require-memory",action="store_true",help="Require optional psutil RSS samples")
    args=parser.parse_args()
    try:
        sys.path.insert(0,str(args.module_dir.resolve()))
        import cgl
        versions={v.name.replace("CG_VERSION_",""):v for v in cgl.CrossGateVersionList()}
        names=args.versions or list(versions)
        requested=[(int(versions[name]),name) for name in names]
        with cgl.Session(engine_root_path=str(args.root.resolve())) as session:
            preview=Preview(session,version=versions[names[0]])
            preview.version_options={int(v):v for v in versions.values()}
            report=verify(preview,requested,args.cycles,args.require_memory)
        text=json.dumps(report,indent=2)
        if args.report:
            args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(text+"\n",encoding="utf-8")
        print(text)
        if report["status"]!="passed":parser.exit(1)
    except Exception as error:parser.exit(1,f"Direction preview acceptance failed: {error}\n")

if __name__=="__main__":main()
