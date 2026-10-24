# Bounded animation preparation validation

Base: develop `cbedbd6` (PR #72). Measured 2026-10-24 on the development PC,
AMD Radeon 880M integrated GPU. This is not a measurement on a dedicated 4/8 GiB GPU.

## Change and ownership

Remote appearances can prepare one next clip on the existing CPU worker while
one previous clip uploads. The scheduler excludes the uploading key when choosing
lookahead, preserves same-region work and aging, and cancels obsolete or
visible-displaced prefetch preparation without cancelling submitted GPU work.
No additional worker or FIFO is introduced. Scene entry and synchronous acquisition
keep their existing interfaces.

The combined uploading RGBA payload and CPU reservation remain at most 32 MiB.
The CPU planner receives the smaller of free texture capacity and the remaining
32 MiB working allowance. Its exact reservation starts before pixel decoding;
metadata alone occupies the slot at zero pixel bytes. Preparation may be skipped
or fail temporary admission and retry after the previous upload completes. It
never requires doubling the working allowance to 64 MiB.

The accounting invariant is ready + retired + uploading + CPU reservation <=
configured animation budget. The one GPU staging allocation remains at most
4 MiB. This ledger counts payload/reservations, not total physical VRAM, driver
allocations, map textures, reader caches or staging. Neither this cap nor process
RSS constitutes a complete host/device memory bound.

Only the owner thread touches GPU state. Upload polling precedes draining CPU
cancellation, so a reader-locked cancelled job cannot prevent upload progress.
A cancelled worker retains its reservation until collection; builder scratch is
cleared only after CPU access ends. Promotion moves CPU timing to upload timing;
completed uploads and cancelled lookahead have separate timing ownership. Shutdown
still joins/cancels CPU access before reader destruction and releases submitted
GPU resources through the existing completion/lifetime boundaries.

## Validation

- Full normal CTest **260/260** and MSVC ASan CTest **259/259** passed.
  Complete positive logs contain no VUID/validation-error/ASan error markers.
  The existing normal-only Python session test accounts for the difference.
- One windowless scheduler test verifies exclusion without losing age, visible
  priority, capacity constraints or the snapshot bound.
- Four actual Vulkan lifecycle tests exercise simultaneous exact reservations and
  separate successful timings; zero-capacity fallback; zero-byte reader-locked
  cancellation while upload progresses; reusing the worker after cancellation;
  snapshot removal; and temporary admission failure followed by exact-fit recovery.
  Existing retired reuse, scene restart, budget bypass and churn tests also pass.
- MSVC ASan checks invalid memory accesses; these results are not proof that every
  CPU/driver allocation is leak-free. Tests check drained slots and ownership
  accounting after cancellation/clear, and actual Vulkan cleanup logs are inspected.

Full local logs: `build/preparation-dev-final-tests.log` and
`build/preparation-asan-final-tests.log`. Run with the Visual Studio x64 developer
environment (required for the MSVC ASan runtime):

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
```

## Native comparison

Both versions run separately, without concurrent builds/GPU tests, with VSync off,
1280x720, map `map/0/100.dat`, root `C:/CrossGate-Legacy`, seed 3169 and 6000 fixed
world updates. The small case has 128 visible + 160 outer actors, catalog 1024,
replacement every 600 updates. The large case has 512 + 512, catalog 2048,
replacement every 1200. All actors can share grid cells up to the existing ten
remote actors/grid bound. Same-group input fingerprints match across all budgets
and binaries. The before executable was retained before rebuilding; its source
matches the squash-merged #72 base.

World time advances at 16.666667 ms/update, much faster than real-time on this PC;
this is accelerated churn, not a server/network latency simulation. Coverage is
ready actor-updates / visible actor-updates. Successful miss samples retain only
the most recent 256 completions; withdrawn and unresolved demands remain separate.
Comparing successful means alone can hide capacity-constrained unfinished work.

| Actors | Version | MiB | FPS | Max frame ms | Over 33 ms | Ready visible % |
| --- | --- | --- | --- | --- | --- | --- |
| 288 | before | 64 | 900.227 | 19.0659 | 0 | 44.02 |
| 288 | before | 512 | 932.698 | 18.7322 | 0 | 87.71 |
| 288 | after | 64 | 854.305 | 18.2307 | 0 | 47.35 |
| 288 | after | 512 | 937.631 | 18.6305 | 0 | 94.01 |
| 1024 | before | 64 | 565.223 | 19.3083 | 0 | 19.72 |
| 1024 | before | 512 | 516.361 | 20.3456 | 0 | 68.80 |
| 1024 | after | 64 | 534.934 | 19.1656 | 0 | 21.82 |
| 1024 | after | 512 | 493.04 | 19.5118 | 0 | 80.46 |

| Actors | Version | MiB | Miss mean ms | Queued mean ms | Visible P99 ms | Peak visible age ms |
| --- | --- | --- | --- | --- | --- | --- |
| 288 | before | 64 | 419.855 | 412.82 | 539 | 643 |
| 288 | before | 512 | 38.0391 | 33.6172 | 227 | 454 |
| 288 | after | 64 | 433.273 | 422.418 | 596 | 703 |
| 288 | after | 512 | 39.5859 | 32.9375 | 212 | 349 |
| 1024 | before | 64 | 2228.68 | 2217.53 | 2586 | 2658 |
| 1024 | before | 512 | 392.684 | 380.801 | 536 | 2088 |
| 1024 | after | 64 | 2106.22 | 2092.72 | 2437 | 2603 |
| 1024 | after | 512 | 243.562 | 229.969 | 283 | 1716 |

| Actors | Version | MiB | Started | Completed | Withdrawn | Pending (visible) | Ledger peak bytes | Process peak MiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 288 | before | 64 | 5812 | 995 | 4570 | 247 (110) | 67108752 | 463.2 |
| 288 | before | 512 | 2396 | 1016 | 1380 | 0 (0) | 348328160 | 639.4 |
| 288 | after | 64 | 5749 | 1079 | 4429 | 241 (107) | 67108764 | 469.8 |
| 288 | after | 512 | 1869 | 1014 | 855 | 0 (0) | 348419552 | 663.9 |
| 1024 | before | 64 | 14073 | 1043 | 12188 | 842 (418) | 67108376 | 427.7 |
| 1024 | before | 512 | 7438 | 1583 | 5742 | 113 (65) | 536869444 | 883.0 |
| 1024 | after | 64 | 14251 | 1257 | 12187 | 807 (403) | 67108604 | 457.4 |
| 1024 | after | 512 | 6511 | 1905 | 4469 | 137 (71) | 536870540 | 933.2 |

All eight cases have zero failed loads/misses and zero over-33ms gameplay frames.
That is an observation of these runs, not a general no-stutter guarantee. In
particular, the historical unattributed 44.59 ms #72 trace remains in its report.
Pending counts are censored observations at the final update, not leaks or proof
of complete residency. Each row satisfies started = completed + withdrawn + pending.

The large 512 MiB case improves coverage from 68.80% to 80.46%, with more completed
misses and fewer withdrawals. Its FPS falls from 516.361 to 493.04. More appearances
are actually available to draw, so these runs do not hold rendered work constant;
neither a pure renderer overhead estimate nor a universal FPS increase follows.
At 64 MiB, capacity remains the dominant limitation; small-case visible P99 is
worse despite higher coverage. No policy evicts visible owners to hide this limit.

A second pair for the constrained small case and the large 512 MiB case reverses
execution order (after then before), to check that the result is not just one
execution ordering:

| Actors | Version | MiB | FPS | Max ms | Ready % | Miss mean ms | Visible P99 ms | Completed | Withdrawn | Pending |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 288 | after | 64 | 837.684 | 17.3828 | 47.81 | 441.723 | 566 | 1189 | 4437 | 238 |
| 288 | before | 64 | 917.39 | 18.1013 | 42.86 | 449.512 | 554 | 980 | 4622 | 251 |
| 1024 | after | 512 | 489.236 | 18.9502 | 80.14 | 361.297 | 538 | 1924 | 4502 | 124 |
| 1024 | before | 512 | 528.368 | 20.4266 | 68.93 | 322.172 | 427 | 1601 | 5749 | 107 |

These four cases also have zero failed loads and zero over-33ms frames. Coverage
and completion improvement repeat, but successful mean/P99 latency does **not**
improve consistently. For the large case, visible P99 ranges 427-536 ms before and
283-538 ms after. This change improves resource preparation throughput; it is not
a claim of universally lower tail latency or higher FPS. Larger fixed-render-load
profiles are needed to separate scheduler, upload/allocation and extra drawing costs.

Process peaks increase in several after runs. CPU working data, caches, allocator
and driver state all contribute; this report does not attribute that difference
or infer leak freedom from RSS. Ledger peaks remain within their configured
budgets. The largest observed after combined pending payload is 29,489,808 bytes,
below 32 MiB; per-update observation is not proof of all between-update peaks.
The admission/ownership tests and bounded snapshot contract address that invariant.

Local primary artifacts: `build/preparation-{288,1024}-{before,after}/summary.json`;
repeat artifacts: `build/preparation-repeat-{288,1024}-{before,after}/summary.json`.
Each directory also contains the unique input INIs, full JSON reports and logs.

Reproduce each version with the same installed corpus and no manual input:

```powershell
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --output build/preparation-288-after
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --output build/preparation-1024-after
```

Run an equivalent #72 build for before results. Repeated cases use the same command
with just budget 64 (small) or 512 (large), fresh output directories and reversed
binary order. These limited repetitions are not a statistical performance guarantee.

## Instrumented native smoke

An additional **600-update MSVC ASan + Vulkan validation** run, 64 MiB,
128 visible/160 outer, catalog 512, leg 60/replacement 120, completed and shut down
cleanly. Outcome conservation: 1031 started = 289 completed + 619 withdrawn +
123 pending; zero failed. Peak ledger 59,790,552 bytes stays below 64 MiB. The
report precedes normal cancellation/cleanup, so its pending values are expected.
All thirteen native logs (twelve normal comparisons plus this smoke) contain the
normal engine-close marker and no VUID/validation-error/ASan error markers.

```powershell
python tools/StreamingStress/run_stress.py --client build/asan/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 --frames 600 --leg-frames 60 --replace-frames 120 --catalog 512 --validation --output build/preparation-asan-smoke
```

Instrumentation changes timing substantially; its FPS is not compared to normal
builds. Visible capacity policy, long-session CPU/driver residency profiling and
render culling/submission costs remain separate follow-ups. No new thread pool,
visible eviction or cross-platform abstraction is introduced by this PR.
