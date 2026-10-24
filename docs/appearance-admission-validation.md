# Appearance admission validation

Base: develop `d8497f7` (PR #69), measured 2026-10-24 on AMD Radeon 880M
integrated graphics. The retained before executable was built from source identical
to the squash-merged base. This does not validate a 4/8 GiB dedicated-GPU target or
measure physical VRAM. The animation budget is an RGBA payload/reservation ledger.

## Policy and ownership

Already displayed visible clips remain held. A known request larger than sampled
eventual capacity is parked while other fitting/unknown requests can use the
single metadata/decode/upload slot. This is not optimal subset selection or a
promise that every actor will become ready. Unknown costs still need metadata;
known costs and original demand age live only in the current unresolved snapshot,
bounded by 1024 demands. Ready/removed demands lose these records.

Capacity hints scan cache-only ownership no more often than every 250 ms during
pressure and subtract held payload/pending reservations. They include idle/retired
payload that may eventually be reclaimed. They never credit actual allocation
capacity early: existing exact admission, bounded retirement and completed graphics
serials remain authoritative. A stale hint may delay retry or select work that
still waits. Submitted uploads are not cancelled by this policy. Removing demand
also clears its obsolete budget wait, including when no request is active.

New report fields distinguish parked demand from a library admission wait. Current,
peak and update counts cover unique visible/prefetch appearance requirements that
exceed the last sampled capacity; they are estimates, not entity counts or bytes.
The Python runner accepts old reports without those fields and validates new ones.
Demand-to-ready latency including parked time remains follow-up work.

## Automated checks

- Normal CTest: 248/248 passed; MSVC AddressSanitizer CTest: 247/247 passed.
  The difference is the normal-only Python session test. No ASan errors reported.
- CPU-only tests cover exact fit, smaller-request progress, retained age/priority,
  shared appearance deduplication, snapshot pruning and non-mutating idle-byte hints.
- Native Vulkan regression loads a resident clip, then puts a too-large visible
  request before a smaller one. The small clip becomes ready without evicting the
  resident. Releasing that owner lets the parked large request complete, with
  actual ready/retired/pending accounting checked throughout and cleanup returning
  to baseline. Existing request-removal regression checks parked-owner cleanup.
- Existing lifecycle suites cover actual frame fences, pending CPU/upload work,
  repeated replacement, restarts and visible-only overload with placeholders.

A green test count is not a validation-clean Vulkan suite: the unchanged
`BasicDrawTest.BasicDrawFlow` submits an empty command buffer without transitioning
the acquired image before present. Normal and ASan logs report
`VUID-VkPresentInfoKHR-pImageIndices-01430` for that test. Inspection found no VUIDs
in resource/scene tests. Repairing that old graphics fixture, window cleanup and
validation-error assertions is explicitly tracked separately in the follow-up plan.
ASan and accounting checks do not establish the absence of every leak.

## Native workload comparison

Each run uses map100, VSync disabled/immediate presentation, validation disabled,
6000 fixed-step world updates, seed 3169, 16.666667 ms simulated step, 240-update
movement legs. 128/160 uses catalog1024/replace600; 512/512 uses catalog2048/replace1200.
No concurrent builds/GPU tests. One run per configuration; background completion
and FPS are real wall-time measurements while movement/snapshots use accelerated
simulated time. Do not extrapolate these values to production/network latency or
compare their absolute FPS to older sessions under different host conditions.
Fingerprints match before/after: `15372329328879399793` and `835595881056024373`.

| Visible/prefetch version | Budget MiB | Visible coverage | FPS | Max frame ms | Library wait updates | Parked-demand updates | Peak ledger MiB | Peak process RSS MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128/160 before | 64 | 45.09% | 298.1 | 23.95 | 2602 | n/a | 63.999 | 441.3 |
| 128/160 before | 512 | 88.53% | 325.2 | 22.24 | 0 | n/a | 332.150 | 626.9 |
| 128/160 after | 64 | 43.75% | 306.0 | 22.47 | 2604 | 19 | 64.000 | 426.1 |
| 128/160 after | 512 | 88.52% | 319.4 | 21.42 | 0 | 0 | 332.150 | 636.9 |
| 512/512 before | 64 | 18.28% | 189.5 | 31.13 | 2740 | n/a | 63.999 | 376.7 |
| 512/512 before | 512 | 68.88% | 156.1 | 27.87 | 957 | n/a | 511.999 | 864.2 |
| 512/512 after | 64 | 19.69% | 185.6 | 25.79 | 2527 | 545 | 64.000 | 398.5 |
| 512/512 after | 512 | 69.15% | 158.3 | 26.31 | 914 | 0 | 512.000 | 888.4 |

All eight exited normally, stayed within budget, reported zero appearance-load
failures and zero frames above 1000/30 ms. Coverage counts actor-updates, not wall
time. RSS is sampled process memory including non-GPU allocations/driver overhead,
not a leak detector. Differences from these single runs do not establish a general
FPS, memory or coverage improvement. The high-count/64 MiB trace shows a modest
coverage change; the targeted regression demonstrates the guaranteed behavioral
fix. Visible-only over-budget actors may still wait indefinitely because held
visible clips are not rotated. Cache scanning is throttled, not a CPU-time bound.

## Reproduce

Build with `cmake --build --preset dev`, then from the repository root:

```powershell
ctest --test-dir build/dev -R "AppearanceScheduler|IdleByteHint|BudgetBlockedVisibleClip|RemoteBudgetWaitCanChange" --output-on-failure
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --output build/admission-check
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --frames 6000 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --output build/admission-check1024
```

Adjust the installed asset root. `psutil` is optional for sampled process memory.
Each output directory retains generated INIs, logs, JSON reports and summaries.
The normal interactive demo retains WASD/N/R controls with stress mode disabled.
