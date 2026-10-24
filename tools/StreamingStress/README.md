# Native streaming movement/churn stress

This tool runs **cgl-client**, not a Python renderer or server. It uses the engine's
normal map, movement, snapshot, streaming and Vulkan paths. Python only creates
INI files, runs the executable and validates its JSON report.

## Run

Build the client with the normal `dev` preset first. From the repository root:

```powershell
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 16 64 512 --frames 6000
```

For larger interest snapshots (asset catalog must contain enough valid characters):

```powershell
python tools/StreamingStress/run_stress.py --client build/dev/bin/cgl-client.exe --root C:/CrossGate-Legacy --budgets 64 512 --visible 512 --prefetch 512 --catalog 2048 --frames 12000 --replace-frames 1200 --output build/stress1024
```

`--visible` is the on-screen target; `--prefetch` is the outer band of a 1.5-view
rectangle, **not another full viewport**. Counts sum to at most 1024 and occupancy
is capped at ten remote snapshot actors per grid (the local player is additional). Map edges/small maps can provide fewer cells.
For 1024 on-screen targets without outer prefetch, use `--visible 1024 --prefetch 0`.
Appearances are distinct within a snapshot when enough catalog entries exist;
entries can be reused across successive snapshots. Counts are targets, not proof
that every requested appearance is ready. Catalog enumeration fails explicitly if
the installed corpus cannot provide the configured distinct targets.

The default is 128 visible + 160 prefetch actors, catalog limit 1024, seed 3169,
6000 world updates, 240 updates per square-route leg and complete replacement every
600 updates. World movement/animation advances by a fixed 16.666667 ms simulated step; frame and
CPU/GPU timing stay real. Simulated time can run much faster than wall time: this
is an aggressive frame-driven demand workload, not a real-time network model.
Existing cell interpolation, facing and camera-follow behavior remain in use.
The script overrides WASD/N during the run. Do not resize the window, restart it
with R, change the corpus, or otherwise interact when comparing workloads.

The client closes automatically after exactly `StressFrames` ready-world updates.
No `--frames` CLI cap is needed; a smaller external cap/manual close is detected as
incomplete. Stress mode requires ServerDemo and a telemetry report. It is disabled
by default (`StressFrames=0`), preserving interactive WASD/N operation.

## Outputs and interpretation

Each run gets unique INI/JSON/log files under `build/streaming-stress` (or `--output`).
`summary.json` contains successful cases completed so far and is replaced for a new
invocation; individual run files remain. A failed/timeout child is reaped rather
than left running. `--timeout` defaults to 180 wall seconds per case. Standard
Python suffices; if `psutil` is already available, the tool also samples process
working set every 20 ms, retaining only 128 tail observations and a peak scalar. Tail samples can include normal shutdown/reclamation.
Without it, process memory values are null. These are sampled CPU process values,
not dedicated GPU residency and not a leak detector.

The tool checks scene/script completion, update counts, movement/replacement,
reported animation-budget accounting and valid coverage/timing counters. Across
budgets it compares a non-cryptographic snapshot fingerprint of server IDs,
appearance indexes and MAP positions. This validates workload repeatability for
the **same binary/platform/corpus**, not asset contents, driver scheduling or
identical asynchronous completion order. Different implementations of shuffle or
manual input can change the fingerprint. No frame-time/quality pass threshold is
invented by the tool: raw measurements remain available for review.

Key JSON fields:

- `lifetime_fps`, `lifetime_frame_max_ms`, `lifetime_frames_over_33ms`: all accepted
  gameplay frame samples (over-33ms follows the existing 1000/30 ms threshold); startup/loading frames follow the existing exclusion.
  `frame_p95_ms`/`frame_p99_ms` remain the final rolling 512-frame window.
- `stream_frames`, `stress_configured_frames`, `stress_completed`: script progress.
  `status=complete` means a clean client run, **not full appearance coverage**.
