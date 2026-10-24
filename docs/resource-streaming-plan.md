# Resource and rendering follow-up plan

Current base: merged PR #65 (frame-driven scene animation admission).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Visible/prefetch scheduling, same-region aging and nonblocking stale CPU-work discard | Bounded current demand; no early reservation credit or stale upload; stable active work; full normal/ASan and native regression |
| Next | Streaming measurements and admission refinement: exact metadata reservations, per-request latency, sustained churn, configurable demo budgets and low-budget cases | Warm/cold FPS p95/p99/max, cancellation/latency distributions, pending/retired plateaus, repeated teardown, separately measured process memory and GPU residency |

Resource work is estimated at **2-3 PRs including the current change**.
The scheduler ownership change is separate from metadata reservation and sustained
churn measurement. Existing correctness tests are not substitutes for sustained
churn measurements; measurements and reservation refinement can split if the next
PR grows too large.
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
