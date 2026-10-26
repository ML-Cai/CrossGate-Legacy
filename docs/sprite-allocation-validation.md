# Sprite allocation diagnostics

## Scope

The animation/map libraries budget RGBA payload and pending reservations. Those
numbers do not describe Vulkan allocation alignment or concurrent staging memory.
The native device now exposes an optional, owner-thread snapshot of actual
`vkAllocateMemory` allocation sizes for SpriteRenderer images and upload staging.
Counters are recorded only after a successful allocation, and removed after the
actual `vkFreeMemory`. Retiring a ready resource or cancelling a CPU decode does
not by itself change this accounting.

The categories are **image** and **staging**, not dedicated-VRAM and system-RAM.
On integrated GPUs their heaps/properties can overlap. Counters are not physical
residency or total device/process usage. They exclude swapchain/backbuffers,
other buffer/image helpers, descriptors, pipelines, driver allocations and CPU
RGBA/reader caches. No heap-budget extension or residency query is introduced.
Existing memory admission/eviction thresholds remain RGBA budgets; this PR does
not change allocation strategy, memory types, rendering or fence ordering.

The ledger keeps fixed counters (no handle map, heap allocation or polling in
sprite drawing). It tracks current bytes/counts and lifetime high-water marks for
images, staging and their simultaneous total. The total high-water mark is not
the sum of independent category peaks. Scope is one native Device wrapper's
lifetime; peaks survive resource cleanup and explicit device destruction for
inspection. Allocation/free calls are render-owner-thread operations, as before;
these counters are not a cross-thread API or an allocation profiler.

Each renderer retains its two allocation sizes. A failed allocation does not
publish a handle/size to the renderer. Existing cleanup releases tracked memory
on later preparation failure, upload completion, cancellation or destruction.
Overflow/invalid transitions increment `accountingErrors` and leave current
usage unchanged. Aggregate counters cannot identify every mismatched handle or
size; actual Vulkan validation/ownership tests are still necessary.

## JSON fields

When runtime telemetry is enabled, the client report includes:

- `sprite_allocation_supported`: whether the backend supplies this measurement.
- `sprite_{image,staging}_allocation_{bytes,count,peak_bytes,peak_count}`.
- `sprite_allocation_{bytes,count,peak_bytes,peak_count}`: simultaneous total.
- `sprite_allocation_accounting_errors`: expected to remain zero in real runs.

Unsupported backends use the default optional API and emit null numeric values,
not a false zero. Supported zero means no SpriteRenderer memory is currently
allocated. Counters update at allocation/free boundaries even when frame timing
is disabled; reporting reads a fixed snapshot only. Never add these bytes to RGBA
payload as though the two ledgers represented disjoint allocations. Peak total
may include staging that has already disappeared from a ready-only snapshot.
The client final report is taken with the live scene/cache still present; nonzero
ready images are expected there. The lifecycle soak instead checks zero after
world/cache cleanup. Nonzero accounting errors invalidate the measurement.

## Validation

- CPU-only unit cases cover simultaneous versus independent peaks, invalid releases,
  zero/overflow rejection, 100,000 upload/cancel-style transitions, unsupported
  backend defaults and null-versus-zero JSON. The same suite is built in the
  desktop and SDK-free Windows/Linux ordinary/ASan configurations.
- Actual Vulkan tests observe synchronous upload cleanup, live progressive staging,
  completion, cancellation, invalid preflight and repeated cleanup. Manually
  allocated offscreen/readback fixture memory deliberately remains outside scope.
