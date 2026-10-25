# Map ready-index scheduling validation

Base: merged PR #76 (`4357a7b`). This is a CPU sorting change. It preserves the
existing footprint constraints, complete graph membership (including offscreen
nodes), baseline index order, deterministic cycle fallback and draw payload.
No sprite batching, graphics API, upload, residency, gameplay or public tool API
changes are included.

## Change and correctness boundary

The topological sorter previously reserved an N-element min-heap of ready
baseline indices. Every selected node must be the lowest currently ready index;
FIFO ordering would change painter order. `MapReadyIndices` uses 64-bit membership
words, with each upper-level bit marking a nonempty word on the level below.
Extraction follows the lowest set bit from the top down and clears empty-word
markers on its way back up. Insertion propagates only when a word was empty.
This preserves the exact minimum-index selection, including newly ready indices
below the previously extracted index. Duplicate insertion is idempotent.

The sorter still builds the same edges/incoming counts. Empty-ready cycles still
force the same lowest not-yet-emitted baseline index, and emitted nodes still
ignore later incoming-edge updates. Only ready membership/selection changes.
The existing exhaustive all-pairs reference retains its independent min-heap and
separate adjacency/permutation implementation; it is not changed to match the
new representation.

Insert/extract visit at most max(1, ceil(log64(N))) levels for nonempty input; the heap previously used
O(log2(N)) comparisons for extraction. This does not change the sorter's overall
asymptotic bound: anchor/sweep sorting remains O(N log N), and mutually overlapping
large-footprint graphs can still have quadratic edges. Small/sparsely ready
workloads need not improve. The point-only fast path remains unchanged.

The helper owns one vector of numeric words plus fixed metadata for the platform
index width. It uses portable C++20 `std::countr_zero`, not Windows-specific or
GPU APIs. All inserted indices must be within the last reset count. Reset clears
membership and reuses capacity; zero-size reset is safe. Explicit clear or scene
sorter destruction releases capacity. No entity, renderer pointer, resource or
asynchronous work is retained. Dynamic retained-byte reporting includes the word
vector capacity, but excludes object metadata/allocator overhead as before.

## Tests

Three new windowless tests cover:

- Empty state, duplicate insertion, exact 63/64/65, 4095/4096/4097 and 262145-index
  hierarchy/tail boundaries, descending insertion and ascending extraction,
  reuse after zero-size reset, shrinking and explicit release.
- 300,000 mixed insert/extract operations compared with independent `std::set`
  membership/order, including newly inserted lower indices and empty transitions.
- Moving fractional footprints and shuffled inputs across 64-node boundaries,
  matching the unmodified exhaustive graph reference's order and cycle count.

Existing tests also compare all draw-item fields and cycles across 120 changing
snapshots, 1,024/6,000 dense actors with buildings, wide sprites, strict touching
edges, the house-front occlusion case, retained-capacity reuse, renderer recording
failure/reuse and weak resource ownership.

## Reproduction

