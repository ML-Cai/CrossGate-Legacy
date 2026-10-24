# Resource and rendering follow-up plan

Current base: merged PR #71 (validated basic presentation flow and fixture cleanup).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Observe complete appearance-miss latency, queued/deferred/loading phases, visible waiting, pending ages and censored outcomes | Deterministic windowless clocks; bounded rings/current demand; sharing, promotion, withdrawals/failures/reentry/reset; native blocked-request recovery; full normal/ASan and identical churn traces |
| Next | Profile eligible queue waiting (busy service slot versus another request blocked on global capacity); evaluate bounded preparation/upload overlap before choosing visible-residency changes | Same routes and miss outcomes; exact ready/retired/pending accounting; cancellation/scene exit; queued and visible tail latency together with FPS; no unbounded parallel jobs |

Resource work is estimated at **1-2 PRs including the current change**. Existing
visible owners remain held; no quality/eviction rule changes in the current PR.
Do not add repeated uploads just to show more distinct clips without measuring
visible latency, unresolved demand and frame costs together. Metrics report only
observed misses, with lifetime pending-age maxima and censored counts alongside
recent successful samples; fast cache hits are already reflected in coverage.

The observed queued phase includes waiting behind other requests and global capacity
pressure, not only CPU work. Successful sample means cannot describe withdrawn
or still-pending demand. Use both facts when selecting the next optimization.

The present LRU policy scans/sorts the cache when pressure scans are due. Profile
that cost before replacing it with a persistent/incremental LRU structure. Count
limits are not CPU-time guarantees, and map loading is still synchronous.

The old BasicDrawFlow UNDEFINED-layout presentation error was addressed in #71. Its teardown now checks the backend validation-error counter,
including device-child cleanup diagnostics. Other fixtures retain their existing
checks; inspecting the complete validation logs remains necessary. See
[basic draw validation](basic-draw-validation.md) for the negative control and
reproduction commands.

After the resource baseline, reserve approximately **2 more PRs** for visible-area
culling/sort work and draw submission/batching, selected from actual profiles.
Total near-term estimate: **3-4 PRs including the current change**, with tests and
runtime measurements in each; allow scope changes if new correctness issues appear.
No calendar deadline is implied, and review/asset/driver investigations can change
this estimate.

Later architecture work stays separate: ECS reference safety, CPU-only public APIs
and Python bindings, and UI input consumption/focus. Android/Linux platform
lifecycle and surface integration require their own concrete requirements and
validation; Vulkan support alone does not complete either port.
