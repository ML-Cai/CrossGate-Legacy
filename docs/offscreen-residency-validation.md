# Off-screen animation residency validation

Base: develop `c96c96b` (PR #68). Measured on the same development PC,
2026-10-24. Before executable was retained from #68; its source tree is identical
to the squash-merged base. This is a correctness/lifetime fix, not a demonstrated
throughput or coverage improvement.

## Automated validation

- Normal CTest: 243/243 passed.
- MSVC AddressSanitizer CTest: 242/242 passed (normal-only Python session test
  accounts for the count difference).
- New Vulkan regression: a key shared by visible and outer actors remains pinned;
  image bounds overlapping the viewport stay pinned even with an off-screen foot;
  after both old owners move fully off-screen, a different visible clip obtains capacity
  under a budget that fits only one remote clip. Old ownership expires after
  retirement/completion, no entity is removed, promotion reuses a ready clip, and
  cleanup returns animation accounting to its original baseline.
- Existing suites cover actual frame serials, cancelled workers, submitted uploads,
  scene restart, visible-only over-budget placeholders and shutdown ordering.

No sanitizer errors were reported. ASan and accounting assertions do not prove
absence of every leak; process RSS includes allocators/readers/driver allocations,
and the animation ledger is not a physical VRAM measurement.

## Native churn measurements

Each run: map `100.dat`, 6000 scripted updates, seed 3169, 16.666667 ms simulated
step, 240-update movement legs, VSync disabled/immediate presentation, Vulkan
validation disabled. No concurrent GPU tests or builds. Catalog sizes 1024/2048,
replacement intervals 600/1200 for the two actor counts, respectively. One run per
configuration; worker completion depends on wall time, so small differences are
not evidence of a performance gain/regression. Source snapshot fingerprints match
before/after: `15372329328879399793` and `835595881056024373`.

| Visible/prefetch version | Budget MiB | Visible coverage | Lifetime FPS | Max frame ms | Budget-wait updates | Peak animation ledger MiB | Peak process RSS MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128/160 before | 64 | 41.98% | 611.2 | 18.24 | 2536 | 64.000 | 460.0 |
| 128/160 before | 512 | 86.46% | 600.3 | 19.50 | 0 | 332.150 | 627.4 |
| 128/160 after | 64 | 42.20% | 602.6 | 19.80 | 2515 | 64.000 | 447.5 |
| 128/160 after | 512 | 86.07% | 611.4 | 17.69 | 0 | 332.150 | 651.0 |
| 512/512 before | 64 | 19.09% | 442.8 | 18.94 | 3328 | 63.999 | 392.0 |
| 512/512 before | 512 | 68.37% | 379.8 | 20.87 | 813 | 511.999 | 892.0 |
| 512/512 after | 64 | 18.34% | 423.7 | 22.97 | 2551 | 64.000 | 422.8 |
| 512/512 after | 512 | 67.62% | 390.6 | 20.92 | 879 | 511.999 | 890.7 |

All eight runs exited normally, stayed within their animation budget, reported
zero load failures and zero frames above 1000/30 ms. Coverage counts actor-updates,
not elapsed time. Process RSS high water marks are sampled, not exact allocation
peaks. No claim of lower process memory follows from these single runs.

The broad traces show roughly unchanged coverage: visible owners alone can fill
the budget. The targeted regression demonstrates the newly recoverable case where
old prefetch owners would otherwise block visible admission indefinitely. The
next quality-policy PR must address visible-only overload and demand latency;
this change deliberately does not rotate live visible clips or hide entities.

## Reproduce

Build with `cmake --build --preset dev`, then run from the repository root:

```powershell
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --output build/residency-check
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --output build/residency-check1024
ctest --test-dir build/dev -R "PrefetchDoesNotPin|VisibleWorkingSetOverBudget|DeferredRetirement|ScriptedServerChurn" --output-on-failure
```

Use the installed asset root on your machine. `psutil` is optional for the runner;
install it to collect process memory. JSON reports, exact generated INIs, logs and
summaries are retained under each output directory. The ordinary interactive
ServerDemo keeps WASD movement, N replacement and R restart.
