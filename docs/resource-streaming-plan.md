# Resource and rendering follow-up plan

Current base: merged PR #70 (fitting appearances can bypass a budget-blocked request).
This is an estimate of independently reviewable changes, not a claim that the
remaining features already exist. Each PR must run on its merged develop base.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Repair BasicDrawFlow render-pass/presentation validation and fixture cleanup; assert validation errors instead of only logging them | Real multi-frame clear/present; headless diagnostic-counter tests; negative control reproducing the old VUID and failing; repeated lifecycle runs; full normal/ASan and native validation smoke |
| Next | Measure demand-to-ready latency; decide whether distance-based visible residency or cheaper representations are justified | Visible-only overload; stable victim selection/hysteresis if introduced; quality/latency/FPS tradeoffs; no stale ownership or early GPU release |

Resource work remaining is estimated at **1-2 PRs**. The merged admission policy
retains already-presented visible clips, fills remaining space where possible,
and leaves unresolved actors as placeholders. It does not rotate live visible
clips or guarantee all appearances fit. Demand-to-ready observation and any
far-visible replacement policy should follow measured need rather than introduce
repeated uploads just to improve the number of distinct clips shown.

The present LRU policy scans/sorts the cache when pressure scans are due. Profile
that cost before replacing it with a persistent/incremental LRU structure. Count
limits are not CPU-time guarantees, and map loading is still synchronous.

The old BasicDrawFlow UNDEFINED-layout presentation error is addressed in the
current change. Its teardown now checks the backend validation-error counter,
including device-child cleanup diagnostics. Other fixtures retain their existing
checks; inspecting the complete validation logs remains necessary. See
[basic draw validation](basic-draw-validation.md) for the negative control and
reproduction commands.

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