- `stream_snapshots`, `stream_replacements`, `stream_snapshot_fingerprint`: feed
  workload. The fingerprint is a decimal string to preserve all 64 bits in JS.
- `stream_visible_samples`, `stream_ready_visible_samples`: sums over world updates;
  their ratio measures actor-update coverage, not wall-time-weighted coverage.
  `stream_missing_visible_frames` counts updates with any unresolved visible actor.
- `stream_visible`/`stream_ready_visible`, `stream_prefetch`/`stream_ready_prefetch`:
  final counts, including a potentially fresh replacement on the last update.
- `stream_budget_wait_frames`: updates with the library's active admission budget
  wait, not all unresolved demand. A blocked request may now be parked so another
  appearance can use the slot.
- `stream_budget_deferred_appearances`, `stream_peak_budget_deferred_appearances`,
  `stream_budget_deferred_frames`: current/peak unique appearance demands whose
  known required bytes exceed the last sampled eventual capacity, and updates
  with at least one such demand. These include visible and prefetch requests;
  they are policy estimates, not reserved bytes or successful loads. The runner
  preserves these counters when present and accepts older-client reports without
  them for comparisons.
- `stream_peak_accounted_bytes`, `stream_peak_retired_bytes`,
  `stream_peak_pending_bytes`: observed per-update high water marks; they can miss
  brief between-update changes. Animation accounting is ready + retired + reserved
  RGBA. It excludes map textures, staging, driver overhead and CPU reader caches.
- `stream_player_x/y`, `stream_travel_cells`: final MAP position and cumulative
  frame-end displacement. Turns within one update can make this slightly less
  than the integrated path length.
- The background-load timing/cancellation/failure fields from PR #67 remain in the
  report. Oversized-for-budget clips can count as permanent load failures; not
  every failure implies a corrupt source file. Pending work at the final scripted
  update is expected; normal shutdown waits/cancels/releases it after reporting.

## Appearance demand latency

With Telemetry enabled, ServerDemo tracks each continuous observed cache miss by
catalog appearance index, shared across actors. Clock origin is the first scene
update observing missing ready data in the current 1.5-view interest region;
completion is the update observing usable cached data (and attaching it for a
visible actor). It is not server/network time, GPU timestamp or presentation time.
Already-ready cache hits produce no latency sample; consult coverage alongside it.

`demand_started/completed/withdrawn/failed/pending` count scene-lifetime miss
outcomes and current misses. Started equals completed + withdrawn + failed +
pending. Leaving interest/removing the last user withdraws a miss; known failed
appearances do not create repeated failures every update. Reentry after withdrawal
starts a fresh miss. Replacing actor IDs while the appearance remains demanded
does not restart the shared miss. Region membership uses the existing foot point.

`demand_samples` is the most recent 256 successful misses; completed is lifetime.
`demand_mean_ms/p95_ms/p99_ms/max_ms` describe that window. Queued, deferred and
loading mean fields partition the same samples and sum to mean_ms. Queued includes
selection/slot-drain waiting; deferred means a known capacity exclusion or the
library's exact budget wait; loading means that request owns metadata/decode/upload
work or a just-completed library handoff, including owner-thread polling. CPU worker and upload-wait timings remain
separate projections in the existing animation_load fields; do not add them again.
Phases attribute the interval following each observation, with 1 ms wall-clock
resolution. They are sampled states, not exact GPU/CPU service timestamps.

`demand_visible_samples/visible_completed/visible_mean_ms/visible_p95_ms/
visible_p99_ms/visible_max_ms` measure only the current continuous visible period
when a tracked miss completes visibly. Prefetch before entry is excluded. Leaving
the visible region resets that period; ready-at-promotion may give zero visible
wait. Their sample window differs from the total-miss window.

