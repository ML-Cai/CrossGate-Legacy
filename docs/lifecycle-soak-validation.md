# Same-process resource lifecycle validation

Base: merged PR #80 (`063df1b`). This is test/tooling infrastructure, not a change
to gameplay, cache budgets, rendering performance or resource admission policy.
Hosted CPU-only CI remains the next independent PR.

## What is exercised

One native test process retains the same Vulkan device/window, frame pass and
readers through alternating cold map-100/map-2100 cycles. Each cycle runs a
16-remote-actor world (8 visible / 8 outer), movement and at least four snapshot
replacements, records draws, exits, retires cached owners against the actual last
submitted serial, waits for GPU completion, then collects. Weak map/clip owners
must expire. It also cancels metadata while reader access is held and cancels an
already-submitted GPU upload.

At every quiescent checkpoint native assertions require zero live sprites/remote
actors, map/loading/streaming singletons, cached/retired maps/outlines/animation
clips, pending work, budget waits and tracked payload. Persistent readers, worker
service, frame/device objects, allocator/driver caches and their memory are not
claimed to disappear. Libraries are reused across cycles; they are not replaced
by a new process or cleared wholesale on every iteration.

The shared desktop fixture checks the validation error count (including repeated
identical messages) through explicit device destruction before deleting the
counter owner. The soak rejects `Display.Validation=false`; a separate negative
configuration proves this fails rather than silently running without validation.
That expected failing log is separate from positive validation evidence.

## Automated coverage and reproduction

[LifecycleSoak](../tools/LifecycleSoak/README.md) documents commands, requirements,
checkpoint/RSS semantics and optional measured growth gates. Ordinary full CTest
runs four cycles; `dev-lifecycle-soak` and `asan-lifecycle-soak` run 100 cycles,
with 20 warmup cycles excluded from RSS windows. Native resources are still
checked on every warmup cycle.

Ten GPU/asset/psutil-independent Python tests cover cycle acknowledgement,
missing/skipped tests, malformed checkpoint order, nonzero exit, sanitizer/VUID
messages, log-reader and broken-ack/pipe-close failures, timeout child termination/reaping, explicit missing
memory instrumentation and disjoint-window/growth-gate arithmetic. Synthetic
failure messages in these unit tests are expected; they cannot make a failed
native run look complete.

Measurements are Windows, AMD Radeon 880M integrated graphics. These are lifetime
checks with idle waits and sampling pauses, not an FPS benchmark or a 4/8 GiB
dedicated VRAM capacity test. The optional process RSS sample is taken while the
child is paused after cleanup, before the next reload. Sampled peak RSS may miss
short peaks; accounting bytes and process RSS are not physical GPU residency.

## Results

Full normal CTest: **287/287 passed**, 61.59 s. Full MSVC ASan CTest:
**286/286 passed**, 266.45 s. The normal-only Python binding environment test
explains the difference. The explicit validation-enabled guard was subsequently
rechecked using the final native test binary (positive and negative modes).

Dedicated 100-cycle runs:

| Build | Test elapsed s | Sampled peak RSS bytes | First post-warmup window bytes | Last window bytes | Difference |
| --- | ---: | ---: | ---: | ---: | ---: |
| Normal | 34.13 | 326,418,432 | 307,929,088 | 301,314,048 | -6,615,040 |
| ASan | 56.31 | 1,028,669,440 | 936,591,360 | 970,670,080 | +34,078,720 |

Both complete all 100 checkpoints with all-cycle resource assertions passing.
ASan's +32.5 MiB difference is reported, not classified as either a leak or a
proven benign cache. Its ten-cycle medians after cycle 60 were about
923.03/924.24/925.29/925.70 MiB; a longer run follows below to examine mature
behavior. Do not compare ordinary and instrumented RSS as a resource saving.

Passing assertions/ASan/Vulkan diagnostics and an RSS trend do not establish
exhaustive leak freedom. This is a bounded repeatable workload, not an hours-long
session, device-loss test or coverage of every resource/platform path.


Final guarded ASan extension: **300/300 cycles passed**, with 100 warmup cycles,
required psutil sampling and an explicitly selected 64 MiB growth gate:

```powershell
python tools/LifecycleSoak/run_soak.py --test-exe build/asan/bin/test_scene_lifecycle.exe --cycles 300 --warmup-cycles 100 --require-memory --max-growth-mib 64 --output build/soak-asan-300
```

Sampled peak RSS: 869,330,944 bytes. First/last post-warmup ten-sample medians:
822,405,120 -> 805,705,728 bytes, a difference of **-16,699,392 bytes (-15.93 MiB)**.
Fifty-cycle window medians across this run were 787.18, 783.71, 784.28, 775.71,
768.51 and 768.30 MiB. There is no sustained increase in these windows. Absolute
RSS differs between separate runs; do not splice the 100/300-cycle sequences or
attribute the earlier rise to a particular allocator/driver without measurement.
The 64 MiB gate is a diagnostic choice for this mature-run comparison, not a new
engine memory cap or a universal leak criterion.

All three dedicated native runs pass all cycle invariants and teardown without
VUID, Vulkan validation errors or sanitizer reports. The final validation-enabled
guard is also exercised by a short ASan CTest rerun, and the final runner's ten
protocol/error tests pass. Disabling validation in an isolated copied settings
file produces the expected native assertion and failed wrapper summary; the
shared test settings are never edited by the runner.

Ignored evidence: `build/soak-final-{dev,asan}-tests.log`,
`build/soak-long-{dev,asan}.log`, `build/soak-{dev,asan}-100-summary.json`,
`build/soak-asan-300/`, `build/soak-guard-asan-tests.log`,
`build/soak-final-runner-tests.log` and `build/soak-guard-negative.log`.
The last file is an expected negative-control outcome, not a positive test log.
Native logs are named by the associated JSON reports. Reusing a CTest output
directory replaces its latest summary; the 100-cycle summaries were copied before
later four-cycle suite checks. No executable, game asset or raw log is committed.
