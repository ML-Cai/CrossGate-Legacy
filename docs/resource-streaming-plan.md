# Resource and rendering follow-up plan

Current base: merged PR #69 (off-screen animation owners no longer pin prefetch textures).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Let fitting appearance requests bypass a known budget-blocked request; retain visible owners and expose policy deferrals | CPU scheduler aging/pruning/exact-fit tests; native smaller-request progress and later capacity recovery; obsolete wait cleanup; full normal/ASan and identical churn traces |
| Next | Measure demand-to-ready latency; decide whether distance-based visible residency or cheaper representations are justified | Visible-only overload; stable victim selection/hysteresis if introduced; quality/latency/FPS tradeoffs; no stale ownership or early GPU release |

Resource work is estimated at **2-3 PRs including the current change**. The current
policy retains already-presented visible clips, fills remaining space where
possible, and leaves unresolved actors as placeholders. It does not rotate live
visible clips to give every appearance a turn or guarantee all appearances fit.
Demand-to-ready observation and any far-visible replacement policy remain a
separate reviewable change; replacement should follow measured need rather than
introduce repeated uploads just to improve the number of distinct clips shown.

The present LRU policy scans/sorts the cache when pressure scans are due. Profile
that cost before replacing it with a persistent/incremental LRU structure. Count
limits are not CPU-time guarantees, and map loading is still synchronous.

Validation follow-up: the unchanged graphics `BasicDrawFlow` test submits an empty
command buffer and presents an image still in UNDEFINED layout. Both current
normal/ASan logs report VUID-VkPresentInfoKHR-pImageIndices-01430 although CTest
passes. Reserve **1 separate test cleanup PR** for a real render-pass/layout flow,
fixture window cleanup and validation-error assertions; do not mistake a green
CTest total for a validation-clean renderer. Resource/scene tests showed no VUIDs
in the inspected final logs.

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
