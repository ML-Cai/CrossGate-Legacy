# Resource and rendering follow-up plan

Current base: merged PR #73 (bounded preparation/upload overlap). Each follow-up
must run independently on its merged develop base. Scope is selected from actual
profiles, not a commitment to speculative abstractions.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Current | Reuse scene-owned CPU sorting scratch and flat adjacency; skip provably disjoint prefixes; reorder draw items in place | Exhaustive order/cycle oracle, dense actors, payload preservation, capacity release, full normal/ASan, fixed preloaded crowd and streaming comparisons |
| Next | Profile remaining draw recording/submission costs and reduce redundant work without changing painter order | Same ready resources and actor counts, draw/visual equivalence, command-buffer state and frame-slot lifetimes, actual Vulkan validation and uncapped frame timing |
| If warranted | Refine capacity/working-allowance backpressure and long-session residency from traces | Completed and censored misses together, visible coverage, pending ages, exact ready/retired/pending accounting and process/device memory measurements |

Estimate: **2-3 PRs including the current change** for this near-term work, depending
on whether resource-policy follow-up remains necessary after rendering profiles.
This is a scope estimate, not a calendar deadline. Do not combine CPU sorting,
GPU state/batching, visible-residency policy and platform rewrites into one PR.

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
