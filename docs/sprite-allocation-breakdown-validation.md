# Live sprite allocation breakdown

## Purpose and boundaries

PR #88 measures SpriteRenderer Vulkan image/staging allocation sizes separately
from RGBA payload. This follow-up makes the live image costs attributable to
logical frame canvas sizes before choosing a memory layout or admission change.
It changes diagnostics only: image storage, shaders, sampling, upload scheduling,
budget/eviction policy and draw ordering remain unchanged.

`SpriteAllocationStatistics::imageClasses` is a fixed array of six value counters.
Each class contains live `images`, logical `frames`, `payloadBytes` and Vulkan
`allocationBytes`. Runtime JSON adds `sprite_image_classes` with these keys:

| Key | Classification |
| --- | --- |
| `single_frame` | Any one-frame image, including atlas pages/outline and one-frame clips |
| `multi_frame_le_4k` | More than one frame, each logical RGBA canvas <= 4 KiB |
| `multi_frame_le_16k` | More than one frame, each canvas > 4 and <= 16 KiB |
| `multi_frame_le_64k` | More than one frame, each canvas > 16 and <= 64 KiB |
| `multi_frame_gt_64k` | More than one frame, each canvas > 64 KiB |
| `unclassified` | Allocation without usable footprint metadata |

JSON uses `payload_bytes` / `allocation_bytes`. Canvas size means width * height *
4 bytes, including transparent pixels, not opaque bounds or the entire animation.
The helper compares widened pixel counts to avoid overflow. Unsupported backends
emit null for the whole classification object; supported empty groups emit zero.

Only successful allocation/free boundaries update these fixed counters. There is
no handle table, retained resource owner, heap allocation or per-draw update.
SpriteRenderer derives the footprint from its immutable dimensions when recording
allocation/free, without adding renderer object fields. Retired resources remain
charged until actual destruction after the existing fence boundary. Counter
validation rejects malformed metadata/overflow before changing any live totals.
This is aggregate consistency checking, not detection of every wrong handle.

Group image counts/allocation bytes sum to the existing live image totals.
Per-class values are current snapshots, not historical group peaks; existing
whole-device peaks retain their original semantics. Measurements exclude other
Vulkan objects/driver costs and do not report physical VRAM residency. The RGBA
budget remains a payload budget, and must not be added to allocation bytes as
though the two represent disjoint memory. Cache selection affects the live sample.

A vertical frame-strip prototype was withdrawn after native pixel comparison
found different nearest-texel choices at fractional transforms. No prototype
layout/shader/API code is included here. Any later packing proposal must preserve
those sampling cases as well as resource ownership; savings alone are insufficient.

## Validation

The SDK-free suite now includes nine ledger cases (four new cases), covering size
boundaries, widened arithmetic, aggregate class sums, matching release, malformed
metadata/overflow without partial mutation and JSON null/zero/footprints. The same
cases run in the desktop suite. Real Vulkan tests observe a two-frame image and
verify each group returns to zero after cleanup; scene retirement verifies all
class values survive transfer into the retired list. Every scene fixture teardown
and repeated-lifecycle quiescent checkpoint also checks zero images/frames/payload/
allocation and zero accounting errors before device destruction.

```powershell
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure
cmake --build --preset asan
ctest --test-dir build/asan --output-on-failure
python tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 300 --warmup-cycles 100 --require-memory --max-growth-mib 64 --output build/allocation-breakdown-soak
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/allocation-breakdown-stress
```

ASan commands on Windows require the Visual Studio x64 environment. Game assets
and native Vulkan validation are local requirements; hosted CI checks the same
CPU-only cases on Windows/Linux with ASan off/on. Consult the PR checks and retained
CTest artifacts for the exact reviewed commit. Logs/game data are not committed.

## Local results

On the Radeon 880M development host (integrated GPU):

- Ordinary desktop suite: 333/333 passed. After the final teardown/retirement
  assertions, rebuilt and reran the 11 affected ledger/native tests: 11/11 passed.
