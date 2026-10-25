# Bounded idle-cache selection validation

Base: merged PR #79 (`385fa4d`). Animation streaming retires at most eight entries
per pressure scan, but previously materialized and sorted every idle cache entry.
This change keeps only the oldest K candidate iterators in a max-heap, then orders
that prefix oldest-first. When K is at least the cache size, the original full
sort path remains. K=0 returns before scanning or allocating.

For N cached / M idle entries the bounded path costs O(N + M log K + K log K)
and O(K) temporary iterator storage, rather than O(N + M log M) and O(M).
It still traverses the map and checks ownership: this is not an O(1) LRU list,
a fixed CPU-time budget, an allocation-free scan or a new residency policy.
Eight candidates occupy eight iterator slots, not eight extra strong references.

## Ownership and failure behavior

Selection stores only map iterators. The cache remains the sole cache owner;
`use_count()==1` identifies CPU-idle candidates, not GPU completion. Selection
still happens before the barrier. A throwing barrier leaves all entries intact;
a pin acquired by the barrier is rechecked before transfer. The receiver retains
the selected owner before the cache erases it. If transfer throws, the failing
entry stays cached and earlier transfers remain committed. GPU fences and the
keyed retirement queue are unchanged. No renderer, submission or background job
is introduced. Callbacks still must not mutate cache membership.

Three new CPU-only tests cover exact full-LRU expectations across shuffled touch
order, byte/count targets, K=0/1/8/127/128/999, held resources and zero payloads;
barrier failure and a newly acquired pin; and partial transfer failure followed
by retry and fence-ordered collection. Weak references verify both preservation
and eventual destruction. Existing upload/cancel/restore/destruction tests remain
in the full test suites.

## Reproduction

Run from an x64 Visual Studio developer environment on Windows:

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
cmake -S tools/benchmarks/idle_cache -B build/idle-cache-benchmark -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/idle-cache-benchmark
./build/idle-cache-benchmark/idle_cache_benchmark.exe
```

The benchmark is a standalone C++20 target with no asset/window/Vulkan dependency.
Build the same source on the base and candidate with identical compiler/options.
Alternate retained old/new binaries three times, with no concurrent tests/builds.
Each row has two warmups and 101 measured retirements; construction, LRU touches
and remaining cache destruction are excluded. The measured operation includes
selection, callback and selected entry erasure. The callback checks the exact
oldest-first sequence and emits a checksum, compared across builds. Zero/all
victim rows guard the early-return/unbounded cases. Microseconds are operation
cost, not a frame-time or FPS gain.

Native controls use map 100, 6,000 frames, 512 visible plus 512 prefetch demands,
2,048 catalog identities and replacement every 1,200 frames at 64/512 MiB. Run
base then candidate with VSync/validation disabled and timestamp diagnostics on:

```powershell
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/idle-stream-after
python tools/StreamingStress/run_stress.py --client build/asan/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 --frames 600 --leg-frames 60 --replace-frames 120 --catalog 512 --validation --output build/idle-asan-smoke
```

The separate ASan/Vulkan smoke uses runner defaults of 128 visible/160 prefetch.
The host is Windows with an AMD Radeon 880M integrated GPU; these runs are not
4/8 GiB dedicated GPU capacity tests. Cache accounting and process RSS are not
physical VRAM measurements. MSVC ASan and clean validation do not establish
exhaustive leak freedom; explicit owner, weak-reference and teardown checks
complement them.

## Results

Final normal suite: **285/285 passed**, 62.35 s. The normal configuration has one
additional Python environment test. Final ASan: **284/284 passed**, 260.18 s.
Complete positive logs from both suites contain no VUID, Vulkan validation error
or ASan report.

Three interleaved base/candidate pairs, median of each run's median / p95, in
microseconds (MSVC Release). All checksums and exact oldest-first sequences match:

| Idle entries | Victims | Base median | Candidate median | Base p95 | Candidate p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 512 | 8 | 9.1 | 1.9 | 10.7 | 2.1 |
| 4,096 | 8 | 175.0 | 14.0 | 206.6 | 14.3 |
| 32,768 | 8 | 2,007.6 | 158.9 | 2,067.8 | 174.3 |
| 512 | all | 29.9 | 29.5 | 32.8 | 33.8 |
| 4,096 | all | 422.5 | 427.5 | 457.7 | 475.7 |
| 32,768 | all | 4,285.6 | 4,010.4 | 4,428.1 | 4,433.2 |

The bounded operation is about 79-92% shorter in this all-idle corpus. It also
limits candidate storage to eight iterator slots instead of materializing every
idle entry. Unbounded rows do not improve uniformly and are not the optimization
target. Zero-victim medians read 0.0 us (p95 0.1 us) at this timer's resolution;
this means below the measurement resolution, not literal zero execution time.
The production scan still includes held entries and may benefit less when most
entries are held; this benchmark is not a universal cache-time guarantee.

Fresh map-100 controls, one run per budget/version:

| MiB | Version | FPS | Max frame ms | >33 ms | Visible coverage | End pending | Peak accounted bytes | Peak process RSS bytes |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 64 | #79 | 686.780 | 19.0585 | 0 | 21.56% | 843 | 67,108,840 | 464,818,176 |
| 64 | Candidate | 664.754 | 17.3411 | 0 | 21.17% | 808 | 67,108,464 | 473,735,168 |
| 512 | #79 | 593.492 | 20.3346 | 0 | 79.85% | 109 | 536,870,864 | 975,589,376 |
| 512 | Candidate | 603.540 | 19.0267 | 0 | 79.47% | 87 | 536,870,876 | 969,150,464 |

FPS direction changes by budget; do not claim a frame-rate improvement. This
reduces a throttled maintenance operation, not all-frame rendering cost. No frame
exceeds 33 ms in these short runs, but that is not a promise of no future stalls.
Partial visible coverage and pending demand remain resource-policy limitations.
Reported successful-load tails exclude withdrawn/pending demand and are not
interpreted as a quality or latency improvement here.

All four performance runs use IMMEDIATE presentation with VSync off, have zero
load failures, stay within configured RGBA/reservation budgets, and report 5,998
GPU samples each with zero missing results. Workload fingerprint is
`835595881056024373` in all four. GPU means (64 before/after; 512 before/after)
are 0.455084/0.461054 and 0.614720/0.607204 ms, with a 512-sample retained window;
these are not whole-session GPU averages. See [timing scope](gpu-frame-timing-validation.md).

The separate 600-frame ASan/Vulkan-validation smoke has zero load failures,
peak accounted bytes 59,030,808, and 598 GPU samples with zero missing results.
All five native runs log `Engine close`; complete native logs contain no VUID,
Vulkan validation error or ASan report. Existing missing Steam overlay JSON
loader warnings remain and are not Vulkan VUID errors.

Local ignored evidence: `build/idle-final-{dev,asan}-tests.log`,
`build/idle-final-selection-{before,after}-{1,2,3}.csv`,
`build/idle-final-stream-{before,after}/`, `build/idle-final-asan-smoke/` and
`build/idle-final-summary.txt`. Preliminary `idle-*` files without `final` are
superseded. Binaries and logs are not committed.

Baseline client SHA-256:
`ec7c65d58fe7e49d561f1ee039098d34b1fc3474117ba16283201a82af27c7dc`;
final candidate:
`2e5472adbe757ef24cf21235239a9c175ea05c92e333febfecf025b913649ed5`.
