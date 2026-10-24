# Resource and rendering follow-up plan

Current base: merged PR #67 (exact background reservations and load timing).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Repeatable movement/snapshot churn runner, configurable demo budgets/counts and coverage/high-water diagnostics | Seeded workload equality across budgets; fixed-step route/restart; default interactive regression; bounded placeholders under pinned pressure; full normal/ASan and native measurements |
| Next | Choose and implement over-budget quality policy: prefetch pin release, far-visible residency and demand-to-ready observability | Same churn traces; quality/latency/FPS tradeoffs; no stale ownership or early GPU release; separately measured process memory and GPU residency where available |

Resource work is estimated at **2-3 PRs including the current change**.
The measurement harness is separate from residency/quality policy. Existing
placeholders are now measured explicitly; a clean run does not imply all visible
appearances fit. The next scope may split if residency needs a larger design.
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