- Full ASan suite: 332/332 passed. SDK-free ordinary and ASan: 80/80 each.
- One native process/device, 300 lifecycle cycles, 100 warmup: every checkpoint
  required zero live global/class counters and zero accounting errors. RSS first/
  last post-warmup window medians were 265,707,520 / 270,301,184 bytes (+4,593,664,
  approximately 4.38 MiB); sampled peak 331,481,088 bytes. This overlapped the full
  ASan test process. RSS includes allocator/driver caches, varies with concurrency
  and is neither physical VRAM nor proof against every leak.

### Native streaming workload

Map 100, 512 visible + 512 prefetch actors, 2,048-identity catalog, 6,000 updates,
replacement every 1,200 updates, deterministic fingerprint 835595881056024373.
VSync disabled (`immediate`); validation disabled for timing. Both budgets completed
with zero accounting errors/failed loads and 5,999 GPU timing samples with none
missing. FPS is descriptive, not a paired streaming performance claim.

| Animation RGBA budget | Live image payload | Live image allocation | Extra over payload | Whole-run FPS | Rolling p99 frame ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 64 MiB | 107,445,312 B | 134,742,784 B | 25.41% | 238.638 | 9.2562 |
| 512 MiB | 572,717,880 B | 746,607,872 B | 30.36% | 203.372 | 9.6101 |

These include map/outline and other live sprite images, not just animation-budget
payload. The 64 MiB snapshot includes an in-progress upload (64,944 staging bytes),
which is separately counted; image payload includes the full reserved image canvas.
Ready visible coverage was 21.07% / 78.77% respectively; budget pressure remains
intentional, and this change does not increase readiness or reduce memory.

Per-class live costs (MiB, rounded; 512 MiB case):

| Class | Images | Logical frames | RGBA | Allocation | Extra | Share of image extra |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Single-frame | 3 | 3 | 38.477 | 39.015 | 0.539 | 0.32% |
| Multi, <= 4 KiB/frame | 0 | 0 | 0 | 0 | 0 | 0% |
| Multi, > 4 to 16 KiB/frame | 452 | 3,948 | 51.079 | 65.604 | 14.524 | 8.76% |
| Multi, > 16 to 64 KiB/frame | 776 | 8,346 | 204.064 | 276.121 | 72.057 | 43.45% |
| Multi, > 64 KiB/frame | 44 | 1,679 | 252.566 | 331.281 | 78.715 | 47.47% |

Unclassified remained zero in both runs; class image/allocation sums exactly
matched the global image totals. At 64 MiB, the corresponding shares of extra
were 2.07%, 0%, 16.15%, 56.53% and 25.25%. In this resident sample, > 16 KiB/frame
animations therefore account for 90.92% / 81.78% of extra image allocation at
512 / 64 MiB. This does not uniquely distinguish row pitch, layer layout or minimum
allocation granularity. It is not an installed-catalog census, and pooling alone
cannot eliminate padding internal to a VkImage. Validate any later layout change
against native fractional sampling and matched workloads before changing budgets.

### Fixed preloaded draw control

Retained exact merged #88 client versus this client, run in the order before A,
after A, after B, before B with no concurrent tests/builds. Each uses map 2100,
1280x720, VSync disabled, density 10, 1,024 unique resources, 120 warmup and 600
sample frames through `tools/benchmarks/crowd_benchmark.py --densities 10 --unique
1024 --repeats 1 --gpu-timing`. All runs draw the same 6,050 actors / 6,833 calls;
GPU timing has no missing results.

| Run | FPS | Rolling p99 frame ms | GPU interval mean ms |
| --- | ---: | ---: | ---: |
| Before A | 149.141 | 8.8099 | 3.11187 |
| After A | 144.093 | 10.4977 | 3.11845 |
| After B | 153.874 | 8.8665 | 3.15743 |
| Before B | 143.997 | 10.8738 | 3.10429 |

There is no consistent FPS regression across the two order groups, and no
performance gain is claimed. This small control does not establish statistical
equivalence or performance on dedicated 4/8 GiB GPUs. Full positive native test
logs contain no new Vulkan validation or ASan diagnostics and no skipped cases.
