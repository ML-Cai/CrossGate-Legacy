# Resource and rendering follow-up plan

Current base: merged PR #66 (visible-demand scheduling and nonblocking CPU cancellation).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Exact worker-side metadata reservation and bounded background-load latency diagnostics | Zero-byte occupied-slot safety; budget snapshot/shrink rules; cancellation/error cleanup; real Vulkan payload accounting; full normal/ASan and native regression |
| Next | Sustained movement/churn harness, configurable demo budgets and over-budget display policy | Warm/cold FPS p95/p99/max, request/demand latency distinction, cancellation distributions, pending/retired plateaus, repeated teardown, separately measured process memory and GPU residency |

Resource work is estimated at **2-3 PRs including the current change**.
Admission/timing is separated from sustained churn measurement and the visible
working-set fallback policy. Existing correctness tests do not replace sustained
measurements; the next scope may split if the fallback needs a larger design.
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
