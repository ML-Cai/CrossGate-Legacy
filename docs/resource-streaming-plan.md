# Resource and rendering follow-up plan

Current base: merged PR #85 (progressive map loading). Each follow-up
must run independently on its merged develop base. Scope is selected from actual
profiles, not a commitment to speculative abstractions.

| Priority | PR scope | Required validation |
| --- | --- | --- |
| Complete (#82) | CPU-only CI build/test lane | Shared native test registry, SDK-free builds, Windows/Linux ordinary and ASan jobs, retained failure artifacts |
| Complete (#83) | Explicit visible-appearance readiness policy | Priority/fallback behavior, coverage, pending demand age and no premature resource release |
| Complete (#84) | CPU map preparation boundary | Owned metadata and one-page pixel results, no GPU/window requirements, format fidelity and failed-page cleanup |
| Complete (#85) | Progressive map loading | Background preparation and bounded GPU submission, cancellation and failure recovery; preserve a working native client |
| Current | Incremental scene-entry catalog | Measure entry phases, bound candidate work, preserve ordering/filters and cancel partial scene ownership |
| Then | Resource memory/profiling follow-up | Profile local character entry, distinguish payload/reservations from physical host/device memory and validate dedicated GPUs |
| Profile-driven | Further recording/batching or graph optimization | Same resources, painter order, comparable frame/GPU costs and explicit buffer lifetimes |

The catalog step and its measurements are recorded in
[catalog entry validation](catalog-entry-validation.md). Candidate count limits do
not guarantee a wall-time deadline; local clip acquisition remains a separate step.

The earlier 2-3 PR rendering estimate is superseded by this functional sequence.
The soak/tooling, CPU-only CI and readiness PRs are merged. Keep hosted CI dependency/build changes separate
from resource readiness and map-loading behavior.
Progressive map loading proceeds from CPU data ownership to background preparation
and upload scheduling. Further split those steps if cancellation/lifetime
boundaries require it; do not promise a fixed total before those interfaces settle.

The CPU map preparation step in #84 preserved synchronous scene entry and the
existing atlas layout. It plans every page from metadata, builds at most one RGBA
page at a time and keeps GPU ownership in MapLibrary. Ordinary scenes now build
on that boundary with progressive loading; the synchronous API remains available. See
[CPU map preparation validation](map-cpu-preparation-validation.md).

The readiness change preserves visible-first selection, same-region stability,
budget-fit selection and existing aging. It represents unresolved remote actors
with a shared ground marker and publishes queued/loading/deferred/failed/ready
state separately from whether a ready clip is actually pinned for presentation.
It does not rotate visible victims or guarantee full coverage under an oversized
visible working set. See [fallback validation](visible-appearance-fallback-validation.md).

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

The bounded LRU path still scans the cache during throttled pressure maintenance,
but selects only the oldest requested K candidates rather than sorting every idle
entry. Unbounded trim keeps full sorting. See
[idle-cache selection validation](idle-cache-selection-validation.md). Count limits are not CPU-time guarantees,
and the benchmark map path remains synchronous. Ordinary map scenes now use
[progressive map loading](progressive-map-loading-validation.md). Android/Linux lifecycle/surface adaptation,
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

The current equivalent four-vertex quad change and its validation are recorded
in [sprite quad validation](sprite-quad-validation.md). It preserves the current
triangle coverage and introduces no buffer, allocation, feature or lifetime.

Current quad measurements show a small 1080p GPU reduction (5.33376 -> 5.26104 ms)
and FPS 153.575 -> 156.557; 720p FPS direction changes between execution-order
groups, and streaming FPS does not improve uniformly. Keep gains workload-specific.
A future per-frame draw-region crop must preserve the full sorting envelope and
account for CPU metadata ownership/bounds; it belongs in a separate measured PR.

The four-vertex quad work is merged in #78. The finite-geometry guard merged in #79
is documented in [sprite geometry validation](sprite-geometry-validation.md).
Transparent-padding trials were withdrawn: the measured GPU saving did not
offset CPU overhead, and stricter eligibility selected no clips in the tested
1,024-identity corpus. Revisit cropping/batching only with measured workload
evidence; preserve sampling, painter order and explicit GPU/CPU ownership.
