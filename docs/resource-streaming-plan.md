# Resource and rendering follow-up plan

Current base: merged PR #76 (optional GPU/CPU phase timing). Each follow-up
must run independently on its merged develop base. Scope is selected from actual
profiles, not a commitment to speculative abstractions.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Reduce ready-node scheduling cost while preserving exact map painter order | Headless priority membership and exhaustive graph/reference tests; explicit scratch release; full normal/ASan; interleaved native CPU/GPU-timed crowd and moving-map streaming comparisons |
| Next, if supported by profiles | Investigate CPU sorting/recording; trial order-preserving batching where profiles justify it | Same ready resources and actor counts, transparent order, frame-local buffer lifetimes, actual Vulkan validation and long-frame costs |
| If warranted | Refine capacity/working-allowance backpressure and long-session residency from traces | Completed/censored misses, coverage, pending ages, exact accounting and process/device memory measurements |

Estimate: **2-3 PRs including the current change** for this near-term work, depending
on whether resource-policy follow-up remains necessary after rendering profiles.
This is a scope estimate, not a calendar deadline. Do not combine GPU batching, visible-residency policy and platform rewrites into
one PR.

#73 raises appearance readiness consistently in the measured large cases but does
not uniformly improve FPS or successful latency tails. More ready actors also
mean more drawing. The next rendering comparison therefore includes a fixed,
fully preloaded crowd to separate rendering costs from asynchronous readiness.
See [bounded preparation measurements](bounded-animation-preparation-validation.md).

The current sort keeps the existing footprint constraints and deterministic cycle
fallback. Retained scratch reduces allocation churn but is not a hard CPU memory
budget: overlapping large-footprint graphs still have quadratic worst cases.
Removing offscreen graph nodes or caching static constraints needs separate
correctness analysis, including transitive constraints, movement and viewport
changes. See [sort scratch validation](map-sort-scratch-validation.md).

Keep existing visible resource owners held. Do not add repeated uploads or change
eviction quality merely to raise a distinct-clip count. Successful miss windows
exclude withdrawn/pending demand; compare both outcomes, lifetime pending-age
maxima, coverage and frame cost. Queued time includes global capacity pressure as
well as waiting behind busy CPU/GPU slots. RGBA ledger bytes and process RSS are
not total physical GPU residency, especially on the integrated development GPU.

The LRU policy still scans/sorts the cache during throttled pressure maintenance;
profile that cost before replacing it. Count limits are not CPU-time guarantees,
and map loading remains synchronous. Android/Linux lifecycle/surface adaptation,
CPU-only public APIs/Python bindings, ECS reference safety and UI focus/command
boundaries remain separate architecture work.

The former BasicDrawFlow presentation-layout issue was addressed in #71; its
teardown checks the validation counter through device-child cleanup. Continue
inspecting complete positive validation logs for other fixtures too. See
[basic draw validation](basic-draw-validation.md) for the negative control.

The command-buffer binding optimization and native interop contract are recorded in
[sprite command state validation](sprite-command-state-validation.md). Raw Vulkan
state changes through retained handles require explicit invalidation before the
next tracked sprite draw; UI/custom native recorders must preserve this boundary.

GPU timing scope, sample semantics and final native results are recorded in
[GPU frame timing validation](gpu-frame-timing-validation.md). Diagnostic-disabled
execution keeps query pools and timestamp commands out of the normal path.

The PR #76 fixed-crowd profiling session recorded roughly 5.8-5.9 ms CPU recording,
3.6-3.7 ms of benchmark sorting, 3.1 ms GPU interval and 0.01 ms frame-slot fence
wait on the development host. These are overlapping/independent windows, not
additive costs or a universal GPU classification. Prioritize the measured CPU
path and preserve the GPU timing control for future rendering comparisons.

The current CPU ready-index optimization and its complete validation are recorded
in [map ready-order validation](map-ready-order-validation.md). It does not remove
constraints, cull graph nodes, cap actors or cache geometry across movement.

In the current interleaved ready-index comparison, median sorting falls
1.45208 -> 1.22359 ms and CPU recording 2.11966 -> 1.90657 ms, while FPS remains
roughly 347-348 and frame-slot fence waiting increases. Remaining GPU/backpressure
therefore matters on this session/workload; select the next batching/recording
trial from contemporary controls rather than the earlier session alone.
