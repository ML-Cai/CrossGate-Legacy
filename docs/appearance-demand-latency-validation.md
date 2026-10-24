# Appearance demand latency validation

Base: develop `8f0e827` (PR #71), measured 2026-10-24 on the same development
PC/Radeon 880M integrated GPU. The retained before executable was built from source
identical to the squash-merged base. This PR adds opt-in observation; resource
budgets, selection, eviction and GPU lifetime policy remain unchanged.

## Measurement contract

See [field definitions](../tools/StreamingStress/README.md#appearance-demand-latency).
A miss starts at the first scene observation of missing ready data in the current
interest region, keyed by catalog appearance and shared across actor users. It
ends at the scene observation of usable cached data. Cache hits do not generate
latency samples. Region/visibility uses existing foot-point tests. Server/network,
GPU timestamps and time until a frame reaches the display are outside this clock.

Queued, deferred and loading sample intervals partition total miss wall time.
Loading covers the matching request's metadata/decode/upload/polling or ready
handoff; cancelled-worker draining is queued for the new request. Deferred covers
known capacity exclusions/exact budget waits. Queued includes waiting behind other
requests, including those stalled by global budget pressure; a large queued value
alone does not identify CPU decoding as the bottleneck. Phase assignment has
scene-update sampling and 1 ms clock resolution.

Total and visible successful windows each retain 256 samples; lifetime completion
counts are separate. Visible samples cover the current continuous visible period
of a miss that completes visibly, excluding earlier prefetch and visibility periods.
Current ages/counts, lifetime observed age maxima and withdrawn/failed outcomes
are reported beside successful quantiles so unresolved/censored demand stays visible.
The conservation check is started = completed + withdrawn + failed + pending.
Reports reset with the scene; replacing actor IDs does not reset a still-shared miss.

Tracking is enabled only with RuntimeTelemetry. It retains at most 1024 current
miss nodes, fixed sample rings, a reused scalar observation vector and a bounded
stack scratch array. Input pointers in that scratch never escape observe(). Stable
misses do not repeat insertion lookups; ready observations skip phase lookups.
Sorting occurs on report emission. No tracker holds an entity, asset reader, GPU
object or asynchronous callback. Scene teardown destroys its state.

## Automated and native validation

- Full normal CTest **255/255** and MSVC ASan CTest **254/254** passed. Complete
  positive logs inspected: no VUID/ASan error markers. The difference is the existing
  normal-only Python session test.
- Six deterministic windowless tests cover phase partition, shared users/cache
  hits, prefetch promotion/visibility reset, pending/withdrawn/failed/reentry outcomes,
  1000 completions/256-entry overwrite, repeated disjoint 1024-demand replacement,
  clear/restart and invalid sizes/backward clocks without state mutation.
- GPU blocked-request recovery also checks complete demand samples, positive
  deferred time, phase conservation and eventual completion. Reader-lock/cancellation
  tests verify request matching and exclusion of a draining cancelled worker.
- RuntimeTelemetry-disabled demo leaves tracking empty; enabled scripted scene
  records misses, preserves outcome bounds and drops state on exit. Cold/warm
  restart assertions were refined to allow legitimate all-cache-hit warm runs;
  that test passed three repeats on normal and ASan builds.
- Python report checks reject incomplete, inconsistent, invalid ring/count/age/
  timing/phase data and retain compatibility with reports from the base client.
- Final ASan+Vulkan-validation native smoke: map100, 240 updates, leg60/replace120,
  128 visible/160 prefetch, catalog512, 64 MiB, VSync off. Completed normally,
  648 started = 78 completed + 327 withdrawn + 0 failed + 243 pending; no VUID/
  ASan markers and no appearance-load failures. Normal shutdown handles pending
  work after reporting. Peak animation ledger 14,224,984 bytes. This instrumented
  debug smoke is not a release performance benchmark.

These checks do not establish absence of every allocator/driver/OS leak or measure
physical VRAM; process RSS includes CPU caches and driver memory.

## Identical native traces

Each run: map100, VSync off/immediate, validation off, seed3169, 6000 scripted
updates, 16.666667 ms simulated step, leg240. The 128/160 trace uses catalog1024,
replace600; 512/512 uses catalog2048, replace1200. Timing/FPS are real wall time;
movement/snapshots use accelerated simulated time. Fingerprints match across
versions/budgets: `15372329328879399793` and `835595881056024373`. No concurrent
builds or GPU tests. One run per version/configuration initially; repeated pressure
checks below retain the outlier rather than replace it. Do not compare absolute
FPS to earlier sessions under different host conditions.

| Actors version | Budget MiB | FPS | Max frame ms | Frames over 33.33 ms | Visible coverage | Peak animation ledger MiB | Peak RSS MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128/160 before | 64 | 918.5 | 17.15 | 0 | 43.67% | 64.000 | 425.4 |
| 128/160 before | 512 | 939.8 | 19.14 | 0 | 87.38% | 332.150 | 639.0 |
| 128/160 after | 64 | 897.9 | 44.59 | 1 | 45.25% | 64.000 | 461.2 |
| 128/160 after | 512 | 934.3 | 18.52 | 0 | 87.51% | 332.192 | 665.0 |
| 512/512 before | 64 | 603.9 | 16.75 | 0 | 19.56% | 64.000 | 418.1 |
| 512/512 before | 512 | 548.9 | 17.30 | 0 | 68.91% | 511.999 | 895.0 |
| 512/512 after | 64 | 583.1 | 17.77 | 0 | 19.36% | 63.999 | 437.0 |
| 512/512 after | 512 | 537.0 | 19.44 | 0 | 68.92% | 511.999 | 922.8 |

Initial observed FPS differences with tracking enabled were about 0.6-3.4% after
removing redundant collector lookups. These single comparisons are not a causal
estimate or a guarantee of zero instrumentation cost. Normal gameplay without
Telemetry does not run this tracker. All eight runs exited normally, remained
within animation budgets and reported zero appearance-load failures.

| Visible/prefetch | Budget MiB | Miss mean ms | Queued mean | Deferred mean | Loading mean | Visible successful p99 ms | Peak observed visible wait ms | Started / completed / withdrawn / pending |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 128/160 | 64 | 403.1 | 395.6 | 2.8 | 4.7 | 531 | 724 | 5802 / 1008 / 4549 / 245 |
| 128/160 | 512 | 44.9 | 40.5 | 0.0 | 4.4 | 224 | 453 | 2408 / 1016 / 1392 / 0 |
| 512/512 | 64 | 2064.4 | 2054.2 | 3.8 | 6.4 | 2386 | 2538 | 14101 / 1021 / 12241 / 839 |
| 512/512 | 512 | 314.7 | 304.1 | 4.1 | 6.5 | 417 | 2046 | 7453 / 1600 / 5745 / 108 |

Means/quantiles use recent successful miss windows, while outcomes/age peaks are
scene-lifetime observations; the populations are deliberately different. Miss
failures were zero in these traces. In the 512/512, 64 MiB case, most recent
successful waiting was queued (~2054 of 2064 ms), while 12,241 tracked misses were
withdrawn before usable data was observed. This supports profiling queue causes
and bounded preparation/upload overlap before changing visible victims. It does
not prove that extra parallelism, bigger caches or one replacement rule will solve
all demand. Faster/slower frame rates also change available wall time per simulated
replacement, so coverage changes in this accelerated workload need care.

## Long-frame follow-up

The first final 128/160, 64 MiB run contained one 44.59 ms frame. Its cause was not
identified; no claim of stutter-free execution follows. Three additional paired,
interleaved 6000-update comparisons of exactly that workload gave:

| Repeat version | FPS | Max frame ms | Frames over 33.33 ms |
| --- | ---: | ---: | ---: |
| 1 before | 916.7 | 19.95 | 0 |
| 1 after | 909.3 | 15.65 | 0 |
| 2 before | 918.0 | 18.56 | 0 |
| 2 after | 917.7 | 15.24 | 0 |
| 3 before | 899.3 | 17.68 | 0 |
| 3 after | 902.9 | 15.52 | 0 |

No repeat reproduced the spike; the original remains in the table. All six repeat
runs passed budget/outcome/report checks with no load failures. They do not isolate
OS/driver/allocator scheduling or exclude rarer stalls.

## Reproduce

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -R "AppearanceDemandTelemetry|BudgetBlockedVisibleClip|BackgroundMetadataKeepsZeroByteSlot|ScriptedServerChurn|streaming_stress_runner" --output-on-failure
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --output build/demand-check
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --output build/demand-check1024
```

Adjust the installed asset root. Generated INIs, JSON, logs and summaries stay in
each output folder. psutil is optional for RSS. Use a Visual Studio developer shell
for MSVC ASan binaries; run GPU tests and benchmarks sequentially. Ordinary demo
WASD/N/R behavior remains available with stress mode disabled.
