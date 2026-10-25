# Finite sprite geometry validation

Base: merged PR #78 (`ef49fd4`). Finite position and scale inputs can still
produce infinity during the existing float normalization, or when the vertex
shader adds a finite origin and extent. Previously those draws still recorded
pipeline, descriptor and viewport state with the nonfinite push constants.

The renderer now rejects a nonfinite computed rectangle or far corner before
recording any sprite state. Normal coordinate arithmetic and sampling remain
unchanged. This extends the existing invalid-input ignore contract; it adds no
resource, allocation, shader, descriptor, upload, async job or ownership change.

## Regression and lifecycle verification

The actual-Vulkan regression records extreme finite X/Y positions, scale and a
finite-origin/extent case whose far corner overflows. On unchanged #78 the new
test fails: pipeline=1, descriptor=1 and viewport=2 instead of zero. The recording
is deliberately discarded, so this negative control does not execute invalid
geometry on the GPU. It establishes unwanted recording, not a reproduced driver
crash or leak. The test then submits a normal sprite and verifies its pixel,
checking recovery and tracked state through the existing teardown validation.

Reproduce from the Visual Studio x64 developer environment:

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
```

Full results and native regression controls follow below. MSVC AddressSanitizer
and clean Vulkan validation do not prove absence of all leaks. This change adds
no resource ownership; existing cancellation, slot reuse and destruction tests
remain relevant.

## Native measurement method

Windows, AMD Radeon 880M integrated GPU; this is not a dedicated 4/8 GiB GPU
capacity test. VSync and validation are disabled for performance runs, with
IMMEDIATE presentation required. GPU timestamps are enabled for both controls.
Three interleaved old/new fixed-crowd pairs use map 2100, 1280x720, 6,050 actors,
1,024 animation identities, 120 warmup frames and 600 measured frames. These are
CPU frame-loop FPS and separate timestamp intervals, not displayed FPS or additive
CPU/GPU timing components. See [GPU timing scope](gpu-frame-timing-validation.md).

The new version also traverses map 100 for 6,000 frames at each of 64/512 MiB
stream budgets: 512 visible plus 512 prefetch demands, 2,048 identities and
replacement every 1,200 frames. A separate 600-frame ASan/Vulkan-validation run
uses 64 MiB, 512 identities, 60-frame movement legs and replacement every 120
frames. Instrumented FPS is not a performance comparison.

Representative commands (repeat the crowd with the retained #78 executable):

```powershell
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/geometry-crowd-after-1 --densities 10 --unique 1024 --repeats 1 --gpu-timing
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/geometry-stream
python tools/StreamingStress/run_stress.py --client build/asan/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 --frames 600 --leg-frames 60 --replace-frames 120 --catalog 512 --validation --output build/geometry-asan-smoke
```

Transparent-padding experiments were withdrawn: GPU savings did not offset CPU
cost, and stricter eligibility selected no clips in the tested 1,024-identity
corpus. No experimental crop code, API or metadata is retained. This PR is a
correctness guard, not a rendering speedup.

## Results

- Normal: **282/282 passed** (63.79 s); ASan: **281/281 passed** (261.01 s).
  The normal-only additional Python environment test explains the count difference.
- Six valid crowd runs use identical ordered resource manifests, RGBA payload
  340,655,616 bytes, 6,050 actors drawn and 6,833 draw calls; one shared pipeline.
  Each has 719 GPU samples and zero missing results.
- All nine native runs exit cleanly with `Engine close`. Complete positive test
  and native logs contain no VUID, Vulkan validation error or ASan report.
  Existing Steam overlay JSON loader warnings are not claimed as validation-free
  loader configuration.

Three-run medians (before -> after):

| Metric | #78 | Guard |
| --- | ---: | ---: |
| CPU frame-loop FPS | 351.089 | 353.980 |
| Frame p99 ms | 3.9508 | 4.0756 |
| CPU recording mean ms | 1.91892 | 1.95272 |
| Sorting mean ms | 1.23947 | 1.23092 |
| Frame-slot fence wait mean ms | 0.604652 | 0.537693 |
| GPU interval mean ms | 2.26085 | 2.25997 |
| Process peak RSS bytes | 663,699,456 | 702,722,048 |

No crowd frame exceeds 33 ms. Recording/p99 and peak RSS are slightly higher;
throughput and GPU intervals are comparable in this short session. This is a
regression control, not evidence of a speedup, lower memory use or leak freedom.

Map-100 new-version runs:

| Budget MiB | FPS | Max frame ms | >33 ms | Visible coverage | End pending | Peak accounted bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 64 | 662.552 | 16.1001 | 0 | 21.52% | 833 | 67,108,460 |
| 512 | 605.710 | 19.8674 | 0 | 79.99% | 117 | 536,869,580 |

Both budget ledgers stay within their configured limits, with zero load failures,
5,998 GPU samples and zero missing results. Snapshot fingerprint is
`835595881056024373` in both cases. Partial coverage/pending demands remain known
resource-policy limitations; high FPS with fewer ready actors is not a quality
improvement. These runs do not establish an improvement over #78 streaming.

The ASan/validation smoke uses the runner defaults of 128 visible/160 prefetch,
finishes with zero load failures, peak accounted bytes 59,095,656 and 598 GPU
samples with zero missing results. It exercises movement, replacement and cleanup;
it is not an exhaustive long-session leak test.

Ignored local evidence: `build/geometry-{dev,asan}-tests.log`,
`build/geometry-red-test.log`, `build/geometry-crowd-{before,after}-{1,2,3}/`,
`build/geometry-stream/`, `build/geometry-asan-smoke/` and
`build/geometry-summary.txt`. Baseline executable SHA-256:
`28e6f77ba5183218c9d24f04fecdb8aad984738b078bd5734daee2f32c58e4d9`;
new executable:
`ec7c65d58fe7e49d561f1ee039098d34b1fc3474117ba16283201a82af27c7dc`.
