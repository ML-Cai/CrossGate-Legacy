# Progressive local-character scene entry

## Scope

Normal MapScene entry previously acquired all eight idle and eight walking clips
synchronously. PR #86 separated catalog scanning; its remaining largest owner
call was local clip acquisition (about 29 ms on the development corpus).

MapScene now uses `AnimationLibrary::acquireForSceneProgressive`: the existing
single CPU decode worker and chunked GPU uploader, with no scene lookahead or
second queue. Cached clips still publish immediately. A cold clip is planned and
decoded off the owner thread, then uploaded by owner-thread polls using at most
4 MiB per chunk. A poll that starts an upload does not also poll it; finishing a
clip can start the next CPU job but cannot submit its GPU upload in that update.
Partial clips remain unpublishable. The local character is created only after all
16 clips are ready, and normal world input/update/drawing still wait for entry.

The original metadata working-set admission runs before map/pixel allocation.
Completed scene clips stay pinned while old submitted draws may retain budget;
no retired ownership is credited before actual fence completion. Restart/exit
cancels and joins the CPU slot before dropping its reservation or replacing
readers. A submitted upload's renderer waits for its own chunk on cancellation.
Existing cache retirement and device teardown order are unchanged.

## Limits and memory

The remote API retains its default 32 MiB clip/combined preparation limit. The
scene API explicitly allows the existing local 256 MiB per-clip limit, bounded
further by the same total animation budget. `submitPlanned` validates this explicit
limit before accepting a job; zero or above-256 MiB limits are rejected. The default
worker task API stays at 32 MiB. Metadata occupies the slot at zero RGBA bytes;
CPU decoding reserves exact payload before allocation, including large local clips.
No scene CPU lookahead is requested, and cached/retired/pending bytes all continue
to count against the total budget. Shrinking during occupied CPU work remains
forbidden. An over-total-budget clip is an error; a fitting clip blocked by old
owners is deferred without pixel reads.

The payload ledger is not physical RAM/VRAM accounting. One CPU canvas can be up
to 256 MiB, plus bounded indexed-frame/cache/scratch data; during upload CPU source
and GPU destination may coexist, with at most 4 MiB staging. This was also possible
in the former synchronous path. This PR does not claim a 32 MiB CPU ceiling for
local scenes or validate dedicated 4/8 GiB GPUs.

The separate AnimationScene preview and offline/preloaded benchmark intentionally
retain their existing synchronous APIs. Metadata working-set planning, GPU object
allocation, tiny outline creation and individual upload polls are not hard real-
time operations. The change removes the sixteen synchronous decode/full-upload
sequence; it does not promise a deadline for every entry call or shorter total
load time. More loading updates may be necessary.

Successful local loads now contribute to the existing `animation_load_*` timing
counters, alongside successful remote background loads. `entry_character_clips_*`
measures owner-thread acquisition polls (including cheap retries), not worker CPU
execution. Demand/coverage telemetry begins with remote world creation as before.

## Tests and reproduction

- CPU worker tests exercise explicit large reservations, unchanged remote defaults,
  exact budget rejection before pixel decode, and cancellation retaining a large
  reservation/captured owner until CPU access finishes. They run in the shared
  SDK-free Windows/Linux ordinary/ASan lane.
- Actual Vulkan tests cancel during the first CPU job and a submitted upload, verify
  no partial character publication, reload all sixteen clips over separate polls,
  draw, and verify the released clip expires.
- A real-reader proxy expands only transparent canvas offsets, producing a
  2074x2053x4-frame clip (>32 MiB). A pinned owner first blocks its admission with
  no pixel reads; releasing it permits chunked GPU completion within the exact
  budget. The remote API rejects that same clip before pixels. Reader restoration
  joins CPU work even on assertion exits.
