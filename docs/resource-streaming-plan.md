# Resource and rendering follow-up plan

Current base: merged PR #62 (fence-acknowledged GPU retirement).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Progressive appearance admission/backpressure, pressure-driven idle LRU retirement, bounded per-update destruction, budget-wait cancellation and diagnostics | CPU policy tests; real Vulkan pinned/retired/retry cases; full normal/ASan tests; map100 and 512-character cold-load regression |
| Next | Reuse keyed retired assets safely before destruction; handle synchronous scene re-entry/entry budget pressure without duplicate uploads or unsafe early budget credit | Re-enter before fence completion; reuse versus actual free accounting; failure/cancellation and device lifetime; scene-change peak memory |
| Then | Streaming scheduler: superseded/off-screen CPU work, visible versus 1.5-view prefetch priority, metadata size planning, fairness and per-request latency | Movement with repeated appearance batches, pending decode/upload cancellation boundaries, starvation/placeholder checks |
| Follow-up, possibly combined with scheduler | Sustained churn benchmark and memory evidence; configurable demo budgets; 512/1024 distinct appearances and low-budget cases; decide fallback when the visible working set exceeds budget | Warm/cold FPS p95/p99/max, admission latency, pending/retired plateaus, repeated teardown, separately measured process memory and GPU residency |

Resource work is estimated at **3-4 PRs including the current change**. Existing
correctness tests are not substitutes for sustained churn measurements; the
standalone benchmark PR can be combined only if the scheduler PR stays reviewable.
The present LRU policy scans/sorts the cache when pressure scans are due. Profile
that cost before replacing it with a persistent/incremental LRU structure. Count
limits are not CPU-time guarantees, and map loading is still synchronous.

After the resource baseline, reserve approximately **2 more PRs** for visible-area
culling/sort work and draw submission/batching, selected from actual profiles.
Total near-term estimate: **5-6 PRs including the current change**, with tests and
runtime measurements in each; allow scope changes if new correctness issues appear.
No calendar deadline is implied, and review/asset/driver investigations can change
this estimate.

Later architecture work stays separate: ECS reference safety, CPU-only public APIs
and Python bindings, and UI input consumption/focus. Android/Linux platform
lifecycle and surface integration require their own concrete requirements and
validation; Vulkan support alone does not complete either port.
