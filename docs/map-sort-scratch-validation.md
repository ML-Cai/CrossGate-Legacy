# Map sorting scratch validation

Base: develop `ab64c9e` (merged PR #73). Validated 2026-10-24 on Windows,
RelWithDebInfo, AMD Radeon 880M integrated GPU. Dedicated 4/8 GiB GPU capacity
is not measured on this machine.

## Change

MapWorldRenderer now retains one CPU MapDrawSorter. Sweep indices, image rectangles,
prefix right bounds, flat adjacency edges, incoming counts, ready heap, emitted flags and output indices
reuse scene-owned capacity. A permutation rotates items in place, using one item
on the stack instead of a second array of renderer-bearing draw items. Borrowed
renderer pointers remain in the frame's item array only, which is cleared on
success and exceptions by the existing recording guard.

The anchor key, overlapping pairs, ground-footprint relations, lowest-index ready
choice and deterministic cycle fallback are unchanged. A monotone prefix maximum
of image right edges allows a binary search past rectangles that all end at or
before the current large object's left edge. A wide earlier sprite is retained
by its right bound, regardless of its far-left starting position. This avoids
scanning definitely disjoint prefixes without removing any overlap constraint. No map objects are
culled before the constraint graph, and no blending/draw order or GPU ownership
policy changes. Dropping offscreen nodes before topological sorting requires
separate analysis because they can mediate constraints between other nodes.

The scratch owns numbers only, never resources, entities or submitted work.
Its capacity follows the largest scene graph and is released on explicit clear
or scene destruction. MapWorldRenderer.clear also releases its item array. This
is not a fixed CPU byte budget: graph storage remains O(n + E), with a quadratic
worst case for many mutually overlapping large footprints. Flat edges carry two
indices, so reducing allocation churn does not imply smaller memory on every
graph. No count/quality limit is introduced to hide that worst case.

## Tests

Four new windowless tests compare against an independent exhaustive all-pairs
reference with separate adjacency/permutation implementation. They cover 120
changing snapshots, actual cyclic graphs, deterministic ties, item payload,
1024/6000 dense actors plus buildings, repeated shuffled input, stable retained
capacity, explicit release/reuse, very wide earlier sprites and exact edge contact. The map recording failure/reuse test now
includes a large footprint, checking cleanup and weak clip ownership through the
graph path. MapRenderTest is included in the memory-labelled CTest selection.

Full normal CTest **264/264** and MSVC ASan **263/263** passed. The existing
normal-only Python session test accounts for the difference. Complete positive
logs contain no VUID/validation-error/ASan error markers. Local logs:
`build/sort-index-dev-tests.log`, `build/sort-index-asan-tests.log`, and the 19-test
targeted CPU recording/sort run `build/sort-index-targeted.log`. After making
test-data integer conversions explicit for portability, all 16 MapRenderTest cases
were rebuilt and passed again in normal/ASan builds (`build/sort-final-cpu-*-tests.log`).

MSVC ASan checks invalid memory accesses, not exhaustive leak detection. Explicit
scratch release, recording-error recovery, weak clip ownership and native cleanup
checks complement it; process RSS alone cannot establish leak freedom.

Run from the x64 Visual Studio developer environment (ASan needs its runtime):

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
```

## Native measurements

Before binaries were retained from the source identical to the #73 squash merge.
Both versions run separately, without concurrent builds/GPU tests. VSync is off.
Fixed crowd runs use actual IMMEDIATE presentation, map `map/0/2100.dat`, root
`C:/CrossGate-Legacy`, 1280x720, ten actors per grid, 1024 preloaded appearances,
120 warmup and 600 sample frames. The runner validates resource identities,
actor/drawn counts, completion and uncapped presentation. This holds appearance
readiness fixed rather than crediting a different visible-resource workload.

Streaming runs use map 100, 512 visible + 512 outer actors, catalog 2048, seed 3169,
6000 world updates, 240-update route legs and replacement every 1200 updates.
The 16.666667 ms simulated step is accelerated world time, not real network
latency. Frame and sorting times are CPU-observed, including GPU backpressure,
not GPU timestamp queries or displayed FPS.

| Version | Run | FPS | Sort mean ms | Frame P99 ms | Frame max ms | Peak process MiB |
| --- | --- | --- | --- | --- | --- | --- |
| before | 1 | 232.402 | 2.02239 | 7.0321 | 8.3429 | 638.2 |
| before | 2 | 242.161 | 1.9916 | 5.6642 | 6.2355 | 642.9 |
| before | 3 | 242.277 | 1.99707 | 5.4642 | 6.8313 | 630.1 |
| after | 1 | 246.892 | 1.76779 | 5.7076 | 6.5047 | 678.9 |
| after | 2 | 230.568 | 1.77657 | 7.4237 | 8.6525 | 665.4 |
| after | 3 | 246.276 | 1.77004 | 6.0879 | 7.7879 | 651.9 |

Median sort mean falls **1.99707 -> 1.77004 ms** (about 11.4%). Median FPS rises
**242.161 -> 246.276** (about 1.7%). The slower after run is retained: worst-run
P99 changes **7.0321 -> 7.4237 ms**, so these measurements do not demonstrate a
uniform P99 improvement. All six cases draw exactly 6050 actors with identical
1024 resource identities and have zero over-33ms sample frames. Three repetitions
are a limited development measurement, not a statistical guarantee.

An exploratory reuse-only version, before adding the prefix bounds, measured
sort means 1.87573/1.87315/1.86694 ms and FPS 246.661/245.8/247.332 against earlier
before means 1.98809/2.00145/1.99823 ms. This motivated checking the remaining
prefix scan. Those local artifacts remain under `build/sort-crowd-*`; they are
not substituted for the final before/after comparison above.

| Version | MiB | FPS | Sort mean ms | Frame max ms | Ready visible % | Visible miss P99 ms | Pending |
| --- | --- | --- | --- | --- | --- | --- | --- |
| before | 64 | 376.115 | 0.764803 | 22.5594 | 21.96 | 3349 | 801 |
| before | 512 | 342.287 | 0.856095 | 23.588 | 79.78 | 357 | 110 |
| after | 64 | 428.705 | 0.500643 | 22.1491 | 21.55 | 3113 | 811 |
| after | 512 | 375.376 | 0.549937 | 24.9677 | 79.54 | 430 | 125 |

| Version | MiB | Started | Completed | Withdrawn | Pending | Ledger peak bytes | Process peak MiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| before | 64 | 14114 | 1178 | 12135 | 801 | 67108676 | 429.9 |
| before | 512 | 6583 | 1875 | 4598 | 110 | 536870868 | 885.5 |
| after | 64 | 14034 | 1163 | 12060 | 811 | 67108436 | 446.8 |
| after | 512 | 6609 | 1880 | 4604 | 125 | 536870012 | 921.6 |

All four streaming cases complete with matching input fingerprints, no failed
loads/misses and zero over-33ms gameplay frames. Outcome conservation holds in
each row: started = completed + withdrawn + pending. Sorting mean falls roughly
35-36% in these moving views. Coverage is slightly lower in both after cases;
FPS changes the real CPU/worker time available for each fixed-step snapshot, so
these are not fixed-render-work or deterministic loading-order comparisons.
Pending/censored misses and latency remain visible rather than being treated as
cache hits or successful loads. This PR changes no resource admission/eviction rule.

Some process RSS peaks increase. RSS includes caches, allocator and driver state,
not just sort scratch, and is not physical VRAM or evidence of leak freedom. All
RGBA ledger peaks remain within budget. Retained CPU scratch is released explicitly
in tests; its worst-case graph memory remains as described above.

Final native artifacts are `build/sort-final-crowd-{before,after}/summary.json` and
`build/sort-final-stream-{before,after}/summary.json`, with per-run INIs, reports
and complete logs in each directory. Run each executable sequentially in fresh
directories with the same installed corpus and no manual input:

```powershell
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --map map/0/2100.dat --output-dir build/sort-final-crowd-after --densities 10 --unique 1024 --repeats 3
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/sort-final-stream-after
```

Use an equivalent #73 build for before results. The runners preserve raw frames,
resource identities, coverage/outcomes and logs; they do not invent performance
pass thresholds. Do not compare FPS alone across different readiness/actor loads.

## ASan + Vulkan validation native smoke

An additional 600-update native run, 128 visible/160 outer, catalog 512, 64 MiB,
leg 60/replacement 120, completed with normal engine shutdown. Outcomes:
1033 started = 285 completed + 622 withdrawn + 126 pending, no failures.
Peak RGBA ledger **59,478,264 bytes** remains below 64 MiB. Pending state is reported
before normal shutdown cancellation/cleanup. All eleven final native logs reached
engine close and contain no VUID/validation-error/ASan error markers.

```powershell
python tools/StreamingStress/run_stress.py --client build/asan/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 --frames 600 --leg-frames 60 --replace-frames 120 --catalog 512 --validation --output build/sort-final-asan-smoke
```

Run from the Visual Studio environment so the ASan runtime is available. The
instrumented smoke is a lifecycle/validation check, not a performance comparison.
The next rendering step should profile command recording/state/submission costs
while preserving this ordering and the existing submission-completion lifetimes.
