# Resource and rendering follow-up plan

Current base: merged PR #68 (repeatable churn workloads and coverage diagnostics).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Remove off-screen Sprite pins; keep outer prefetch cache-owned and reclaimable under visible admission pressure | Shared-key protection, promotion/demotion, eventual visible admission under a tight budget, serial-safe retirement, full normal/ASan and identical native churn traces |
| Next | Decide far-visible over-budget quality policy and add demand-to-ready observability | Visible-only overload; stable victim selection/hysteresis; quality/latency/FPS tradeoffs; no stale ownership or early GPU release |

Resource work is estimated at **2 PRs including the current change**, with another
possible split if visible residency needs a larger design. This PR does not evict
live visible owners or guarantee all visible appearances fit the budget.

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
