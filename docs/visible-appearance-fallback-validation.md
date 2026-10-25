# Visible appearance fallback validation

Base: develop `428cdbd` (PR #82). Measured 2026-10-25 on the development PC,
AMD Radeon 880M integrated GPU. Dedicated 4/8 GiB GPU behavior is not verified.

## Behavior and ownership

Unresolved remote actors display the existing cached yellow diamond at their
ground anchor, rather than borrowing the local player's identity visually.
Markers render after ground tiles and before all world objects. They are ground
indicators, so a building can cover them. Cursor hover still draws on top.
Overlapping actors can share the same visible diamond; this is not a count badge
or a final UI design. Loading, budget waits and permanent failures use the same
marker; internal state distinguishes queued/loading/deferred/failed/ready.

Readiness refers to the requested appearance, not pointer inequality against a
placeholder. A ready offscreen appearance remains cache-owned unless its sprite
pixels intersect the viewport. The separate `presented` flag prevents drawing
the compatibility Sprite placeholder as an actual remote character. Readiness
state is updated even without telemetry. Changed appearance IDs reset presentation
immediately; failed keys are not retried every frame. Completed uploads become
presented on the next update, preserving the existing update/recording boundary.

The outline is borrowed from the scene's existing MapLibrary entry. This change
adds no textures, GPU handles, per-actor upload, cache or worker. There is still
one scene outline owner, and its cache/retired owner survives submitted work until
the established fence boundary. Renderer recording retains no borrowed pointer
between calls, including exceptions. Compatibility Sprite references still share
the existing player placeholder; they do not allocate a second character clip.

Visible-first selection, aging, stable same-region active work, budget-fit
selection and bounded lookahead remain unchanged. Ready visible owners are not
evicted just to cycle appearances. An oversized visible working set may remain
partially unresolved indefinitely. This is explicit fallback, not a throughput,
full-coverage or total-VRAM guarantee.

## Counters and tests

`drawn_actors_last_frame` now excludes unresolved remote placeholders.
`fallback_markers_last_frame` counts clipped marker draw submissions separately;
it can include a near-edge actor with its foot outside the viewport. Buildings
may later cover those pixels. Neither counter measures unobscured image pixels.
Existing ground-anchor ready coverage, pending demand ages/outcomes and RGBA
accounting retain their definitions. Markers still contribute to draw-call count.

Recording tests cover queued/loading/deferred/failed fallback, ready transition,
object/hover ordering, mismatched camera/outline scales, offscreen culling, null
clips, missing outlines, 512 overlapping actors sharing one renderer, and reuse
after a recording exception. Native tests additionally cover metadata loading,
upload completion, failed-key stability, replacement, matching requested and
placeholder keys, offscreen unpinning and temporary budget pressure. Existing
visible/preload priority and lifetime tests remain enabled.

Reproduce from an x64 Visual Studio developer shell with installed game assets:

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error

python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/fallback-streaming
```

The scripted route disables VSync and reports the selected present mode. Inspect
each raw report alongside its summary for fallback count, ready coverage, pending
demand age and successful/withdrawn/failed outcomes. Fixed frame counts do not
mean identical elapsed loading time; higher FPS with fewer ready sprites is not
proof of improved full-character rendering performance.

Interactive test:

```powershell
.\build\dev\bin\cgl-client.exe --settings .\build\dev\bin\server-demo100.ini
```

Set `General.EngineRootPath` to the installed assets. WASD moves with the player
centered, N replaces appearances and R restarts. Initial/loading markers should
turn into characters as their uploads finish. Use a tighter animation budget to
exercise persistent fallback. For diagnostics enable `Display.Validation`; do not
compare validation/ASan FPS against ordinary runs.

## Measured results

- Full ordinary CTest: **291/291**. Full MSVC ASan CTest: **290/290**.
  The normal-only Python binding test accounts for the difference. The final
  deferred-state assertion is also checked by a targeted rerun in each build.
- A separate 600-frame ASan streaming run with validation enabled completes at
  64 MiB, 128 visible/160 outer actors, a 512-appearance catalog and replacement
  every 120 frames. All 598 completed GPU timings are present, with no failed
  appearance loads. Complete positive logs contain no VUID, Vulkan validation
  error or AddressSanitizer error/summary markers.
- A separate **100/100-cycle** ordinary lifecycle soak passes with validation
  enabled, alternating maps 100/2100, actor replacement, submitted rendering,
  CPU/GPU cancellation, zero Sprite/remote entities and zero map/animation
  cache/retired/pending payload at each quiescent checkpoint. After 20 warmup
  cycles, first/last ten-sample median RSS
  is 417,992,704 -> 385,007,616 bytes (a decrease of 31.46 MiB).
  This observed trend and zero-owner assertions do not prove universal leak
  freedom; MSVC ASan does not provide LeakSanitizer coverage of driver allocations.
- Both 6,000-frame streaming runs complete on map 100, with 512 visible and 512
  outer actors, 2,048-appearance catalog and replacement every 1,200 frames.
  Present mode is IMMEDIATE; both fingerprints are `835595881056024373`.

| Animation budget | Lifetime FPS | Rolling frame p99 ms | Ground-anchor ready coverage | Final recorded fallback markers |
| --- | ---: | ---: | ---: | ---: |
| 64 MiB | 749.533 | 3.0052 | 21.6735% | 460 |
| 512 MiB | 644.579 | 4.1144 | 79.3555% | 88 |

Peak accounted animation payload/reservations are 67,108,392 / 536,870,616 bytes,
within each budget. Both runs have zero failed loads and 5,998 completed GPU
timings with zero missing results. Lifetime maximum CPU frame duration is
20.0387 / 17.5127 ms. Peak visible demand age is 1,923 / 1,336 ms, and final
oldest visible pending age is 1,609 / 128 ms. Pending/withdrawn demand is not
counted as successful loading. There is no paired before/after performance
claim: fallback changes rendering workload and the amount of real loading time
available within a fixed frame count.

The separate fully preloaded control draws **6,050 actors, 1,024 distinct clips,
6,833 draws** at 1280x720, ten actors per cell, map 2100. One run (120 warmup,
600 samples) records **345.418 FPS**, frame p99 **4.2503 ms**, GPU mean
**2.28811 ms**, 719 completed GPU timings and zero missing results. It uses
IMMEDIATE presentation and reports a valid sample. This checks the ordinary
ready-character path under the existing load; one run is not a regression
threshold or a performance comparison against another revision.

Dedicated soak reproduction:

```powershell
python tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 100 --warmup-cycles 20 --require-memory --output build/fallback-soak
python tools/benchmarks/crowd_benchmark.py --exe build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --output-dir build/fallback-crowd --densities 10 --unique 1024 --repeats 1 --gpu-timing
```

Ignored evidence is retained under `build/fallback-{streaming,crowd,soak}/`,
`build/fallback-asan-streaming/`, `build/fallback-tests.log`,
`build/fallback-asan-tests.log` and final targeted logs.
No executable, game assets or raw logs are committed.
