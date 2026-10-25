# Progressive map loading validation

Base: develop `84dfba4` (PR #84). Ordinary native map scenes now prepare the map
on a CPU worker and upload atlas pages in bounded chunks. The synchronous library
API remains available for fixed preloaded crowd benchmarks and explicit tools.
Existing resource formats, placement, footprints and complete-map publication
remain unchanged. This does not introduce a new map format or a network server.

## Scheduling and lifetime

One engine-owned `MapDecodeWorker` has one occupied slot, including completed
output awaiting collection. Metadata planning uses zero RGBA reservation but is
still busy. Page decode reserves its exact size first, at most 32 MiB. Running
cancellation retains the reservation until CPU access finishes. There is no job
queue, per-actor worker or page lookahead.

The builder checks cooperative cancellation between layout cells, image queries,
metadata pages and RGBA rows. Reader-lock acquisition retries with cancellation
checks instead of blocking exit behind another reader user. Individual file read,
decoder and atlas-sort calls are not forcibly interrupted; exit can wait for such
a call to return. Reader calls stay serialized by the existing query mutex.

`MapLibrary::acquireProgressive` transfers the sole completed CPU page into the
existing pending sprite uploader. Creation starts at most one 4 MiB chunk;
subsequent owner-thread calls poll and can submit at most one next chunk. The
ordinary scene calls it once per update; initial onEnter only starts metadata,
so the same-frame update cannot double-submit an upload. A completed upload poll
never starts another page in that call. Callers must honor the once-per-frame
polling contract; the API does not deduplicate arbitrary caller loops by frame ID.

No next CPU page starts while the previous page uploads. Page RGBA working
reservation is therefore at most 32 MiB, with at most 4 MiB GPU staging. Completed
partial atlas pages still consume GPU payload, up to the existing 256 MiB map
limit. Layout/metadata, decoded source images, reader caches, animation work,
ready/retired maps, allocator and driver overhead are extra. These limits are
not a total physical VRAM or process-memory budget.

Only the owner thread touches GPU objects and submits graphics work. The cache
publishes a map only after all pages reach Ready. Partial pages have never been
drawn: cancellation first ends CPU access to borrowed plan/readers, then releases
partial pages and the uploader, whose destructor waits for its own submitted
chunk. Published maps still use the existing keyed retirement and actual frame
completion serials. No submitted world resource is freed early to fund a load.

Changing a pending map request requires explicit cancellation. A synchronous
cache miss during progressive work is rejected rather than duplicating work.
Cache/retired hits preserve resource reuse. Errors clear the slot and partial
resources before propagation. Scene exit/restart invokes cancellation; final
library clear also stops/joins the idle thread before reader/device teardown.

## Scene behavior and measurement

Before readiness, the native render loop clears/presents and processes window
events; R restarts loading and closing exits normally. The window title reports
metadata or completed/total pages, updated only when progress changes. Camera,
hover and world entities are installed after map completion. Normal movement and
remote appearance streaming then follow their existing paths.

Fixed crowd benchmarks retain synchronous preloading to keep their measurement
workload comparable. The initial local player's existing 16-clip acquisition, hover creation and
server-demo catalog setup still occur during scene entry; they are not governed by the map
copy limit. Driver allocation/pipeline creation can also take time. A byte cap is
not a guarantee of a particular frame duration or entirely hitch-free startup.

Telemetry reports current `map_loading`, `map_cpu_pending`, `map_upload_pending`,
`map_pending_bytes` and `map_working_bytes`. Peak counters retain planned working
and partial-map payload until clear; they are not additional live allocations.
`map_owner_call_max_ms` is the maximum wall time of one progressive acquire call,
including initial GPU creation/polling and any error cleanup.

`map_loading_samples`, `map_loading_fps`, `map_loading_frame_p99_ms`,
`map_loading_frame_max_ms` and `map_loading_update_mean_ms` are separate CPU
loading-frame telemetry. The sample window is bounded at 512; lifetime count,
FPS and maximum remain lifetime statistics. Scope includes the final scene-entry
update/local-player setup, but excludes initial engine asset-reader initialization.
Loading a blank world is not equivalent to gameplay FPS. Ordinary gameplay
telemetry retains its ready-world scope. GPU results retain the existing
ready-scene submission filter, decided after the update, whereas CPU gameplay
classification starts before it. Their first-ready boundaries and delayed
results need not give identical counts. Loading-frame measurements here are CPU
wall times, not a separate GPU loading-time profile.

`--frames` continues counting rendered loop iterations including loading frames;
a very small frame limit can close before asynchronous map readiness.

## Tests and reproduction

The SDK-free registry contains seven native suites (60 cases) plus two Python
runner cases. New worker tests cover retained completed output, zero-byte busy
metadata, running cancellation, reservation retention, error/oversized-output
recovery, thread stop/reuse and captured-owner release. Builder tests exercise
cancellation before reads and after a decoded source image returns.

Native Vulkan tests cancel while a reader lock is held, cancel after a chunk is
submitted, reuse the slot, enforce working/partial payload bounds, validate the
complete two-page map, inject a second-page failure, and leave a loading scene.
Existing tests now wait for actual asynchronous map readiness where appropriate;
budget waits and frame-fence retirement assertions remain intact. Lifecycle soak
also checks zero pending/working map payload and CPU/upload activity after exit.

```powershell
# x64 Visual Studio developer environment on Windows; installed assets required.
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --preset asan
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error

.\build\dev\bin\cgl-client.exe --settings .\build\dev\bin\server-demo100.ini
```

Set General.EngineRootPath to the installed resources. This demo disables VSync;
WASD moves with a centered player, N replaces the roster and R restarts. During
initial loading the title advances and the window remains responsive.

The SDK-free project and hosted Windows/Linux ordinary/ASan CI use the same
production worker/builder sources. Desktop validation uses the Radeon 880M
integrated GPU, not dedicated 4/8 GiB hardware. MSVC ASan is not LeakSanitizer;
hosted Linux core ASan enables leak detection. Process RSS is not physical VRAM.

## Measured results

Measured 2026-10-25 on the development PC:

- Full ordinary CTest **310/310**, full MSVC ASan **309/309**, SDK-free ordinary
  **62/62**. The ordinary-only Python binding test accounts for the difference.
  The final progress/timing/title changes also pass eight focused native tests
  covering cancellation, fences, budget waits, partial entry and map startup.
- A validation-enabled **100/100-cycle** lifecycle run passes every zero-owner,
  pending-payload and cancellation invariant. With 20 warmup cycles, first/last
  ten-sample median RSS rises 273,686,528 -> 286,507,008 bytes (+12.23 MiB).
- A fresh mature **300/300-cycle** run, with 100 warmup cycles and a selected
  64 MiB diagnostic growth gate, passes. First/last post-warmup medians are
  280,895,488 -> 270,096,384 bytes (-10.30 MiB); peak sampled RSS is 330,297,344.
  This run does not show continued growth in those windows. These observations
  do not prove every allocation leak-free, attribute allocator/driver retention,
  or define a new engine memory cap.

Two final 6,000-frame map-100 workloads use 512 visible/512 outer actors, a
2,048-appearance catalog and replacement every 1,200 frames. Both use IMMEDIATE
presentation (VSync disabled) and fingerprint `835595881056024373`.

| Animation budget | Gameplay lifetime FPS | Gameplay rolling p99 ms | True-ready coverage | Map loading samples |
| --- | ---: | ---: | ---: | ---: |
| 64 MiB | 427.196 | 5.0096 | 21.9322% | 153 |
| 512 MiB | 408.250 | 5.4750 | 78.1445% | 160 |

Gameplay lifetime maximum frame durations are 21.1170 / 21.3346 ms. Both runs
finish without failed appearances, missing GPU timing results or pending map
work; each collects 5,999 ready-scene GPU results. Animation accounting stays
within 67,108,864 / 536,870,912-byte limits. Map working reservation peaks at
33,488,928 bytes, below 32 MiB, and partial-map payload peaks at 40,330,752 bytes
(the same complete two-page atlas payload as the synchronous map).

Loading CPU p99 is 11.1203 / 9.8333 ms, but lifetime maximum loading frames are
**259.272 / 232.532 ms**. Maximum single progressive map calls are only
10.4669 / 9.1207 ms. The remaining whole-frame peak has not been precisely
attributed: local-character acquisition, server catalog/world setup and other
GPU/OS waits remain separate profiling targets. This PR bounds map preparation
and copy work; it does **not** claim all scene entry is hitch-free. Blank loading
FPS (371.081 / 443.483) is not full-world rendering throughput.

There is no paired before/after FPS improvement claim. Resource readiness changes
the drawn workload, and fixed frame counts provide different elapsed loading
time at different FPS. Dedicated 4/8 GiB GPU behavior remains unverified.

```powershell
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/progressive-map-final-streaming
python tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 300 --warmup-cycles 100 --require-memory --max-growth-mib 64 --output build/progressive-map-mature-soak
```

Complete positive logs have no VUID, Vulkan validation-error or sanitizer error
markers. Ignored evidence is under `build/progressive-map-final-streaming/`,
`build/progressive-map-{soak,mature-soak}/`, `build/progressive-map-tests.log`,
`build/progressive-map-asan-tests.log`, `build/progressive-map-core-tests.log` and
`build/progressive-map-final-check-tests.log`. No game files, executable or raw
logs are committed.
