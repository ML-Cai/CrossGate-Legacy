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
