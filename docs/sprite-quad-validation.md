# Four-vertex sprite quad validation

Base: merged PR #77 (`ad46296`). The shared sprite pipeline now submits a
four-vertex triangle strip instead of a six-vertex triangle list. No batching,
new device feature, descriptor/texture allocation, buffer or resource policy is
introduced. The fragment shader, straight-alpha blending, painter order, push
constant layout, UV mapping and animation layer selection are unchanged.

## Geometry and lifecycle contract

Old list corners were `A B C, C B D`, where A=(0,0), B=(1,0), C=(0,1), D=(1,1).
The new strip submits `A B C D`, producing the same two triangles including strip
winding parity. Pipeline topology, shader corner array and native draw vertex
count change together. Primitive restart remains disabled. The draw is still
non-indexed with firstVertex=0 and one instance. No index/vertex buffer is added.
The topology is part of core Vulkan; see the official
[primitive topology reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkPrimitiveTopology.html).

Submitted vertices per sprite fall from 6 to 4 (33.3%), while triangle count and
draw-call count stay the same. This is a source-level submission count, not a
pipeline-statistics measurement of actual shader invocations or a promise of
33% less GPU time. For the fixed 6,833-draw workload it is 40,998 -> 27,332
submitted vertices and 13,666 triangles in either version. Other GPU work,
fragment overdraw, CPU recording and synchronization can dominate.

Pipeline/shader ownership and cleanup are unchanged. Existing shader modules are
released after pipeline creation; the shared pipeline remains owned by its render
pass/renderers and is released after the existing GPU-completion boundary. This
change adds no asynchronous work or allocation lifetime. It does not implement
instancing, transparent-region cropping or a new CPU metadata cache.

## Tests and reproduction

Two new actual-Vulkan readback tests exercise:

- A half-transparent quad over the entire target and an offset/scissored viewport
  with fractional position/scale. Every covered pixel must be blended once;
  overlapping triangles or a shared-diagonal hole would fail.
- A subregion inside a two-layer atlas, fractional position/scale, distinct texels
  on both triangles and alpha 1/64/128/192. This checks orientation, UVs, layer
  selection and faint/partial transparency against expected RGBA values.

Existing tests cover nearest scaling, zero alpha, opaque sprites, painter order,
renderer/atlas alternation, state invalidation, command-slot reuse, partial upload,
cancellation and pipeline ownership through device-child teardown.

