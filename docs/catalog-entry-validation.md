# Incremental scene-entry character catalog

## Scope and ownership

A map100 / 512 visible + 512 outer / catalog2048 diagnostic run on the
pre-change path measured 108.490 ms inside `ReadCharacterCatalog`, 109.102 ms
for the whole server-demo entry, and 29.270 ms for the local character clips.
The loading-frame maximum was 141.360 ms. This identified synchronous catalog
scanning as a worthwhile next step; these timings are observations, not a bound.
Temporary diagnostic logging was removed.

`CharacterCatalogScan` now owns one version's sorted serial list and a result
vector capped at 4096 requests. Each scene update inspects at most 16 candidates,
including rejected candidates. Version enumeration/sorting runs in a separate
poll. A busy reader mutex causes a retry without blocking or changing the cursor.
The scanner retains neither pixels nor GPU resources, starts no worker, and
releases all scan storage on completion or scene exit. Installed reader index
limits still bound the per-version list; no second all-version catalog or queue
is introduced. The readers must stay alive and unchanged during a scan.

The blocking helper uses the same scanner for offline/preloaded benchmark paths.
Version order, ascending serial order, direction selection and all existing
motion/frame/canvas/payload filters are preserved. The simulated server feed,
remote entities and streaming diagnostics are created once after the catalog is
complete. Until then the scene stays unready: world input/update and drawing do
not run. R/scene destruction releases the partial local character and scan state.
No changes were made to submitted-GPU resource retirement or texture budgets.

This is a work-count bound, not a real-time guarantee. Enumeration/sorting and
one metadata query remain indivisible; slow storage or unusually expensive
metadata can still stall a poll. Local character clips and the tiny hover-outline
upload remain synchronous and need separate measurement/optimization.

## Diagnostics

When runtime telemetry is enabled, the JSON report includes
`entry_{plan,map_finalize,character_clips,catalog,remote_world}_{calls,total_ms,max_ms}`.
These are CPU wall times accumulated over scene visits in fixed counters, with
no retained resource references or per-frame allocation. Failed calls are included.
Zero calls means that scope was not observed. `catalog` measures a single scan
poll (including an immediate busy-lock retry), `remote_world` measures final
catalog transfer/feed/entity setup, `character_clips` measures each scene plan
acquisition attempt, and `map_finalize` measures camera/hover-outline setup.
The stages exclude intervening loading frames and do not sum to elapsed load time.
They are separate from gameplay FPS and GPU timestamps.

## Reproduction

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error

build/python-session-env/Scripts/python.exe tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/catalog-entry-stress
build/python-session-env/Scripts/python.exe tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 100 --warmup-cycles 20 --require-memory --output build/catalog-entry-soak
```

Use an x64 Visual Studio developer environment for MSVC and ASan tests. Game
assets and a working Vulkan driver are required for desktop validation. The
catalog unit suite is also part of the SDK-free Windows/Linux ordinary/ASan CI.

## Results

On the development Radeon 880M integrated GPU, VSync disabled with verified
IMMEDIATE present mode, the 600-update 64 MiB diagnostic control used exactly
the same snapshot fingerprint (`11928563578253860611`) before and after:

| CPU wall-time measurement | Blocking catalog | Incremental catalog |
| --- | ---: | ---: |
| Catalog largest call | 108.490 ms | 2.855 ms |
| Catalog total | 108.490 ms | 107.575 ms / 225 calls |
| Local clip largest call | 29.270 ms | 28.9753 ms |
| Loading-frame lifetime maximum | 141.360 ms | 31.3562 ms |
| Loading samples | 232 | 452 |
| Sum of sampled loading-frame times | 226.950 ms | 268.796 ms |

This single control demonstrates work distribution and preserved actor data,
not faster total scene entry or improved gameplay throughput. Cache/storage and
OS scheduling can change individual calls; candidate counts are not deadlines.

Fresh 6,000-update map100 stress results at 1280x720, 512 visible + 512 outer,
catalog2048, replacement every1200 updates:

| Metric | 64 MiB animation budget | 512 MiB animation budget |
| --- | ---: | ---: |
| Gameplay lifetime FPS | 746.123 | 635.260 |
| Gameplay rolling p99 | 2.7094 ms | 3.9970 ms |
| Gameplay lifetime maximum | 19.5817 ms | 18.1870 ms |
| True-ready visible coverage | 22.1070% | 79.3747% |
| Catalog largest poll | 3.0816 ms | 2.8563 ms |
| Loading-frame lifetime maximum | 32.3062 ms | 32.5127 ms |
| Local clip largest call | 27.3694 ms | 29.7144 ms |
| Peak accounted animation payload | 67,108,728 B | 536,870,872 B |

Both runs complete with fingerprint `835595881056024373`, zero appearance
failures, 5,999 GPU timing samples / zero missing results, and zero map pending
CPU/upload payload at completion. Map peak CPU page working payload is
33,488,928 B (within 32 MiB); peak partial GPU map payload is 40,330,752 B.
These ledger values are not total physical device memory. Dedicated 4/8 GiB
hardware remains unverified. There is no paired gameplay FPS improvement claim.

Validation:

- Full desktop ordinary 319/319; full MSVC ASan 318/318 (the ordinary-only
  Python binding case accounts for the difference).
- SDK-free ordinary and MSVC ASan 69/69 each, including seven catalog cases.
- Final focused ordinary and ASan checks cover catalog, timing counters and
  actual Vulkan cancellation/re-entry. Tests retain budget/fence ownership checks.
- 100/100 same-process lifecycle cycles complete, with zero scene/remote owners
  and pending map/animation work at every checkpoint. After 20 warmup cycles,
  first/last checkpoint-window RSS medians are 284,575,744 / 297,873,408 B
  (+12.68 MiB); peak sampled RSS is 345,010,176 B. RSS includes allocator/driver
  retention and does not alone establish a leak or prove leak freedom.
- A fresh 300/300 cycle run, warmup100 and a diagnostic 64 MiB growth gate,
  also completes. RSS window medians are 268,365,824 / 269,910,016 B (+1.47 MiB),
  peak sampled RSS 308,576,256 B. All ownership/activity checkpoints clear;
  the growth gate is a test diagnostic, not an engine memory budget.
- Complete positive native/stress/soak logs contain no Vulkan VUID/validation-error
  or ASan error markers. Windows ASan does not provide LeakSanitizer coverage.
- Hosted Windows/Linux ordinary/ASan matrix: pending final completion.

Ignored evidence lives under `build/entry-profile-{before,after}/`,
`build/catalog-entry-stress/`, `build/catalog-entry-{soak,mature-soak}/`,
`build/entry-final-tests.log`, `build/entry-scan-asan-tests.log`, and
`build/entry-{headless,headless-asan}-tests.log`. No game files, binaries or raw
logs are committed.