Use a Visual Studio x64 developer shell; ASan runs need its runtime environment.

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/ready-crowd --densities 10 --unique 1024 --repeats 3 --gpu-timing
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/ready-stream
```

The tests and binaries run without Python except when explicitly using the
benchmark/stress launcher. Performance runs require actual IMMEDIATE presentation,
VSync off and validation off; both versions enable the same CPU/GPU diagnostics.
Native measurements run after builds/tests without concurrent GPU workloads.
Dedicated 4/8 GiB GPU capacity and Android/Linux behavior are not tested on this
Windows/Radeon 880M integrated-GPU development host.

## Results

### Fixed crowd

Six interleaved fresh-process runs (before1/after1/before2/after2/before3/after3),
map2100, 1280x720, 6,050 actors, 1,024 preloaded appearances, 120 warmup +
600 measured frames. Both variants enable `--gpu-timing`. All manifests and
6,833 last-frame draw counts match, with complete reports and actual IMMEDIATE
presentation. Reports retain 512 recent CPU/GPU timing samples independently of
the benchmark's 600-frame window; see [GPU timing semantics](gpu-frame-timing-validation.md).

| Version / repeat | FPS | Sort mean ms | CPU record mean ms | CPU fence wait mean ms | GPU mean ms | Frame p99 ms | Frame max ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Before 1 | 346.661 | 1.42857 | 2.08740 | 0.480883 | 2.29021 | 3.8989 | 4.1573 |
| After 1 | 347.974 | 1.22359 | 1.90494 | 0.640689 | 2.29465 | 4.0947 | 4.9517 |
| Before 2 | 348.829 | 1.45208 | 2.11966 | 0.444381 | 2.30404 | 3.9241 | 4.1316 |
| After 2 | 348.452 | 1.22397 | 1.90657 | 0.648198 | 2.29593 | 4.0622 | 4.3365 |
| Before 3 | 339.311 | 1.47089 | 2.18752 | 0.411966 | 2.33159 | 4.2082 | 5.0886 |
| After 3 | 346.942 | 1.22229 | 1.91772 | 0.650165 | 2.29462 | 3.9446 | 5.1803 |

Median sort mean falls **1.45208 -> 1.22359 ms (15.7%)** and CPU recording falls
**2.11966 -> 1.90657 ms (10.1%)**. Median FPS changes **346.661 -> 347.974**
(0.38%, within observed variation). Larger fence waits offset the saved CPU work
in this workload, consistent with existing GPU/presentation backpressure; this
is CPU headroom, not evidence of a large FPS improvement or a GPU utilization
measurement. Sorting is included in recording, so these savings are not additive.
Worst-run p99 is 4.2082 -> 4.0947 ms, but the maximum frame worsens slightly
5.0886 -> 5.1803 ms. No sampled frame exceeds 33.33 ms. No uniform tail-latency
improvement is claimed.

Peak process RSS spans 697,475,072-736,886,784 bytes before and
684,072,960-691,572,736 after. These process/allocator differences cannot be
attributed to the much smaller ready-set payload change and do not measure VRAM
or prove leak freedom. Host clocks/background activity are not controlled. The
contemporary baseline differs substantially from PR #76's historical session;
use the interleaved controls, not cross-session FPS, for attribution.

Before executable SHA-256:
`46da56dcd8beb651782c3b624f1070f0d7e46258710bf67aa10ca9e1aedaad82`.
After executable SHA-256:
`c018e52911c2a0a9514f41ecdb427be0250258c8738d370da59fb0472b6d6757`.
Raw INIs/logs/reports/manifests/hashes: ignored `build/ready-crowd-{before,after}-{1,2,3}`.


### CPU-only synthetic sort and scratch

Supplementary MSVC `/O2 /std:c++20` diagnostic, no window/GPU, three interleaved
before/after process pairs. Each process reconstructs the same input before each
sort, warms 10 iterations and averages 120 timed sorts (input copy excluded).
Actors have point anchors `(i % 31, (i / 31) % 23)`, 64x120 images and (-32,-110)
offsets. Twelve 3x4 buildings at `(3+i%4, 4+i/4)` have 300x350 images and
(-150,-300) offsets. All IDs are unique; values below are median process means.
This is a synthetic graph, not a native crowd FPS or GPU-capacity measurement.

| Actors + 12 buildings | Before ms | After ms | Before retained bytes | After retained bytes |
| --- | ---: | ---: | ---: | ---: |
| 1,024 | 0.0671450 | 0.0676433 | 122,284 | 114,140 |
| 6,000 | 0.992023 | 0.826583 | 681,180 | 633,860 |
| 20,000 | 3.56386 | 2.99961 | 2,057,932 | 1,900,388 |

The large cases improve by about 16.7% / 15.8%; the small case is slightly slower
(0.7%). No universal speedup is claimed. The 6,000-actor ready storage changes
from 48,096 bytes of reserved indices to 776 bytes of words; total scratch falls
47,320 bytes. At 20,000 actors total scratch falls 157,544 bytes. Counts include
vector capacity, not allocator overhead or fixed object metadata. These are CPU
scratch reductions, not MB-scale asset/VRAM savings. Raw diagnostic source,
compiler log, executables and per-repeat results remain in ignored
`build/bench-map-ready*` and `build/ready-micro-{before,after}-{1,2,3}.log`.

### Moving map100 streaming

One 6,000-frame coverage run per budget/version, 512 visible + 512 prefetched
actors, 2,048-entry catalog, 240-frame route legs and replacement every 1,200
frames. Fingerprint `835595881056024373` matches all four runs. This accelerated
world-step test is not a long wall-clock soak or real network-latency simulation.

| Version / MiB | Lifetime FPS | Visible coverage | Pending at end | Largest frame ms | Peak accounted bytes | Peak process RSS bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Before / 64 | 649.925 | 21.60% | 844 | 18.1912 | 67,108,056 | 461,447,168 |
| After / 64 | 696.541 | 21.39% | 835 | 17.8257 | 67,108,832 | 478,863,360 |
| Before / 512 | 593.591 | 79.82% | 113 | 18.8953 | 536,870,268 | 943,534,080 |
| After / 512 | 607.332 | 80.16% | 108 | 18.7155 | 536,868,912 | 991,248,384 |

| Version / MiB | Recent sort mean ms | CPU record mean ms | GPU mean ms |
| --- | ---: | ---: | ---: |
| Before / 64 | 0.387544 | 0.560729 | 0.451655 |
| After / 64 | 0.345002 | 0.502118 | 0.451855 |
| Before / 512 | 0.419855 | 0.590309 | 0.613373 |
| After / 512 | 0.375905 | 0.546758 | 0.612538 |

All four complete with no reported animation failure, no frame above 33.33 ms,
5,998 completed GPU samples and zero missing query results; accounting stays
within the configured animation budgets. Timings in the second table are recent
512-sample windows, not whole-run means. FPS, coverage, pending outcomes and RSS
vary with asynchronous readiness and host conditions; one run per cell is not a
statistically established streaming speedup. New-process peak RSS is higher in
these runs and cannot be inferred from the much smaller numeric queue payload.
Physical GPU residency/driver allocations are not measured. Substantial missing
appearances and pending demand remain under low budgets; no quality-policy fix
is claimed. Raw INIs/logs/reports: ignored `build/ready-stream-{before,after}`.

## Final verification and limits

Full normal CTest **279/279** and MSVC ASan **278/278** pass. The normal-only Python
session target accounts for the difference. Targeted map/ready-index coverage
passes **19/19**. Complete verbose logs are ignored `build/ready-dev-tests.log`,
`build/ready-asan-tests.log` and `build/ready-targeted.log`.

Separate ASan + Vulkan-validation native smoke: map100, 600 frames, 64 MiB,
512-entry catalog, 60-frame legs and replacement every 120 frames. It completes
with no animation failure, normal engine closure, 598 GPU samples and no missing
query results. Its 179.02 FPS / 17.36 ms largest frame are instrumented diagnostics,
not performance comparisons. Artifacts: ignored `build/ready-asan-smoke`.

All 11 native comparison/stress logs close normally. Full normal/ASan verbose
logs and all native logs have no VUID, validation-error or ASan-error diagnostics.
The host's existing missing Steam layer-manifest messages remain. Resource
ownership tests check clip release after recording failure/entity destruction;
CPU scratch tests check capacity reuse and explicit release. MSVC ASan is not
exhaustive leak/race detection, and process RSS cannot establish leak freedom.
No new resource orphan was observed. GPU object ownership is unchanged.

Next rendering work must distinguish remaining CPU costs from GPU/backpressure
on its own current workloads. This change creates CPU headroom without a material
fixed-crowd FPS gain; it does not justify a blanket CPU-only bottleneck claim.
Batching/instancing must preserve transparent order, native state invalidation and
frame-local lifetime, and needs a separate measured trial.