- Existing no-draw fence/budget tests still require 15 admitted clips, deferred last
  clip, zero premature retirements, stable pinned ownership and successful restart.
  They now await actual budget deferral, without advancing draw-fence completion.

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error
build/python-session-env/Scripts/python.exe tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --replace-frames 1200 --frames 6000 --output build/scene-animation-entry-stress
build/python-session-env/Scripts/python.exe tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 300 --warmup-cycles 100 --require-memory --max-growth-mib 64 --output build/scene-animation-entry-soak
```

Use the x64 Visual Studio developer environment for MSVC/ASan; desktop tests need
installed assets and a working Vulkan driver. The RSS growth gate is diagnostic,
not an engine memory budget or proof of universal leak freedom. Windows ASan does
not provide LeakSanitizer coverage.

## Results

Local validation:

- Full desktop ordinary **323/323**, MSVC ASan **322/322**; zero skipped cases.
  The ordinary-only Python binding test accounts for the difference.
- SDK-free ordinary and ASan **71/71** each. Final changed worker-test lifetime
  ordering was rebuilt and checked in desktop/headless ordinary/ASan configurations.
- The large real-reader clip is **68,126,752 B** (2074x2053, four frames), requiring
  at least 17 uploader polls; exact-budget completion and unchanged remote
  rejection pass with Vulkan validation, also under MSVC ASan.
- **300/300** same-process lifecycle cycles, warmup100, diagnostic 64 MiB growth
  gate: all scene/remote/CPU/upload/cache ownership checkpoints clear. First/last
  checkpoint-window RSS medians **270,434,304 -> 266,516,480 B (-3.74 MiB)**,
  peak sampled RSS **313,241,600 B**. This observes stable reclamation in this run;
  RSS alone does not prove leak freedom. The soak ran alongside native ASan tests;
  performance measurements below ran after both finished, without other test/build jobs.

### Interleaved entry control

On the Radeon 880M integrated development GPU, map100, 1280x720, VSync disabled
with verified IMMEDIATE present mode, 512 visible + 512 outer, catalog2048,
600 gameplay updates and replacement every120 updates. Execution order was
before-A / after-A / after-B / before-B. All four fingerprints are identical
(`11928563578253860611`). Values below are medians of the two runs per version.

| CPU wall-time metric | Merged #86 | Progressive scene clips |
| --- | ---: | ---: |
| Largest local-clip owner call | 60.8266 ms | 3.0592 ms |
| Total local-clip owner calls | 60.8401 ms | 65.2898 ms |
| Loading-frame lifetime maximum | 66.5687 ms | 9.2094 ms |
| Loading rolling p99 | 5.3351 ms | 5.1328 ms |
| Sum of sampled loading-frame times | 540.5485 ms | 612.9270 ms |

Individual local-call maxima were 57.3697 / 64.2834 ms before and
2.9414 / 3.1770 ms after. This small paired control demonstrates distribution of
owner work; aggregate owner cost and sampled loading duration increase. It is not
an improvement claim for total load time or gameplay FPS. The separate original
29 ms observation came from a different host session; use the current paired
control for this comparison. Remaining map owner calls reach roughly 8-11 ms in
these controls and are not bounded by the animation chunk-byte limit.

### Native streaming regression

Fresh 6,000-update runs, same map/view/counts, replacement every1200 updates:

| Metric | 64 MiB animation budget | 512 MiB animation budget |
| --- | ---: | ---: |
| Gameplay lifetime FPS | 428.605 | 392.888 |
| Gameplay rolling p99 | 5.3824 ms | 5.6422 ms |
| Gameplay lifetime maximum | 21.0494 ms | 23.8273 ms |
| True-ready visible coverage | 21.6064% | 78.2691% |
| Local-clip largest owner poll | 3.1082 ms | 2.7554 ms |
| Loading-frame lifetime maximum | 8.5508 ms | 8.3592 ms |
| Peak accounted animation payload | 67,106,328 B | 536,870,704 B |

Both complete with fingerprint `835595881056024373`, no appearance/load failures,
5,999 GPU timing samples and zero missing results. Map CPU page peak is
33,488,928 B (within 32 MiB); partial map GPU payload peak is 40,330,752 B;
map pending/working bytes are zero at completion. Budget pressure/readiness
still affects how many real actors are drawn; there is no paired gameplay FPS
gain claim versus previous sessions. Dedicated 4/8 GiB GPU residency remains
unverified.

Complete positive native/stress/soak logs contain no VUID, Vulkan validation-error
or ASan error markers. The hosted Windows/Linux ordinary/ASan matrix uses the
same production worker and eleven worker cases; consult this PR's current checks.
Ignored evidence is under `build/scene-animation-entry-control/`,
`build/scene-animation-entry-stress/`, `build/scene-animation-entry-soak/`, and
`build/scene-animation-entry-{tests,asan-tests,headless-tests,headless-asan-tests}.log`.
No game files, executables, game assets or raw logs are committed.
