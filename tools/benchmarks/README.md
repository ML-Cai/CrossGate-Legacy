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