Use an x64 Visual Studio developer shell (required ASan runtime environment):

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/quad-crowd --densities 10 --unique 1024 --repeats 3 --gpu-timing
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/quad-1080 --densities 10 --unique 1024 --width 1920 --height 1080 --repeats 1 --gpu-timing
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/quad-stream
```

Native comparisons run serially after builds/tests, with actual IMMEDIATE
presentation, VSync off and validation off. Both versions enable the same optional
CPU/GPU telemetry. GPU and CPU telemetry retain independent recent 512-sample
windows; benchmark FPS/percentiles use 600 measured frames after 120 warmup frames.
GPU intervals are not utilization or displayed FPS, and CPU/GPU times must not be
added. See [GPU timing semantics](gpu-frame-timing-validation.md).

## Final measurements

Windows, RelWithDebInfo, AMD Radeon 880M integrated graphics. This is not a
4/8 GiB dedicated-GPU capacity test. The source-identical merged #77 binary is
the six-vertex control. Host clocks/background work are not controlled, so all
attribution uses current-session controls rather than historical FPS.

### 1280x720: 6,050 actors / 1,024 identities

Two groups of three fresh-process pairs. Initial group runs before/after; the
second reverses each pair after/before to check the inconsistent first results.
Map2100, 6,833 draw calls, 120 warmup + 600 sample frames per process. All runs
complete with identical resource manifests and actor/draw counts.


| Group / version / repeat | FPS | GPU mean ms | CPU record mean ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| crowd / before / 1 | 305.735 | 2.28513 | 2.42846 | 5.6658 | 7.5994 |
| crowd / after / 1 | 313.791 | 2.23726 | 2.41095 | 4.7492 | 5.5665 |
| crowd / before / 2 | 298.610 | 2.29582 | 2.59231 | 4.7244 | 5.5807 |
| crowd / after / 2 | 291.162 | 2.27047 | 2.70887 | 4.9847 | 5.8858 |
| crowd / before / 3 | 307.951 | 2.27345 | 2.44819 | 5.4668 | 7.0775 |
| crowd / after / 3 | 277.160 | 2.32114 | 2.78328 | 5.5491 | 6.8027 |
| reverse / after / 1 | 313.527 | 2.23349 | 2.39643 | 4.6733 | 5.3737 |
| reverse / before / 1 | 303.591 | 2.28609 | 2.44471 | 4.8623 | 6.8542 |
| reverse / after / 2 | 315.565 | 2.23713 | 2.37966 | 4.4749 | 5.4232 |
| reverse / before / 2 | 308.352 | 2.28094 | 2.42444 | 4.8062 | 5.2761 |
| reverse / after / 3 | 308.328 | 2.25524 | 2.41445 | 4.8440 | 5.7965 |
| reverse / before / 3 | 313.179 | 2.26674 | 2.36272 | 4.7545 | 5.4728 |

Initial group medians: FPS **305.735 -> 291.162 (-4.8%)**, GPU mean
**2.28513 -> 2.27047 ms**. Reversed group: FPS **308.352 -> 313.527 (+1.7%)**,
GPU mean **2.28094 -> 2.23713 ms**. The direction of the FPS result changes;
it is not a stable FPS win. CPU recording also varies although the source-level
host recording sequence is unchanged apart from the vertex-count argument.

Across all six processes per version, median FPS is 306.843 -> 310.9275 and
median GPU mean is 2.283035 -> 2.24625 ms (about 1.6% lower). Pooling does not
remove time/order effects or prove a general speedup. Worst p99 is 5.6658 ->
5.5491 ms; frame maximum is 7.5994 -> 6.8027 ms. These finite-run tails do not
guarantee future hitch-free rendering. No sampled frame exceeds 33.33 ms.

### 1920x1080: 13,270 actors / 1,024 identities

Three fresh-process pairs. First pair is before/after; the next two reverse the
order. Same map/resources/warmup/sampling/diagnostics. Draw count is 15,003,
submitted vertices 90,018 -> 60,012, submitted triangles 30,006 in both versions.

| Version / repeat | FPS | GPU mean ms | GPU p95 ms | CPU record mean ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| before / 1 | 153.575 | 5.33332 | 5.76368 | 5.15625 | 10.8198 | 13.3671 |
| after / 1 | 156.756 | 5.25592 | 5.66752 | 5.12581 | 10.1881 | 10.7604 |
| after / 2 | 156.429 | 5.26104 | 5.66968 | 5.15684 | 10.2137 | 11.7977 |
| before / 2 | 154.873 | 5.33551 | 5.78628 | 5.13216 | 10.6569 | 12.3847 |
| after / 3 | 156.557 | 5.26161 | 5.66520 | 5.13584 | 9.5423 | 11.1508 |
| before / 3 | 153.554 | 5.33376 | 5.75096 | 5.14510 | 10.5146 | 13.0379 |

Median GPU mean **5.33376 -> 5.26104 ms (-1.4%)**, median FPS
**153.575 -> 156.557 (+1.9%)**. CPU recording is essentially unchanged
(5.14510 -> 5.13584 ms). GPU mean is lower in each pair on this host; the measured
gain is small, not proportional to the 33.3% submitted-vertex reduction. Worst-run
p99 is 10.8198 -> 10.2137 ms; maximum 13.3671 -> 11.7977 ms, with no sampled
frame above 33.33 ms. Other devices/drivers/workloads remain unverified.

All 18 fixed-crowd reports use actual IMMEDIATE presentation, one shared sprite
pipeline creation, 719 valid GPU completion samples and zero missing results.
No actor, draw, resource identity or texture-payload reduction is credited as
performance. Raw settings/logs/reports/manifests/hashes: ignored
`build/quad-crowd-*`, `build/quad-reverse-*`, `build/quad-1080-*`.

Before executable SHA-256:
`c018e52911c2a0a9514f41ecdb427be0250258c8738d370da59fb0472b6d6757`.
After executable SHA-256:
`28e6f77ba5183218c9d24f04fecdb8aad984738b078bd5734daee2f32c58e4d9`.

### Moving map100 streaming

One 6,000-frame run per version/budget, 512 visible + 512 prefetched actors,
2,048-entry catalog, 240-frame route legs and replacements every 1,200 frames.
All four use fingerprint `835595881056024373` and complete. This accelerated
world-step test is not a long wall-clock soak or real network-latency simulation.

| Version / budget MiB | Lifetime FPS | Visible coverage | Pending at end | Largest frame ms | Peak accounted bytes | Peak process RSS bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| before / 64 | 494.446 | 20.83% | 839 | 19.2790 | 67,107,884 | 439,177,216 |
| before / 512 | 390.659 | 79.48% | 123 | 21.3046 | 536,867,520 | 953,950,208 |
| after / 64 | 469.304 | 20.77% | 815 | 20.7299 | 67,108,612 | 477,687,808 |
| after / 512 | 413.333 | 78.59% | 104 | 20.2202 | 536,869,212 | 932,196,352 |

The 64 MiB FPS regresses while 512 MiB improves; readiness/coverage and host
conditions also vary, so these single runs are not an established streaming
speedup. All have zero animation-load failures, zero frames above 33.33 ms,
5,998 completed GPU samples and zero missing queries. Animation accounting stays
within budget. Accounted RGBA and RSS are not total physical VRAM or proof of
leak freedom. Large pending demand/missing appearances under low budgets remain
real limitations. No residency-policy fix is claimed. Raw artifacts: ignored
`build/quad-stream-{before,after}`.

## Final verification and limits

Full normal CTest **281/281** and MSVC ASan **280/280** pass on the retained runtime
change. The normal-only Python session target accounts for the difference. After
strengthening the faint-pixel check to require exact nonzero red/green (rather
than accepting a one-LSB tolerance), both new pixel tests passed again in normal
and ASan builds (**2/2** each). No production code changed for that final test
strengthening. Logs: ignored `build/quad-{dev,asan}-tests.log` and
`build/quad-final-{dev,asan}-pixels.log`.

Separate native ASan + Vulkan-validation smoke: map100, 600 frames, 64 MiB,
512-entry catalog, 60-frame legs, replacement every 120 frames. It completes with
normal shutdown, 598 GPU samples, zero missing results and no animation failure.
Instrumented lifetime FPS is 116.56 and maximum frame 30.23 ms (zero above
33.33 ms); instrumented timings are not performance comparisons. Artifacts:
ignored `build/quad-asan-smoke`.

All 23 native crowd/streaming/smoke logs close normally. Full normal/ASan and final
pixel-test logs, and all native logs, contain no VUID, validation-error or ASan-error
diagnostics. Existing missing Steam layer-manifest loader messages remain on
this host. Pixel fixtures check the device validation counter through device-child
cleanup; existing shared-pipeline, pending-cancellation and weak ownership tests
remain passing. No new resource orphan was observed. MSVC ASan is not exhaustive
leak/race detection, and GPU ownership is unchanged rather than newly redesigned.

This is a small equivalent-geometry cleanup with a modest measured 1080p GPU gain.
It does not solve high-overdraw crowds, long-session residency or every frame
stall. Further batching/cropping work must justify its own CPU/GPU cost, memory
bounds, transparent order and resource lifetime; do not infer a universal gain
from fewer submitted vertices alone.