- Existing deferred-retirement tests verify bytes/counts do not disappear when
  ready resources move to retired ownership. All native sprite/scene teardown and
  every same-process lifecycle checkpoint require zero live sprite allocations
  and zero accounting errors before destroying the device.

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
build/python-session-env/Scripts/python.exe tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/sprite-allocation-stress
build/python-session-env/Scripts/python.exe tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 300 --warmup-cycles 100 --require-memory --max-growth-mib 64 --output build/sprite-allocation-soak
```

Use an x64 Visual Studio developer environment for MSVC/ASan; desktop tests need
installed assets and Vulkan. RSS includes allocator/driver retention and is not
proof of universal leak freedom. Windows ASan does not provide LeakSanitizer.
Dedicated 4/8 GiB GPU behavior remains to be tested on suitable hardware.

## Results

Local results:

- Full desktop ordinary **329/329**, MSVC ASan **328/328** (ordinary-only Python
  binding case), zero skipped cases. SDK-free ordinary/ASan **76/76** each.
- The final native allocation/completion/cancellation case was rerun in both
  configurations after making teardown continue cleanup on a missing measurement.
- **300/300** same-process lifecycle cycles, warmup100: all logical resource,
  pending-work and actual sprite-allocation checkpoints clear; accounting errors
  remain zero. First/last RSS window medians **262,819,840 / 262,912,000 B**
  (**+90 KiB**); peak sampled RSS **301,928,448 B**. The diagnostic 64 MiB growth
  gate passes. This soak ran alongside native ASan tests; performance runs below
  started after all tests/builds finished.

### Streaming and actual allocation sizes

Radeon 880M integrated development GPU, 1280x720, map100, VSync off with verified
IMMEDIATE present mode, 512 visible + 512 outer, catalog2048, 6000 updates,
replacement every1200 updates:

| Measurement | 64 MiB animation budget | 512 MiB animation budget |
| --- | ---: | ---: |
| Gameplay lifetime FPS | 230.412 | 192.461 |
| Gameplay rolling p99 | 9.5497 ms | 8.9156 ms |
| True-ready visible coverage | 21.4090% | 80.1746% |
| Final ready/retired image RGBA payload, including map/outline | 107,449,136 B | 575,608,268 B |
| Final live sprite image allocation sizes | 134,400,768 B | 749,252,352 B |
| Final image allocation count | 276 | 1,289 |
| Peak simultaneous sprite allocation sizes | 139,173,120 B | 756,940,480 B |
| Peak simultaneous allocation count | 279 | 1,345 |
| Staging allocation peak | 4,194,304 B / 1 allocation | 4,194,304 B / 1 allocation |
| Final staging allocation sizes/count | 0 / 0 | 0 / 0 |

Final image allocation sizes exceed the corresponding ready/retired image payload
by **25.08% / 30.17%** in these snapshots. This exposes format/layout/alignment
costs; it is not a universal multiplier or a physical residency measurement.
Do not compare an animation-only budget to combined map/animation allocation totals
as though the scopes matched. Pooling separate images cannot be assumed to remove
padding within each image's required layout; investigate that separately before
choosing an atlas/allocator change.

Both runs complete with fingerprint `835595881056024373`, zero accounting or
appearance/load failures and 5999 GPU timing results / zero missing results. Current
category sums/counts and simultaneous peak bounds are consistent. Animation payload
peaks are **67,107,904 / 536,870,468 B**, within their existing budgets. These are
contemporary regression results, not a paired streaming FPS improvement claim.

### Fixed preloaded rendering control

To separate drawing from asynchronous appearance readiness, run before-A / after-A /
after-B / before-B with map2100, 6050 actors, 1024 ready resources, 6833 draws,
120 warmup + 600 sampled frames, GPU timing enabled, VSync off. The retained
control executable predates #87 (#86); graphics/SpriteRenderer and the executed
fixed preloaded benchmark path are identical through merged #87. The #87 local
scene-entry asynchronous path is not executed by this benchmark. Binary hashes and
raw reports are retained with the measurements. This is a rendering-path control,
not a whole-application comparison against freshly rebuilt #87.

| Median of two runs per version | Before | With allocation counters |
| --- | ---: | ---: |
| FPS | 140.2465 | 141.3930 |
| Rolling p99 | 9.9711 ms | 9.2019 ms |
| CPU recording mean | 5.83753 ms | 5.81014 ms |
| GPU interval mean | 3.077405 ms | 3.074135 ms |

Individual FPS is 142.555 / 137.938 before and 135.658 / 147.128 after. The small
control shows no consistent-direction slowdown; it does not establish zero cost
or a performance gain. Readiness/cold-upload workloads are separate.

Positive native validation-enabled fixtures and full native/stress/control/soak
logs contain no Vulkan VUID/validation-error or sanitizer error markers. Ignored
evidence: `build/sprite-allocation-{tests,asan-tests,headless-tests,headless-asan-tests}.log`,
`build/sprite-allocation-stress/`, `build/sprite-allocation-render-control/`, and
`build/sprite-allocation-soak/`. No game files, binaries or raw logs are committed.
Hosted Windows/Linux ordinary/ASan jobs reuse the same CPU-only ledger suite;
consult the PR checks for the current matrix result.
