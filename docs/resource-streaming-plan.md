# Resource and rendering follow-up plan

Current base: merged PR #64 (keyed retired-resource reuse).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Frame-driven scene animation admission while old GPU resources retire | Whole-set metadata validation; no partial characters or movement during loading; partial-load restart/cancellation; full normal/ASan and native regression |
| Next | Streaming scheduler: superseded/off-screen CPU work, visible versus 1.5-view prefetch priority, metadata size planning, fairness and per-request latency | Movement with repeated appearance batches, pending decode/upload cancellation boundaries, starvation/placeholder checks |
| Follow-up, possibly combined with scheduler | Sustained churn benchmark and memory evidence; configurable demo budgets; 512/1024 distinct appearances and low-budget cases; decide fallback when the visible working set exceeds budget | Warm/cold FPS p95/p99/max, admission latency, pending/retired plateaus, repeated teardown, separately measured process memory and GPU residency |

Resource work is estimated at **2-3 PRs including the current change**.
Scene-entry admission is separate from keyed reuse. Existing correctness tests
are not substitutes for sustained churn measurements; the standalone benchmark PR
can be combined only if the scheduler PR stays reviewable.
The present LRU policy scans/sorts the cache when pressure scans are due. Profile
that cost before replacing it with a persistent/incremental LRU structure. Count
limits are not CPU-time guarantees, and map loading is still synchronous.

After the resource baseline, reserve approximately **2 more PRs** for visible-area
culling/sort work and draw submission/batching, selected from actual profiles.
Total near-term estimate: **4-5 PRs including the current change**, with tests and
runtime measurements in each; allow scope changes if new correctness issues appear.
No calendar deadline is implied, and review/asset/driver investigations can change
this estimate.

Later architecture work stays separate: ECS reference safety, CPU-only public APIs
and Python bindings, and UI input consumption/focus. Android/Linux platform
lifecycle and surface integration require their own concrete requirements and
validation; Vulkan support alone does not complete either port.
