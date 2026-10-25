# Optional GPU frame timing

Base: merged PR #75 (`8de76db`). This change adds diagnostics before selecting the
next rendering optimization. It does not change sprite ordering, draw count,
resource budgets, preparation concurrency, input or gameplay.

## Measurement and ownership contract

`[Telemetry] Enabled=true` enables CPU phase timings and optional GPU timestamps.
The default disabled path creates no query pool and records no query commands.
The crowd runner exposes this through `--gpu-timing`; streaming stress already
enables telemetry. The client remains standalone and needs no Python at runtime.

Each enabled command buffer owns a pool of two timestamp queries. The clock uses
the selected graphics queue family's `timestampValidBits` and device
`timestampPeriod` (nanoseconds). Unsupported clocks and pool-creation failure
leave timing disabled. Begin records query reset and TOP_OF_PIPE; end records
BOTTOM_OF_PIPE. The elapsed interval covers the graphics recording, including
any GPU dependencies/stalls within that span. It is not exclusive shader time,
GPU utilization, presentation duration or displayed FPS. Concurrent/overlapping
GPU work and pipeline scheduling can affect it.

On reuse, the engine waits for its existing frame-slot fence, reads that slot's
previous successful recording once, then resets it. The read requests 64-bit
values and availability, without WAIT. No extra fence/queue-idle is added for
measurement. The graphics API explicitly requires the caller to establish
completion of a successful submission; it must not read an in-flight or merely
recorded/unsubmitted command buffer. Availability alone could otherwise describe
an older use before a queued reset executes. Reset/discard clears host-side
recording state. Unavailable/error results become missing samples, not 0 ms.
Only ready game submissions contribute; loading submissions are excluded.

Destroying a command-buffer list releases its query pools before the command
pool. Cleanup is idempotent and still requires all uses to have completed, as did
the original command pool. The native pixel test repeats pool creation, completed
submissions, multiple slots, reuse, discarded unsubmitted recordings and repeated
destroy, and checks the validation error counter through device-child teardown.

Unsigned delta conversion handles counter wrap and masks invalid high bits before
converting to floating point. Elapsed time must be less than one counter period;
multiple wraps cannot be reconstructed from two values. Invalid/non-finite clock
or converted values produce no sample. These cases have headless unit tests.

