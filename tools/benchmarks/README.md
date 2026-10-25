# Native crowd benchmark

Build `cgl-client` with CMake in RelWithDebInfo or Release. Run from the repository root (use a fresh output directory):

```powershell
python tools/benchmarks/crowd_benchmark.py --exe build/pr2-clean/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/crowd-run --densities 1,5,10 --unique 512 --budget-mib 512 --repeats 3
python tools/benchmarks/crowd_benchmark.py --exe build/pr2-clean/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/crowd-1024 --densities 10 --unique 1024 --budget-mib 512 --repeats 3
```

Python uses the standard library; optional `psutil` adds peak process RSS sampling. Map defaults to `map/0/2100.dat`. Add `--width 1920 --height 1080` for the larger viewport, or `--switch-motions --budget-mib 1024` to preload and switch idle/walk together every second. Each run warms up 120 frames and records 600 frames by default. The client exits automatically. Reports include frame samples, resource identities, stage means, frame percentiles, loading time and draw counts. The runner rejects incomplete runs and mismatched resource/actor counts; it preserves logs and INIs.

VSync and validation are disabled by the runner, with no frame limiter. It requires actual IMMEDIATE presentation for an uncapped result. Explicit `--vsync` / `--validation` enable diagnostic comparisons. Normal client defaults remain VSync=true and Validation=true. The Vulkan fallback when IMMEDIATE is unavailable is MAILBOX then FIFO; these remain synchronized and are not accepted as uncapped samples. See [Vulkan present modes](https://docs.vulkan.org/refpages/latest/refpages/source/VkPresentModeKHR.html).

`Benchmark.ActorsPerCell` places 1?10 actors on every grid whose center is inside the viewport. An explicit `ActorCount` overrides density and may leave cells empty; limits are 20,000 actors and 10 actors per cell. Default zero disables the benchmark. Resource selection is deterministic across installed versions, requires eight-direction idle/walk descriptions and valid sprite metadata, and fails when the corpus or configured budget is insufficient. Different version/serial identities can still share artwork. The report records the selected identities. Tests render one facing; only idle, or idle+walk with switching, is resident. This is not a measurement with all eight directions resident.

## Measurements (2026-10-20)

AMD Radeon 880M integrated graphics, Windows, RelWithDebInfo, map 2100, IMMEDIATE, validation off. Three runs per row; FPS is the median and p99 is the worst run's p99. Frame timing is CPU-observed loop duration including GPU backpressure, not GPU timestamp queries or displayed FPS. Loading and warmup are excluded.

| Viewport | Actors | Resource identities | Mode | FPS | p99 ms |
| --- | ---: | ---: | --- | ---: | ---: |
| 1280x720 | 605 | 512 | idle | 589.9 | 3.52 |
| 1280x720 | 3025 | 512 | idle | 389.3 | 4.26 |
| 1280x720 | 6050 | 512 | idle | 237.9 | 6.16 |
| 1280x720 | 6050 | 1024 | idle | 219.5 | 7.42 |
| 1280x720 | 6050 | 1024 | idle/walk switching | 232.3 | 6.15 |
| 1920x1080 | 13270 | 1024 | idle | 98.0 | 14.65 |

No sampled frame exceeded 33.33 ms in these 18 final runs; the largest frame was 18.55 ms. This does not guarantee future workloads will never stall. The matched 6050/1024 control, using the previous sorter and ECS query with identical resource identities, measured 102.3 FPS / 15.78 ms p99. Avoiding point-to-point footprint comparisons and querying the smallest dense ECS storage raised that to 219.5 FPS / 7.42 ms p99. Removing only redundant sort comparisons measured 209.6 FPS. Animation differences and measurement noise mean switching FPS should not be interpreted as intrinsically faster.

1024 idle clips contain 324.9 MiB RGBA payload; idle+walk contains 480.7 MiB. Payload and process RSS are not total VRAM: map textures, allocation padding, pipelines, descriptors, staging and driver allocations are excluded. This machine is not a 4/8 GB dedicated GPU, so capacity on those cards remains unverified.

Scene preload still takes roughly 2.4?2.6 seconds for 512 idle resources, 4.8?5.3 seconds for 1024, and 9.2?10.0 seconds for idle+walk. Reader initialization precedes that measurement. Steady-state animation switches perform no uploads. Dynamic streaming/upload hitches are not solved here: shared pipelines, batched uploads, bounded asynchronous streaming and actual GPU memory telemetry remain follow-up work. AI, networking, pathfinding and collision are outside this benchmark.

## Verification

145 regression tests passed excluding the existing common_env_test and trace_logger_test suites. The full run was 147/151 passing: two existing env tests expect default Info while the implementation defaults to Trace; two existing logger tests fail output-format assertions. Their implementation and test files are unchanged by this PR. 64 targeted AddressSanitizer tests passed, plus a validation-enabled native crowd run under ASan. Windows MSVC ASan checks invalid memory access, not exhaustive leak detection. Scene restart/shutdown tests explicitly verify entity counts and weak clip references before/after cache clearing. A 6050-actor validation-enabled run produced no new validation errors; the loader still reports the machine's two missing Steam layer JSON files. Invalid benchmark settings and an unwritable report path returned failure with normal engine shutdown.

## Cold characters during playback

Add `--load-during-playback --samples 1200` for 1024 resources. The scene begins with one clip and uses it as a placeholder for not-yet-ready appearances. After warmup, one new appearance is decoded/uploaded on each frame, in a deterministic shuffled order. Idle+walk can be combined with `--switch-motions`; allow enough samples for every requested resource and a motion switch. `cold_load_count`, `cold_load_p99_ms` and `cold_load_max_ms` measure the entire synchronous load/publication step, not GPU transfer time alone. The runner rejects unfinished coverage; the client reports incomplete if resources remain at the end. Start a fresh process to avoid testing cache hits. The selected manifest is recorded, and all identities must have loaded before success.

This is a diagnostic with a stationary camera, not production streaming: it deliberately retains synchronous pixel decoding, per-texture allocation and `vkQueueWaitIdle` to expose remaining stalls. Metadata enumeration occurs at entry and is excluded from sampled frame times. Pending actors use the initial clip; the test does not add network actors or exercise camera-driven request cancellation. `shared_pipeline_creations` counts successful shared-bundle creations reported by the backend. Textures/descriptors are separate; sharing the pipeline does not batch draw calls or uploads.

### Shared pipeline measurements (2026-10-21)

Matched harness with only the sprite implementation changed, map2100, 6050 actors,
1024 resource identities, Radeon 880M, IMMEDIATE, validation off. All matched
resource manifests and submitted actor counts were verified. The old implementation
creates a pipeline/sampler/layout per texture (1024 idle clips + map + outline);
the shared backend logs exactly one bundle creation per run. This is structural
resource reduction, not a promise of higher FPS or lower total VRAM.

Three interleaved preload runs measured median scene load 4909.68 ms (control) vs
4687.67 ms (shared), median FPS 197.3 vs 207.2. Worst-run p99 was 9.88 vs 11.17 ms,
so frame latency did not consistently improve. An earlier grouped shared series
ranged 165.1?246.5 FPS and 4.76?6.13 seconds loading, illustrating environmental
variance; it must not be presented as a robust speedup. Process RSS also overlapped.

Cold-mode runs used 120 warmup + 1200 sample frames, with 1023 new identities
loaded during sampling, three fresh processes per variant. Control median FPS was
83.5 (worst-run p99 25.58 ms, max 56.45 ms); shared median was 87.7 (p99 24.97 ms,
max 63.22 ms). All requested resources completed; no substitution counted as
success. Synchronous cold-load step p99 remained about 18?19 ms, with max 48.1 ms.
Long frames remain and bounded asynchronous decoding/upload is still required.
Raw JSON reports and binary hashes are retained under the ignored
`build/shared-results` directory in the development worktree.

Vulkan pipeline reuse follows the fixed shader/render-state association described
in the [Khronos pipeline management sample](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_cache/README.html).
This implementation shares live pipeline objects within a pass, not a disk pipeline cache.

Verification for this change: 158 regression tests passed (existing env/logger
suites excluded), 17 targeted tests also passed under MSVC ASan before the final pass-retirement cleanup hook, including texture
churn, final shared-bundle ownership, cold clip publication, early exit and map100.
Validation-enabled ASan cold idle/walk playback completed all 10 resources and
four motion switches. With a forced 16 MiB budget, cold loading failed after 108
resources / 107 playback loads, returned scene_error and shut down normally,
without a new Vulkan validation or ASan error. Windows ASan is not exhaustive leak
detection; explicit weak-reference/lifecycle assertions check resource release.

After the final cleanup hook, the normal regression suite was rerun; 10 ASan
sprite/crowd tests passed. Windows Application Control blocked the rebuilt ASan
scene-lifecycle test executable (WinError 4551), so that final ASan scene-suite
rerun could not complete. No security settings were changed. The native ASan
client remains executable and separately exercises cold loading and pass teardown.

## Fenced progressive uploads

Add `--load-during-playback --progressive-uploads --samples 6000` for 1024
resources. Small clips need a submission frame and a later completion poll; larger
clips span more frames. Allow enough frames for full coverage; incomplete runs are
rejected. Without `--progressive-uploads`, cold loads still wait synchronously and
provide a same-binary control. Both paths use upload-specific fences rather than
queue-wide idle. The default preload mode remains synchronous.

The progressive path has one pending clip, at most 32 MiB decoded RGBA and 4 MiB
copy/staging per update. It is a stationary-camera diagnostic, not full NPC streaming.
Cold work metrics now accumulate CPU time spent in all request/poll/publication
calls for one appearance; they do not measure GPU time. `cold_ready_p99_ms` and
`cold_ready_max_ms` measure elapsed time until that appearance is usable, including
intervening frames. `cold_frame_count/p99_ms/max_ms` isolate frames while appearances
are still arriving, so a long steady-state tail cannot hide cold-load stalls.
A smaller source-data cap of 16 MiB was rejected during validation because a legal
114-frame clip needs about 24 MiB. The revised cap is 32 MiB; the resident budget
still includes pending destination payload. Memory bounds exclude driver padding,
map textures, ready resources and reader scratch. No hard CPU-millisecond bound or
absence of stalls is promised while decode/allocation remain synchronous.

### Fenced upload results (2026-10-21)

Radeon 880M, map2100, 1280x720, 6050 actors, IMMEDIATE, validation off. Three
interleaved fresh-process repeats per variant, 120 warmup + 2000 sample frames,
512 identities. Manifests, submitted actor counts and 511 completed cold loads
match; the cold phase includes every load after fixing an off-by-one warmup issue.

| Metric | Synchronous fence control | Progressive |
| --- | ---: | ---: |
| Median overall FPS | 174.8 | 185.5 |
| Worst-run cold-phase p99 ms | 16.33 | 11.36 |
| Largest sampled frame ms | 32.62 | 34.24 |
| Worst-run appearance-ready p99 ms | 10.46 | 19.38 |
| Maximum appearance-ready latency ms | 26.51 | 56.29 |

Both variants retain all actors, but placeholders persist longer in the progressive
case, changing the timing of visible artwork. Overall FPS includes a steady-state
tail and is not an isolated transfer benchmark. Better cold-phase p99 comes with
later appearance readiness; the worst frame did not improve consistently. No
no-stall guarantee is made.

A final 1024-identity progressive run (120 warmup + 3000 samples) completed all 1023
cold loads, including the 24 MiB clip. It measured 168.7 FPS overall, 2073 cold-phase
frames, cold p99 11.73 ms, max frame 34.17 ms, ready p99 21.17 ms and max ready
latency 70.42 ms. This is one coverage run, not a three-repeat capacity guarantee.
Raw reports/hashes are under ignored `build/fenced-final-results`; exploratory
16 MiB-cap failures are separate under `build/fenced-results`.

Validation: full build and 161 regression tests passed (existing env/logger suites
excluded); 20 targeted ASan tests passed, including row/layer split readback,
pending-draw suppression, cancellation, shared ownership, repeated scene restart
and progressive request de-duplication. A validation-enabled native ASan map100
idle/walk run completed all 10 resources. Forced 16 MiB resident-budget exhaustion
failed after 108 ready resources / 107 cold loads with scene_error and normal
shutdown. Ending after the first pending upload returned incomplete and closed
normally. No new Vulkan validation/ASan errors were found in these native runs;
the machine's missing Steam layer JSON messages remain. MSVC ASan is not exhaustive
leak detection; explicit ownership/weak-reference tests complement it.

Synchronization and copy-region rules follow [Vulkan fences](https://docs.vulkan.org/spec/latest/chapters/synchronization.html)
and [VkBufferImageCopy](https://docs.vulkan.org/refpages/latest/refpages/source/VkBufferImageCopy.html).

## Background decode comparison

Add `--background-decode` to `--load-during-playback --progressive-uploads`.
For example, use `--unique 1024 --samples 6000 --repeats 1`. Without the new flag,
this diagnostic explicitly selects the main-thread decoder as a same-binary control.
The C++ progressive API defaults to background decoding; normal preload behavior
and benchmark defaults are unchanged. `background_decode` is recorded in JSON.
The background job, its completed result and the pending upload share one 32 MiB
reservation. This means readiness takes additional polling frames; incomplete
coverage remains a failure, and placeholders do not count as loaded resources.
`cold_load_*` measures owner-thread request/poll/publication CPU work only; background
CPU work is excluded. `cold_ready_*` measures elapsed user-visible readiness latency,
and cold-phase frame samples include time while the worker is running. GPU allocation
and driver work still happen on the owner and may cause long frames. Scene-entry
metadata enumeration is still outside sampled frames, and the camera is stationary.

### Background decode results (2026-10-21)

Radeon 880M integrated GPU, map2100, 1280x720, 6050 actors, IMMEDIATE, validation
off, same-binary progressive uploads in both cases. Three interleaved fresh-process
repeats, 512 identities, 120 warmup + 3000 samples; all 511 cold appearances finished
with matching manifests and submitted actor counts.

| Metric | Main-thread decode | Background decode |
| --- | ---: | ---: |
| Median overall FPS | 213.49 | 212.08 |
| Worst-run cold-phase p99 ms | 12.14 | 9.44 |
| Largest sampled frame ms | 25.03 | 25.74 |
| Worst-run ready p99 ms | 22.88 | 22.06 |
| Worst-run owner work p99 ms | 16.21 | 7.72 |

FPS and worst-frame timing did not consistently improve; placeholder/artwork timing
and the longer cold phase also affect the comparison. Background mode uses an
additional decode-completion polling frame and can delay appearance readiness.

Matched 1024-identity coverage (one run per mode, 120 warmup + 6000 samples) completed
all 1023 cold appearances, including the 24 MiB clip. Main vs background: cold p99
11.91 vs 8.72 ms, max frame 34.67 vs 22.33 ms, ready p99 25.13 vs 26.54 ms, max ready
86.42 vs 79.46 ms. Overall FPS was 212.31 vs 215.62. There were 2 vs 0 frames above
33.33 ms. These are coverage observations, not a repeated worst-case guarantee or
a 4/8 GB dedicated-card capacity test. Raw reports/hashes: ignored build/decode-results.

Verification before the final retained-capacity guard: full build and 165 regression
tests passed excluding existing env/logger suites; 20 targeted MSVC ASan tests passed. The three CPU worker tests each passed
20 additional ASan repetitions (60 runs, including 160 gated active cancellations),
covering slot saturation, persistent-thread reuse, cancellation/reference release,
stop/restart, exceptions and result bounds. The strengthened scene-exit and library
clear/reader-identity tests passed in normal and ASan builds after a warm cache was
expanded to force new uncached work. CPU-only builder cancellation is also covered.

Native ASan + Vulkan validation map100 background idle/walk playback completed all
10 resources. Forced budget exhaustion transferred a worker error after 108 ready
resources / 107 cold appearances, exited with scene_error and closed normally.
Closing after the first background request produced incomplete and normal shutdown.
No new validation/ASan errors occurred; missing Steam layer JSON messages remain.
MSVC ASan is not a race detector or exhaustive leak detector. The gated concurrency,
reader locking and explicit lifetime assertions complement it; ThreadSanitizer was
not run on this Windows host.

After adding the final guard against an oversized retained vector capacity (not just
its size), the normal build succeeded and 155 tests passed, including all three
worker tests and the new capacity-only overrun case. Windows Application Control
blocked the final normal and ASan cgl-client executables and the ASan worker test
(WinError 4551). The 10 native client startup cases therefore could not run on that
final build; ASan worker discovery/reruns were also unavailable. No security policy
was changed. Performance/native observations above are from the earlier passing
checkpoint; the final guard does not change valid builder pixel contents, but that
does not substitute for a final executable startup run on this host.

The final available ASan scene/builder subset passed 17/17, including strengthened
uncached-scene cancellation and the background library path. The final guard's
oversized-capacity rejection was verified by the normal CPU worker tests; no final
ASan worker result is claimed.

## Optional GPU frame timing

Add `--gpu-timing` to enable the existing `[Telemetry] Enabled=true` diagnostics.
Without it, no timestamp query pools are created. For example:

```powershell
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/gpu-crowd --densities 10 --unique 1024 --repeats 3 --gpu-timing
```

Reports add `gpu_mean_ms`, `gpu_p95_ms`, `gpu_max_ms`, supported command-buffer
slots and valid/missing sample counts. Unsupported queues have no GPU samples;
GPU durations are JSON `null`, not zero. The runner accepts unsupported devices,
but rejects an enabled, supported run with no completed timing samples. Use enough
frames to reuse a submitted slot (the default warmup/sample lengths do).
`cpu_fence_wait_mean_ms` measures the existing frame-slot fence wait;
`cpu_record_mean_ms` covers reset and command recording, including world sorting.
These phase timings require telemetry and are null when disabled.

GPU timestamps bracket graphics command execution, excluding presentation and
host-side work. They are not GPU utilization or displayed FPS. Results are read
only after the existing fence completes, without an added GPU wait. CPU and GPU
windows independently retain at most 512 samples. GPU results arrive on slot
reuse; the last submitted slots are not flushed for reporting. The GPU window
can include warmup if fewer than 512 subsequent samples exist. Instrumentation
has a cost; compare disabled/enabled runs. See
[GPU timing validation](../../docs/gpu-frame-timing-validation.md) for measurements
and lifecycle/clock limitations.