`demand_visible_pending`, `demand_oldest_pending_ms/oldest_visible_ms` describe the
current sample. `demand_peak_pending_ms/peak_visible_ms` retain the largest observed
ages over the scene, including boundary observations of completion/withdrawal or
failure. A final fresh snapshot therefore does not erase earlier long waits.
Successful percentiles alone cannot characterize unresolved/censored demand.

Tracking holds at most 1024 current miss records and two fixed 256-sample rings,
with no entity/GPU owners or historical snapshot queue. Samples are sorted only
when a JSON report is emitted. It is disabled when RuntimeTelemetry is absent;
scene exit/restart destroys the state. The Python runner validates these optional
fields, including outcome conservation, finite/order/phase/ring/age bounds, and
still accepts older clients without them. Script movement uses simulated time,
while all latency and FPS measurements use actual wall time.

## Current over-budget behavior

The fallback keeps entity identities and a shared placeholder until an appearance
is ready. Actors whose foot point or ready image bounds intersect the viewport pin their
clip through Sprite. Outer-band actors keep CPU identity/position data and warm
cache-only clips; fully off-screen actors release their clip pins as well.
Entering the viewport reuses a ready clip on the next update, otherwise shows the
placeholder while loading. `ready_prefetch` reports cache readiness, not an outer
Sprite ownership guarantee. Image bounds use the same offset, size, scale and intersection test as the renderer;
snapshot interest and coverage counters retain their existing foot-point definition.

Eviction still runs only under actual admission pressure with bounded maintenance
and submission/completion serials. Releasing a Sprite reference does not bypass
GPU completion or force immediate destruction. A key shared by visible and outer
actors remains protected by its visible owners. No blocking GPU waits were added.
When background metadata reports a temporary budget miss, the scheduler keeps the
exact required bytes only for that current unresolved appearance. A known request
that cannot fit is parked while other fitting or as-yet-unknown requests progress;
its original age remains intact. Unknown costs still require the bounded metadata
worker and normal exact admission check. Region and aging priority among eligible
requests are unchanged. Prefetch may use spare capacity if no eligible visible
request exists; it never displaces a pinned visible clip.

During pressure, an owner-thread scan at most every 250 ms estimates capacity after
idle/retired clips can eventually be reclaimed, subtracting held clips and pending
reservations. This hint never grants an allocation or frees in-flight memory;
normal maintenance and exact admission still enforce the actual budget/fences.
A stale hint can delay reconsideration or optimistically select a request that
must still wait. Leaving the demand set drops remembered costs and clears obsolete
budget waits. Already displayed visible owners remain stable to avoid upload churn.

Temporary pressure waits/retries; intrinsically oversized clips fail and keep
their placeholder. A visible working set larger than the budget can still leave
actors unresolved indefinitely: choosing far-visible victims or lower-detail
representations remains separate work. Cache-only prefetch may be evicted before
it is used; this deliberately prioritizes retaining visible appearances.

Native standalone example: `bin/configs/streaming-stress100.ini` (64 MiB). Adjust the
asset root, then run `cgl-client.exe --settings <path-to-ini>`; its report path is
relative to the process working directory. Ordinary interactive demo configs can
also set AnimationBudgetMiB, VisibleActors, PrefetchActors, CatalogLimit and Seed
without enabling the scripted route.

## Tests

```powershell
ctest --test-dir build/dev -R "StreamingStress|Configurable1024|ScriptedServerChurn|VisibleWorkingSetOverBudget|PrefetchDoesNotPin|BudgetBlockedVisibleClip|AppearanceScheduler|IdleByteHint|streaming_stress_runner|LongRunOverwrites" --output-on-failure
```

Headless tests validate schedule boundaries, limits, 1024-appearance seeded churn,
settings loading, bounded lifetime telemetry and report rejection. Vulkan tests
restart the same scripted workload, compare its fingerprint/displacement, verify
entity/clip cleanup, and demonstrate bounded placeholders under a pinned visible
working set larger than the available budget. Run the normal full/ASan suites for
broader lifetime and renderer validation.