The Vulkan contract follows the official
[query specification](https://docs.vulkan.org/spec/latest/chapters/queries.html),
[timestamp command](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp.html)
and [query result API](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetQueryPoolResults.html).

## Report semantics

- `cpu_fence_wait_mean_ms`: wall time in the existing frame-slot fence wait.
- `cpu_record_mean_ms`: reset through command-buffer end, including world sorting.
  These are subsets of CPU render time, not additional independent costs. They
  do not include every render phase: image acquisition, submit and presentation
  remain in the existing total CPU render time.
- `gpu_supported_slots`: command buffers whose optional query pools were enabled;
  this is not the number of frames simultaneously executing.
- `gpu_window_samples`, `gpu_total_samples`, `gpu_missing_results`: bounded-window
  valid samples, lifetime valid samples and lifetime missing/invalid results.
- `gpu_mean_ms`, `gpu_p95_ms`, `gpu_max_ms`: up to 512 most recent valid completion
  samples. No samples means JSON `null`; a measured zero remains numeric zero.
  Disabled/unsupported devices remain runnable and have no GPU samples.

CPU and GPU sample windows are independent and delayed relative to each other.
The last submitted slots are not drained just to include them in reports. CPU
phase means use the last 512 telemetry samples; benchmark frame/FPS statistics
use its configured 600 measured frames after 120 warmup frames. GPU totals can
include ready warmup submissions; the recent window excludes them only after
enough subsequent valid completions. Do not add CPU and GPU means or interpret
them as one-to-one frame pairs. Storage is fixed-size; no per-frame history grows.

## Reproduction

Use a Visual Studio x64 developer shell, installed game data at
`C:/CrossGate-Legacy`, and a fresh output directory:

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure --no-tests=error
cmake --preset asan
cmake --build --preset asan
ctest --test-dir build/asan --output-on-failure --no-tests=error
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --map map/0/2100.dat --output-dir build/gpu-crowd --densities 10 --unique 1024 --repeats 3 --gpu-timing
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/gpu-stream
```

Remove `--gpu-timing` for a same-binary diagnostic-disabled crowd control.
The runner requires actual IMMEDIATE presentation unless VSync is explicitly
requested. Validation and VSync are disabled for performance runs. ASan plus
validation is a separate correctness smoke, not a performance comparison.

## Results

Windows, RelWithDebInfo, AMD Radeon 880M integrated GPU. This is not a dedicated
4/8 GiB GPU capacity test. Runs are serialized after builds/tests. Fixed crowd
uses map2100, 1280x720, 6,050 actors, 1,024 matching resource identities,
6,833 last-frame draw calls, 120 warmup + 600 sample frames. Three fresh processes
per variant, run in baseline/disabled/enabled groups; host clocks and background
activity are not controlled. All runs report IMMEDIATE, complete coverage and one
shared sprite pipeline creation.

| Variant | FPS runs | Median FPS | Worst-run frame p99 ms | Largest frame ms |
| --- | --- | ---: | ---: | ---: |
| Retained #75 binary, telemetry disabled | 142.111 / 146.908 / 138.483 | 142.111 | 10.0617 | 11.6985 |
| New binary, telemetry disabled | 142.592 / 144.502 / 140.119 | 142.592 | 10.1649 | 11.2791 |
| New binary, telemetry enabled | 142.938 / 141.967 / 139.406 | 141.967 | 10.2193 | 11.7185 |

Disabled-path median differs by +0.34% from the retained baseline; enabling all
CPU/GPU telemetry differs by -0.44% from the same binary disabled. These differences
are within observed run variation, not proof of zero overhead or a speed gain.
No sampled frame exceeded 33.33 ms across these nine runs. The maximum measured
process RSS per group was 690,634,752 / 690,094,080 / 701,812,736 bytes; this is
process RSS, not GPU memory or exhaustive leak detection.

The contemporary retained baseline is much slower than the historical #75 report
(about 309.9 FPS median). No code change explains that cross-session baseline
difference here, and its cause was not diagnosed. Use the fresh within-session
controls; do not attribute historical differences to this instrumentation.

| Enabled repeat | CPU record mean ms | CPU fence wait mean ms | GPU mean ms | GPU p95 ms | GPU max ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 5.83213 | 0.0100844 | 3.07534 | 3.57120 | 4.58276 |
| 2 | 5.84561 | 0.0101348 | 3.08048 | 3.61804 | 4.35300 |
| 3 | 5.94249 | 0.0103775 | 3.08586 | 3.61192 | 4.31100 |

Every enabled repeat has 3 enabled command-buffer slots, 719 valid completed GPU
samples, a 512-sample GPU window and 0 missing results. Disabled runs have zero
enabled slots/samples and null GPU durations. CPU sort means in enabled benchmark
runs are 3.64532 / 3.66766 / 3.71788 ms (600-frame benchmark window). This supports
investigating CPU sorting/recording before a GPU-only optimization for this
workload; it is not a universal bottleneck classification. CPU recording contains
sorting, so those values must not be added. GPU intervals may overlap CPU work.

Baseline executable SHA-256:
`d06cc4792485cf11fc2ad458613168c3b733ec111ec82c2c993d365868c678a9`.
New executable SHA-256 (both modes):
`46da56dcd8beb651782c3b624f1070f0d7e46258710bf67aa10ca9e1aedaad82`.
Raw INIs/logs/JSON/manifests/hashes: ignored `build/gpu-crowd-{baseline,disabled,enabled}`.


## Moving map100 streaming

One coverage run per budget, same workload fingerprint `835595881056024373`:
512 visible + 512 prefetched actors, 2,048-entry catalog, replacement every 1,200
frames, 6,000 ready frames, camera walking on map100. This validates the delayed
queries alongside background preparation and resource retirement; it is not a
repeated performance comparison or a full-coverage guarantee.

| Budget MiB | Lifetime FPS | Visible coverage | Pending demand at end | Largest lifetime frame ms |
| --- | ---: | ---: | ---: | ---: |
| 64 | 209.665 | 20.94% | 838 | 22.6667 |
| 512 | 179.024 | 80.27% | 127 | 28.4800 |

| Budget MiB | CPU record mean ms | CPU fence wait mean ms | GPU mean ms | GPU p95 ms | GPU max ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64 | 1.60445 | 0.0103451 | 0.945173 | 1.83572 | 2.12956 |
| 512 | 1.49485 | 0.0137572 | 0.981344 | 2.35648 | 4.69520 |

Timing columns describe independent recent 512-sample windows, whereas FPS,
coverage and frame maxima describe the whole ready run. Both runs have 5,998 valid
GPU completion samples, 3 enabled slots and 0 missing results. Neither has a
frame above 33.33 ms or a reported animation-load failure. Accounted animation
peaks are 67,108,620 / 536,870,412 bytes, within the respective budgets; peak
process RSS is 432,898,048 / 876,105,728 bytes. Accounted RGBA and RSS do not measure
physical VRAM or driver allocations. Low-budget pending demand and incomplete
appearance coverage remain real limitations; this PR does not change residency
policy or claim to solve them. Raw reports/logs: ignored `build/gpu-stream`.

## Final verification

- Full normal CTest: **276/276**, including six new clock, telemetry and real-Vulkan
  tests. Full MSVC ASan CTest: **275/275** (Python session target is not built by
  that preset). Both include unit, memory, asset, desktop and startup coverage.
- Native query lifecycle/readback test passes in both builds without skipping:
  four fresh two-slot lists, three submissions per slot, a query read only after
  completion, single consumption, discarded unsubmitted recordings, repeated
  destroy and validation error counter checked through device-child cleanup.
- Nine fixed-crowd reports preserve identical 1,024-resource manifests, actor/draw
  counts, complete status and actual uncapped presentation. All six new-binary
  reports have correctly enabled/disabled timing fields.
- Two 6,000-frame streaming runs validate completion, bounded animation accounting,
  resource demand and GPU sample fields. Their partial coverage is reported above.
- Separate native ASan + Vulkan validation smoke: map100, 600 frames, 64 MiB,
  512-entry catalog, 60-frame legs and 120-frame replacements. It completed with
  598 GPU samples, 512 recent samples, 0 missing results and 0 animation failures.
  Instrumented lifetime FPS is 56.73 with three frames above 33.33 ms and a
  39.66 ms maximum; these instrumented timings are not a performance comparison.
  Raw artifacts: ignored `build/gpu-asan-smoke`.
- All 12 native benchmark/stress logs end with normal engine closure. Full normal
  and ASan verbose logs and all 12 native logs contain no VUID, validation-error
  or ASan-error diagnostics. This host still emits missing Steam layer-manifest
  loader messages; these are not newly introduced validation VUIDs.

MSVC ASan is not an exhaustive leak detector or race detector. Explicit pool
ownership, repeated cleanup and Vulkan device-child validation complement it;
no new resource orphan was observed in these runs. Long-running device-loss,
Android/Linux lifecycle and dedicated-card VRAM validation remain untested here.

Next: investigate the CPU sort/recording portion with representative moving
footprints, then trial order-preserving batching only where it actually reduces
recording work. Preserve painter order, native state invalidation and frame-local
buffer retirement; do not combine that work with residency-policy changes.
