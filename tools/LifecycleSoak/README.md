# Same-process lifecycle soak

This tests resource lifetime, not FPS. One `test_scene_lifecycle` process reuses
one window/device, frame resources and installed readers for all requested cycles.
Each cycle cold-loads alternating `map/0/100.dat` and `map/0/2100.dat`, moves the
player, replaces a 16-remote-character population at least four times, submits
frames, exits, retires cached assets, waits for GPU completion and collects them.
It then cancels both blocked CPU metadata work and an already-submitted upload.

Native assertions require no remaining sprites/remote entities, map/loading
singletons, pending work, cached/retired entries or accounted payload after each
cycle. Captured weak map/clip references must expire after collection. Reader and
frame-pass identities stay unchanged. The shared desktop fixture now checks the
Vulkan validation error counter through explicit device destruction while its
diagnostic owner is still alive, including repeated identical errors.

## Run

Build with the existing `dev` / `asan` presets and configure each build's
`bin/settings.ini` asset root as described in [build and test](../../docs/build-and-test.md).
The corpus must contain both maps and animation 100000. Set `Display.Validation=true`;
the native test rejects disabled validation. Missing assets or layers fail.

```powershell
ctest --preset dev-lifecycle-soak -V
ctest --preset asan-lifecycle-soak -V
```

Each preset runs 100 cycles in one native process. The ordinary complete CTest
suite runs four cycles, plus the Python runner's unit tests. No long test is
silently skipped. The desktop resource lock serializes CTest GPU runs.

For a separate report directory and required RSS measurements:

```powershell
python -m pip install psutil
python tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 100 --warmup-cycles 20 --require-memory --output build/lifecycle-dev-100
python tools/LifecycleSoak/run_soak.py --test-exe build/asan/bin/test_scene_lifecycle.exe --cycles 100 --warmup-cycles 20 --require-memory --output build/lifecycle-asan-100
```

Use the x64 Visual Studio developer environment for ASan runtime discovery.
Run these serially with no concurrent GPU test. CTest's resource lock does not
coordinate a separately launched CLI. The runner never rewrites settings.ini.

## Reports and failure handling

At each quiescent checkpoint the native test pauses for an acknowledgement. The
runner records optional process RSS before permitting the next reload; it does
not race a sample against the next map's allocations. `summary.json` contains
cycle counts, every checkpoint RSS, sampled peak RSS, first/last post-warmup
window medians and their signed difference. Windows contain up to ten samples,
are disjoint, and need at least four post-warmup checkpoints.

RSS requires optional `psutil`. Without it the report explicitly contains null
samples; native ownership/accounting checks still run. `--require-memory` fails
if memory cannot be measured. `--max-growth-mib N` optionally fails when the last
window median exceeds the first by more than N MiB; it requires memory and enough
post-warmup cycles. Choose a gate from repeated measurements on the intended
hardware/build, not an arbitrary universal cap.

Exit is nonzero on native failure, missing/skipped cycles, malformed checkpoints,
Vulkan VUID/error or ASan/LeakSanitizer diagnostics, timeout, log-reader failure,
or a configured RSS gate failure. A zero-exit native process that ran no tests
is rejected. Timeout/failure terminates and reaps the one launched child. Logs
and failure summaries remain in the output directory. Every run uses a unique
native log; `summary.json` is the latest invocation in that directory.

The test's renderer draws/idle waits and checkpoint pauses are intentionally not
a game performance workload. Do not report its throughput as FPS or compare
ASan RSS with ordinary runtime memory. Driver caches, allocator retention, file
caches and sanitizer quarantine can retain RSS after all tracked owners are
released. A flat RSS trend and clean ASan/validation are not proof of leak freedom;
RSS is not dedicated GPU residency. All-cycle owner/accounting assertions and
validation through final destruction provide additional evidence.

Python protocol/error tests need no GPU, assets, psutil or engine module:

```powershell
python tools/LifecycleSoak/test_runner.py
```


Measured cycle counts, RSS trends and validation evidence are recorded in
[lifecycle validation](../../docs/lifecycle-soak-validation.md). Hosted CPU-only
CI is a separate follow-up; this PR provides the local CTest automation boundary.
