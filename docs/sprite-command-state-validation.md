# Sprite command state validation

Base: develop `cce6fde` (merged PR #74), Windows RelWithDebInfo, AMD Radeon 880M
integrated GPU. Dedicated 4/8 GiB GPU memory capacity is not measured here.

## Recording contract

Each native CommandBuffer now tracks the last sprite pipeline, pipeline layout,
descriptor set and viewport/scissor extent. Only consecutive unchanged bindings
are reused. The draw and its 36-byte push constants are still recorded every
time, in the same painter order. A texture A/B/A sequence rebinds all three
textures; shared pipeline and viewport can remain unchanged. Pipeline/layout
changes conservatively rebind the descriptor. There is no sorting by texture,
new batching buffer, upload allocation or resource-admission change.

Keys and three binding counters belong to one command buffer, not a renderer,
render pass, global cache or frame shared by multiple slots. Reset/begin clear
keys and counters; end/destroy invalidate keys. Counters remain readable after
end for backend tests. The state contains scalar Vulkan handle values, never
resource owners, pending work or dynamically allocated cache entries. Submitted
resource lifetime remains governed by the existing ready/retired cache and
submission-completion serials.

Getting the native `commandBuffer()` handle conservatively invalidates tracked
state. Native recording must remain on the command buffer's owner. If a retained
raw handle modifies graphics state *after* tracked sprite recording, call
`invalidateGraphicsState()` before the next sprite. Handle acquisition cannot
observe subsequent writes through a previously retained handle. Future custom
UI/native rendering must honor this boundary. Current pass/transfer/queue native
access sites use the invalidating accessor; the tracked sprite path records its
push constants and draw internally without invalidating its own keys.

## Tests

Three windowless memory-labelled tests exercise first use, repeats, A/B/A,
pipeline/layout changes, every viewport field, invalidation and independent slots.
Three actual Vulkan readback tests verify pixels and emitted binding counters:
consecutive/alternating textures, changed push constants and invalid draws;
native viewport/scissor changes through fresh and retained handles; tracked
viewport switches; repeated recordings and distinct command-buffer slots.
Existing transparent painter-order, atlas, progressive upload and pipeline churn
tests remain in the suite. Sprite fixture teardown now checks the validation
counter through device destruction, including child cleanup diagnostics.

Full normal CTest **270/270** and MSVC ASan **269/269** passed. The existing
normal-only Python session test explains the difference. Complete positive logs
contain no VUID/validation-error/ASan error markers: `build/state-dev-tests.log`
and `build/state-asan-tests.log`. The initial targeted sprite/state suite also
passed 15/15 (`build/state-targeted.log`); full suites include the strengthened
native-handle and teardown checks. MSVC ASan is not an exhaustive leak detector;
scalar ownership, actual cleanup validation and existing resource-lifecycle tests
complement its invalid-memory-access checks.

The five-draw A/A/B/B/A readback case emits one pipeline bind, one viewport/scissor
pair and three descriptor binds. Each new recording/slot emits its initial binds.
Native modifications and explicit invalidation force fresh bindings; tracked
viewport changes then update viewport/scissor without rebinding unchanged textures.

Run in the Visual Studio x64 environment (ASan needs its runtime):

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --preset asan
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
```

## Native measurements

Before source matches the #74 squash merge; its executable was retained before
rebuilding. Runs are serialized without concurrent builds/GPU tests. Fixed crowd:
map 2100, 1280x720, ten actors per grid, **6050 drawn actors/1024 fully preloaded
appearances**, 120 warmup and 600 sample frames, three repetitions each version.
The runner requires actual IMMEDIATE presentation and validates resource and draw
counts. This holds readiness fixed to separate recording costs from loading.

Streaming: map 100, 512 visible + 512 outer actors, catalog 2048, seed 3169,
6000 fixed-step world updates, route leg 240/replacement 1200, budgets 64/512 MiB.
VSync is off. The simulated 16.666667 ms/update route is accelerated churn, not
real-time network latency. CPU frame timing includes GPU backpressure and is not
a GPU timestamp or displayed-FPS measurement.

| Version | Run | FPS | Render loop mean ms | Sort mean ms | Frame P99 ms | Frame max ms | Process peak MiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| before | 1 | 249.202 | 3.83338 | 1.75908 | 5.8192 | 7.0219 | 675.1 |
| before | 2 | 247.186 | 3.85947 | 1.76742 | 5.9951 | 7.5297 | 670.6 |
| before | 3 | 243.152 | 3.91677 | 1.7789 | 6.2422 | 8.5992 | 654.1 |
| after | 1 | 302.433 | 3.12582 | 1.8085 | 5.2921 | 8.1239 | 679.0 |
| after | 2 | 311.383 | 3.04475 | 1.80328 | 4.6384 | 5.7691 | 677.9 |
| after | 3 | 309.912 | 3.05852 | 1.79983 | 4.4957 | 5.1386 | 660.3 |

Median FPS improves **247.186 -> 309.912** (about 25.4%). Median render-loop mean
falls **3.85947 -> 3.05852 ms** (about 20.8%); this includes GPU backpressure, not
just API-call CPU cost. Worst-run P99 is **6.2422 -> 5.2921 ms**. The slower first
after run is retained. All six fixed cases use identical 1024 resource identities,
draw exactly 6050 actors, and have zero over-33ms sample frames. Three development
repetitions do not establish a universal FPS or no-stutter guarantee.

| Version | MiB | FPS | Frame max ms | Ready visible % | Visible miss P99 ms | Pending |
| --- | --- | --- | --- | --- | --- | --- |
| before | 64 | 434.766 | 18.0553 | 21.51 | 3111 | 838 |
| before | 512 | 391.615 | 21.315 | 79.41 | 467 | 118 |
| after | 64 | 478.147 | 17.1377 | 21.90 | 2783 | 797 |
| after | 512 | 385.12 | 20.2674 | 79.58 | 384 | 141 |

| Version | MiB | Started | Completed | Withdrawn | Pending | Ledger peak bytes | Process peak MiB |
| --- | --- | --- | --- | --- | --- | --- | --- |
| before | 64 | 14180 | 1150 | 12192 | 838 | 67107900 | 434.7 |
| before | 512 | 6614 | 1884 | 4612 | 118 | 536870484 | 907.9 |
| after | 64 | 14086 | 1168 | 12121 | 797 | 67108668 | 448.3 |
| after | 512 | 6635 | 1910 | 4584 | 141 | 536870300 | 932.1 |

All four streaming cases complete with matching fingerprints, no failed loads or
misses and zero over-33ms gameplay frames. Outcome conservation holds: started =
completed + withdrawn + pending. At 64 MiB FPS improves; **512 MiB FPS is slightly
lower (391.615 -> 385.12)** despite the fixed-crowd gain. Dynamic residency,
asynchronous completion and real worker time per accelerated snapshot are not
held constant in this trace. Do not infer universal streaming/FPS improvement
or treat unresolved/withdrawn misses as successes. Successful visible latency
quantiles are conditional on completion; pending outcomes remain beside them.

Process peaks increase in some after cases. RSS includes caches, allocator and
driver state and is neither physical VRAM nor a leak detector. All animation
RGBA ledger peaks remain within budget; no allocation, upload, admission or
resource-owner policy was added by this scalar command-state optimization.

Local artifacts: `build/state-crowd-{before,after}/summary.json` and
`build/state-stream-{before,after}/summary.json`; each directory contains unique
or per-run input INIs, raw reports and complete logs. Reproduce each executable
sequentially with the same corpus, no manual input, and fresh output directories:

```powershell
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --map map/0/2100.dat --output-dir build/state-crowd-after --densities 10 --unique 1024 --repeats 3
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/state-stream-after
```

Use an equivalent #74 build for before results. The runners check completion and
counts/accounting but do not impose invented performance thresholds.

## ASan + Vulkan validation native smoke

An additional 600-update native run, 128 visible/160 outer, catalog 512, 64 MiB,
leg 60/replacement 120, completed with normal shutdown. Outcomes:
**1037 started = 280 completed + 628 withdrawn + 129 pending**, zero failed.
Peak RGBA ledger **59,047,848 bytes** stays below 64 MiB. The report precedes
normal shutdown cancellation/cleanup; pending values at that moment are expected.
All eleven final native logs reached engine close with no VUID/validation-error/
ASan error markers. Instrumented timing is not compared against normal builds.

```powershell
python tools/StreamingStress/run_stress.py --client build/asan/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 --frames 600 --leg-frames 60 --replace-frames 120 --catalog 512 --validation --output build/state-asan-smoke
```

The next scope should use profiles to distinguish remaining CPU recording,
sorting and actual GPU costs before introducing instanced/order-preserving batches.
The existing native interop invalidation and submission-completion lifetime
contracts must also hold for future UI/custom renderers.
