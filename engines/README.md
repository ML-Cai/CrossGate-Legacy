# Scene lifecycle

Startup creates the window, device, render pass, frame command buffers and
synchronization objects in ClientEngine. InitScene owns asynchronous reader
initialization; when ready it requests AnimationScene. Reader results remain in
engine state after the loading scene is destroyed.

Each frame polls input, applies pending scene transitions, updates the active
scene, and runs FrameRenderDrawSystem. The renderer begins its persistent render
pass and calls IScene::draw to record scene content before submitting/presenting.

AnimationScene owns two entity IDs. Each entity has Transform2D, Sprite and
AnimationPlayer components. Both Sprite references use one immutable cached clip,
while each player has independent time, speed, pause and looping state.
UpdateAnimations receives explicit delta time and selects frames before drawing.
Press R to request a new
AnimationScene instance. The manager validates the destination, exits and destroys
the old scene, then enters the new one. Requests made by onEnter survive to the
next update. Failed entries remain owned so shutdown still calls their cleanup.

ClientEngine owns AnimationLibrary and supplies it to scene creation. The cache
is keyed by Classic/daytime serial, direction and motion, and bound to one live
device/render pass. Scene exit deletes its entities; the cache retains clips until synchronized
idle maintenance or final shutdown. Final shutdown waits for GPU completion,
exits the scene, clears the cache, then destroys frame resources, device and window.
Do not evict cached GPU resources while submitted draws can still use them.

When `Map.FilePath` is set, InitScene requests MapScene instead. MapLibrary
loads the MAP layout and unique ground/object images from Classic resources. It
preserves image offsets and converts BGR palettes into top-down RGBA (index 0 is
transparent), honoring per-image palettes. Deterministic shelf packing uses up to
8 atlas pages, each bounded to 2048x4096 (32 MiB RGBA), with one-pixel transparent
borders. All page plans and limits are checked before image decoding. One image
must fit within a page including borders; duplicate IDs are rejected by the planner.
Pages decode/upload sequentially, keeping only one CPU RGBA page buffer at a time.
The maximum RGBA texture payload per map is 256 MiB, excluding GPU/staging overhead.
Each MapImage stores its page index and region; MapResources owns all page renderers.
Ground remains first, and objects/characters keep a single globally sorted draw
list regardless of page, so switching pages cannot reorder overlapping sprites.
The full MAP layout and unique images load at scene entry; per-frame iteration
still uses VisibleMapBounds expanded by sprite reach. This is not chunk streaming.
Classic map 100 in the local fixture is 840x610 cells, 953 images and two pages
with 40,330,752 RGBA texture bytes (about 38.46 MiB). Unknown IDs and upload
failures report a scene error. Classic placeholder IDs 0/2 and object markers
200 through 265 are skipped; resource-version ID remapping and CUT files remain
unsupported.

MapScene owns a MapCamera singleton and one character entity with MapPosition,
Sprite and AnimationPlayer. Its clip comes from AnimationLibrary and uses the
existing Animation settings. The clip retains its canvas offset relative to the
ground anchor. WASD moves between integer MAP cells at 4 coordinate units/second;
render positions interpolate between endpoints. The camera follows its ground anchor, scrolling the map
opposite to movement while the character stays centered. Movement clamps to map
bounds, without collision or pathfinding. R resets camera and character.

The Classic 64x47 projection has file x increasing northeast and file y southeast
(`worldX = 32 * (x+y)`, `worldY = 23.5 * (y-x)`), following the MAP ordering at
https://cgsword.com/filesystem_graphicmap.htm and the legacy coordinate conversion.
The viewport expands by the full image reach before culling ground anchors, so
large objects whose anchors lie offscreen still appear. Ground is drawn first;
objects and characters share one back-to-front list. The baseline key is anchor
`y-x`, then `x+y`, then stable identity (objects before actors at equal anchors).
GraphicsResourceInfo preserves native tileX/tileY, and MapImage carries the
normalized footprint dimensions (zero dimensions default to one for static images).
A Classic object footprint grows from its front/west placement cell toward +MAP X
and -MAP Y. Cell-center bounds are [x-.5,x+tileX-.5] and
[y-tileY+.5,y+.5]; actors use their fractional ground position as a point.

For overlapping sprite rectangles involving a multi-cell object, disjoint ground
extents establish precedence: the object wholly toward +X or -Y draws earlier.
Opposing/ambiguous relations and intersecting footprints retain baseline ordering
where unconstrained. A screen-X sweep limits candidate pairs, then a stable
priority-queue topological sort applies precedence. Footprint precedence is not
used directly as a std::sort comparator, because it is only a partial order.
Cycles fall back deterministically to baseline order. Single-cell pairs preserve
the original ordering. Pixel offsets, image height and animation poses affect
image overlap/culling but do not change the footprint geometry.

The color-only sprite renderer preserves transparent holes without writing depth.
This remains whole-sprite sorting: interlocking geometry, overhangs and characters
inside a building footprint can still require image splitting or explicit masks.
Footprints here are rendering data only; no collision detection is added.

CharacterMovement stores integer cell/target coordinates, step progress, speed,
facing, movement state and shared idle/walk clips. MapScene spawns at integer
(width/2, height/2), rounded down, and preloads all eight directions before play.
UpdateCharacterMovement runs before UpdateAnimations. WASD is screen-relative:
W/A/S/D means screen up/left/down/right. W+A/W+D/S+D/S+A means screen
NW/NE/SE/SW. The input
signs are converted to a neighboring MAP cell with dx=sign(horizontal-vertical),
dy=sign(horizontal+vertical). Animation indices start at screen NW (0) and
continue clockwise: up=1, NE=2, right=3, SE=4, down=5, SW=6, left=7.
Screen movement sectors start at up, so clip selection adds one modulo eight. A step takes its MAP-coordinate distance divided
by cellsPerSecond (default 4): 0.25 seconds for one axis, sqrt(2)/4 for two axes.
Input is sampled at cell boundaries. Release completes the current step; a turn
cannot redirect a step halfway through. Remaining frame time can advance further
steps while held, and completed steps snap exactly to their integer destinations.
The interpolated MapPosition is for rendering and camera following. Out-of-bounds
steps are rejected as a whole, without sliding or consulting collision flags.
Walk phase is preserved on turns; walk/idle transitions reset the animation cycle.
The camera follows the character ground anchor every update, keeping it at the
viewport center even at map edges. Arrow keys do not pan in map mode.
Standalone animation preview settings are unchanged.
All cached animation texture arrays together are limited to 256 MiB of RGBA data
(excluding renderer/staging overhead); missing idle/walk clips or budget overflow
fail during scene entry through the existing error/cleanup path.

CharacterMovementTest covers eight grid neighbors, constant MAP-coordinate speed,
release completion, delayed turns, gait phase, frame partition independence,
whole-step map boundaries, invalid input/speed and unavailable clips. The real map lifecycle
test checks centering and opposite terrain displacement for all eight directions,
submits both walk and idle renderers, checks clip identity, then restarts and
releases the caches.

Camera/character movement changes positions without rebuilding textures. Scene
exit deletes its character and camera; engine-owned map and animation caches
retain uploads until shutdown waits for GPU completion, exits the scene and
clears both caches before destroying frame/device resources.

WindowInputSystem publishes the current logical cursor position, client size and
hover/focus state in CursorPositionState each frame, including when the cursor
does not move. MapScene converts it to render-surface/viewport pixels and reuses
ScreenToMap to populate its MapHover singleton. Cells own [n-0.5,n+0.5) along each
map axis, with exact ties going to the larger coordinate. Selection rejects points
outside map bounds and missing ground IDs 0/2; it does not interpret walkability or select
objects. Leave, minimize or focus loss hides the indicator.

A transparent-center yellow diamond is drawn after scene objects as an overlay.
MapLibrary generates and uploads its bitmap once at the configured scale, keeping
a visible two-pixel stroke even at scale 0.125. Placement uses the same map anchor
and camera scale as ground drawing. The library retains this outline across R
restarts; scene exit removes MapHover and drops its reference, and shutdown clears
the library after GPU completion. Ground/atlas pixels never change for hover.

MapCameraTest pins Classic axes using a numbered 3x2 fixture and checks inverse,
pan, bounds and culling. MapRenderTest checks atlas placement/limits, marker
filtering, large-image visibility and object/character ordering. SpriteRendererTest
uses offscreen pixel readback for source rectangles and transparency under both
draw orders. DesktopSceneTest loads map 2100 with buildings and a character, pans,
moves, restarts three times, checks cache reuse and entity/camera cleanup.
`cgl_client_map_*` covers startup and invalid path/scale/animation cleanup. Desktop
tests need Classic data including `map/0/2100.dat`, `map/0/100.dat` and Vulkan; pure map tests and
sprite readback do not require original game data.
Footprint regression tests use map 2100 house 11283 (4x6, offset -210/-437):
actor (49,58) is outside the front/west wall but would be hidden by anchor sorting;
actors behind the other two footprint edges must remain behind. Asset-reader tests
pin the native footprint bytes, and a real map scene test checks both front/back
ordering with actual resources and submits both views to Vulkan.
MapRenderTest also checks multipage packing, padding, deterministic page assignment,
page limits, oversized images and duplicate IDs. SpriteRendererTest reads back
alternating page A/B/A draws to check texture binding, transparency and painter order.
The large-map lifecycle test visits anchors from every page, moves/restarts twice,
checks page-renderer cache identity, and verifies final map release.

MapHoverTest covers center/edge picking, negative map boundaries, stationary cursor
under pan/scale changes, logical-to-render conversion with a viewport offset,
empty ground and hollow outline pixels at scales 0.125 through 16. The desktop
map lifecycle test also draws a hovered cell, hides it on leave/out-of-window
coordinates, verifies stationary-cursor updates and checks outline reuse/release
and MapHover cleanup across three scene restarts.

Component addresses can change on insertion or swap-removal. Store entity IDs
and requery components; perform entity/component additions and removals outside
forEach iteration. Singleton identity is also cleared when its entity is destroyed.

```powershell
cmake --build build
ctest --test-dir build -R "engine_animation|engine_ecs|SceneManagerTest|DesktopSceneTest|cgl_client" --output-on-failure
```

SceneManagerTest uses lightweight scenes to verify transition ordering, requests
from onEnter, same-scene restart, unsupported destinations and failed-entry cleanup.
DesktopSceneTest loads the real animation, draws and restarts it three times, and
checks that frame resources, readers and the shared clip survive, exactly two
sprite entities exist on every entry, scene exit removes them, and final cache
cleanup releases the clip. Unit tests cover independent playback, pause, reverse
and nonlooping playback, large time steps, entity deletion and singleton recreation.
The desktop tests require Vulkan, validation layers and configured original game files.

## CPU animation assembly and description lifetime

AnimationLibrary delegates pixel preparation to the CPU-only AnimationBuilder.
It first reads frame metadata and validates positive dimensions, the complete
4096x4096 canvas bound and the remaining 256 MiB animation texture-payload budget.
Only then does it read the palette, allocate the aligned RGBA array, and decode
one indexed frame at a time. Each indexed allocation is destroyed before the
next frame is decoded. Offsets, bottom-up native rows, BGR conversion and index-0
transparency preserve the previous output convention. RD/metadata dimension
mismatches still fail before any renderer is created.

The builder retains only the last character's parsed description, so acquiring
idle/walk directions for one character avoids repeatedly parsing all motions.
Changing character or anime reader identity discards it; description limits
match the reader's 65,536 frames per motion and 1,048,576 frames in total, with
at most DirectionTypes::COUNT x MotionTypes::COUNT motions. Failed frame builds
may retain this valid description, but publish no partial pixel result or GPU clip.
The output owns its pixels independently of the builder and readers.

AnimationLibrary::clear() releases cached clips and the CPU description, and
resets device/render-pass binding. Its existing prerequisite remains: release
scene/entity clip references and wait for submitted GPU work before clearing.
Use clear after reloading resources in place as well; unchanged reader addresses
cannot detect edited files, and existing GPU clips would otherwise remain stale.
Builder/library access must be serialized. No GPU eviction during in-flight draws
or background upload is introduced.

The reduction removes simultaneous retention of all decoded indexed frames;
it does not eliminate the RGBA array, per-reader scratch/cache, upload staging,
or live GPU allocations. The texture-payload limit is not a total process-memory
limit. Metadata/header mismatches are rejected after the reader returns its
bounded decoded result; a shared budget for all CPU/GPU allocations remains future
work, along with explicit index-preload limits and Python invalidation/telemetry.

`ctest --test-dir build -R AnimationBuilderTest --output-on-failure` runs synthetic
CPU-only tests for pixels, alignment, cache invalidation, early budget checks,
mid-build failure/retry and repeated cleanup. The target links no renderer,
Vulkan or window library. Existing desktop tests cover client integration and
scene/entity/GPU cache cleanup.

## Crowd performance diagnostics

See [native crowd benchmark](../tools/benchmarks/README.md) for density stress tests,
uncapped frame timing, reproduction commands and measured limitations.
Animation requests can select an installed data version; Classic remains the default.

## Sprite pipeline ownership and incoming characters

Vulkan sprite renderers share one immutable pipeline, pipeline layout, descriptor
layout and nearest sampler per render-pass object. The pass retains the bundle
across texture churn, and live renderers retain it if the pass wrapper is released.
There is no process-global cache or raw-handle key that can survive device reuse.
Texture images, views, descriptor sets/pools and upload staging remain per renderer.
Creation is serialized on the render thread. Existing shutdown rules still apply:
wait for submitted work, release renderers/passes, then destroy the device.
A failed bundle is not published; its partial Vulkan objects are destroyed by RAII.
The fixed shader/blend/sample/subpass configuration must gain an explicit variant
key before this cache can serve different pipeline states. No cross-pass sharing
is attempted.

`Benchmark.LoadDuringPlayback` diagnoses the case where previously unseen NPC/player
appearances are encountered during play; see the benchmark README. It is intentionally
synchronous and does not establish a production no-stall guarantee.

The next streaming steps should preserve these constraints:

- Main-thread appearance requests return a ready handle or placeholder immediately;
  repeated version/serial/direction/motion requests share one pending operation.
- Workers decode CPU pixels using serialized reader access (current readers/builders
  are not thread safe). Bound queued jobs, decoded bytes and total CPU scratch;
  a thread pool alone does not bound memory. Vulkan calls stay on the render thread.
- A GPU uploader limits bytes, submissions and CPU work per frame. Large clips must
  be split into bounded copy regions/layers; a count of one clip is not a time budget.
  Replace queue-wide idle with submission fences/completion polling. Retain staging
  and destination handles until completion, and publish only ready resources.
- Visibility/distance prioritizes work; a scene generation cancels obsolete requests.
  Cancellation before submit frees CPU data; after submit it waits for GPU completion
  before releasing staging/destination. Completed stale jobs must not publish into
  a new scene. Offscreen eviction must respect live and in-flight references.
- Shutdown stops acceptance, joins workers before closing readers, retires submitted
  uploads, clears ready caches, then destroys render passes/device. Track pending
  bytes, resident bytes, upload latency and dropped/cancelled work separately.
- Verify random late arrivals while moving, rapid direction/motion changes, repeated
  requests, oversized clips, queue saturation, scene changes and shutdown mid-upload.
  Measure frame p99/max alongside appearance-ready latency; do not hide slow loading
  by leaving placeholders forever. Actual GPU allocation/budget telemetry is still
  needed for 4/8 GB dedicated-card targets.

This PR provides the shared immutable bundle and diagnostic coverage; background
workers and a fence-retired bounded uploader are subsequent independent changes.

## Progressive sprite uploads

`ISpriteRenderer::createPending` moves CPU RGBA ownership into a pending renderer.
It copies whole rows across array layers, at most 4 MiB per chunk by default.
`pollUpload` queries its fence without waiting; after completion it reuses staging
and the command buffer for at most one next chunk. The final copy transitions the
whole image to shader-read layout. Pending draws are ignored. Publication only
happens after the final fence signals. Synchronous `create` now waits on its own
fence instead of `vkQueueWaitIdle`; it remains useful during scene initialization.

`AnimationLibrary::acquireProgressive` holds one pending clip, caps decoded RGBA at
32 MiB, and reserves the destination payload against the existing resident texture
budget. CPU pixels move into the uploader, avoiding a second full RGBA copy. One
staging buffer is at most 4 MiB; these are payload bounds, not GPU allocation/RSS
bounds. The limit covers the measured 24 MiB/114-frame corpus clip. Larger clips
fail explicitly before RGBA allocation; no silent appearance substitution counts
as successful loading. Other requests retry while the slot is occupied; identical
requests share the pending operation and later the ready cache entry.

The current cold benchmark pumps this path once per frame. This is not a global
upload scheduler: callers must serialize it and must not poll once per entity.
Cached idle plus pending walk still submits at most one chunk in a benchmark update.
A clip is attached to actors only when idle and the requested walk are ready.
Scene exit cancels the pending clip; cancellation/destruction can wait for its own
already-submitted chunk before freeing CPU/staging/image resources. Ready resources
still follow the existing wait-before-cache-clear rule. Shutdown and forced budget
failure use the same ownership path. No GPU object is freed merely because a scene
stops referring to a pending request.

Remaining work: decoding and Vulkan allocation still execute synchronously, so a
large cold clip can stall its first request. Background decoding needs a bounded
byte/job queue with serialized reader access, cancellation generations, visibility
priority and worker shutdown before reader destruction. The pending slot currently
has no queued jobs to cancel and no concurrent worker publication. A production
streaming service should centralize per-frame upload byte/submission/time telemetry,
choose priorities, expose ready latency, and evict only resources unused by both
entities and submitted GPU work. Dedicated transfer queues, persistent staging pools,
real GPU allocation telemetry and 4/8 GB card validation are subsequent work.

GPU allocations and descriptor pools are still created per texture. Before growing
the number of resident directions/motions, collect actual allocation counts/bytes,
check device allocation limits, and introduce pooled/suballocated storage where
measurements justify it. Resident payload limits alone do not prevent allocation
count exhaustion or driver overhead. This is separate from the CPU decode queue.

## Background animation decode

`AnimationLibrary::acquireProgressive` now defaults to background CPU decoding.
A single persistent CPU-only worker is started lazily and reused, rather than
creating a thread per appearance. The queued/running/completed result and the GPU
pending slot share ONE end-to-end slot: decoding does not run ahead of an upload.
Reservation is min(remaining resident payload budget, 32 MiB) before submission;
it remains reserved until the result is taken, even if actual pixels are smaller.
Retained vector capacity is checked as well as the builder's planned pixel size,
so a small resized result cannot keep a larger unaccounted buffer.
`pendingBytes()` reports this reservation during decoding, then destination payload
during upload. This is not measured CPU RSS/VRAM. Failed and cancelled jobs cannot
leave an unbounded collection of results. GPU staging remains <=4 MiB per update.

The worker only assembles CPU pixels. Vulkan allocation, descriptors, copy submission,
fence polling and publication stay on the render thread. The first request returns
nullptr promptly; later calls take a ready result without waiting and move its pixel
buffer into the uploader. A CPU task exception is transferred to the owner; no partial
clip is published. The same requested key shares the existing task/cache entry;
other keys retry while this conservative slot is occupied. Synchronous uncached
acquire is rejected while CPU/GPU work is pending. This is backpressure, not a
production visibility-priority queue or per-NPC task scheduler.

Engine builders, map loading and crowd metadata enumeration serialize reader queries
with `AssetsReaderState::queryMutex`. The shared mutex preserves the component's
move support; the state must NOT move or replace/close readers while a worker uses
it. Every additional engine reader query must use that lock. Module readers and
Python Session instances are not made implicitly thread safe by this engine lock.
Never hold the reader lock while cancelling/joining a worker that may need it.
Avoid synchronous main-thread queries while decoding; even safe lock contention can
stall a frame. Current scene transitions cancel/join access before loading a new map.

Cancellation checks precede metadata/frame work and each pixel row. It cannot abort
a reader call already executing. Scene exit waits for CPU access to finish, discards
its result, then retires any submitted GPU chunk. This strict cancellation barrier
prevents an old completion from publishing into a new scene without a generation
counter. Clear/destruction stops and joins the worker BEFORE clearing builder/cache
state or allowing readers/device to be destroyed; no detached thread holds readers.
A cleared library can start a new worker when reused. Cancellation/scene transitions
can wait; normal frame polling does not wait for task completion.

Follow-ups remain important: a bounded priority queue for simultaneous appearances
(the single slot has head-of-line blocking), ready/failed request states for tools,
visibility cancellation and measured time-to-appearance targets, GPU suballocation /
descriptor pooling, live allocation telemetry and fence-safe resident eviction.
Do not add a large thread pool or decoded-result queue to hide this backpressure.

## Map scenario ownership

MapScene creates and clears the optional CrowdScenario (benchmark) or
ServerDemoScenario (local server simulation). They are concrete scene-owned
helpers, not a replacement scene/state framework. MapScene adapts player input and owns scenario/world-renderer lifetimes.
The benchmark's draw counters still observe the same shared rendering path.

CrowdScenario owns benchmark entity IDs, motion switching and cold-load sampling.
ServerDemoScenario owns the local feed, remote-character bridge, snapshot cadence
and status updates. The two-second/half-prefetch-margin decision is a CPU-only
function with boundary/zoom/minimized-viewport tests. The real clock is read only
by the demo adapter. Existing 128 visible + 160 outer actors and N/R controls stay
the same. The remote-character bridge still uses the local feed's region helper;
a production network snapshot API is a later step.

On scene exit both helpers clear their owned entities and clip references first;
MapScene then cancels the library's pending decode/upload before removing the
player and map references. This ordering also applies to partially failed entries.
Ready clips remain engine-cache-owned until GPU-safe shutdown. Helpers do not
clear engine caches or destroy the device, and must not outlive their ECS world.
Tests exercise demo-to-benchmark-to-demo transitions with pending appearance work,
plus repeated restart, cold-load publication and final weak-reference expiration.

## World update and recording boundaries

UpdateMapWorld advances grid movement, camera follow and animation using explicit
movement/time/map dimensions, with no window or device access. MapScene continues
to orchestrate benchmark/demo modes and restart requests; the benchmark preserves
its existing animation/update cadence.

MapWorldRenderer records ground first, then the globally footprint-sorted object
and actor list, then the hover outline. It consumes borrowed map/clip renderers,
camera and a viewport-local hover value; it does not move actors, advance playback,
write world hover, poll uploads or acquire resources. Scratch draw pointers are
cleared on both success and exceptions and own no GPU/clip lifetime. Engine caches
still retain submitted resources until safe teardown. This is a same-thread
recording boundary, not an immutable snapshot for concurrent rendering.

ResolveMapHover is shared by update and presentation. Update writes MapHover for
the full render surface; draw can compute a local presentation value for the
actual viewport without overwriting that world selection. Pointer snapshots retain
logical-window versus framebuffer dimensions for high-DPI conversion. Benchmark
hover is cleared during update. Render counters/timing are returned separately
and copied into the existing benchmark diagnostics by the scene adapter.

CPU-only recording doubles test ground/object/actor/overlay ordering, playback
and hover immutability, absence of upload polling, exception recovery and clip
reference release. The existing real Vulkan scene suite also verifies the draw
boundary while preserving map depth, movement, restart and mode transitions.

## Ready-cache maintenance boundary

SceneManagerSystem accepts an optional maintenance callback. It runs only after a
validated transition has exited/destroyed the old scene and before the new scene
enters. Callback failure raises a scene error and prevents entry; final shutdown
uses its existing wait/clear path instead of invoking transition maintenance.
ClientEngine wires the callback to the configurable animation/map soft targets.
The scene manager itself has no device/cache dependency. ClientEngine moves idle
ready entries to a deferred ownership queue at this
boundary. Each is tagged with the last successful graphics submission serial;
normal frame-slot fence waits acknowledge completion, and subsequent updates
collect completed ownership. Pending decode/uploads and externally held resources
are excluded. Retired animation payload remains charged to the hard budget.

The renderer currently uses one graphics queue for draws; its serial is not a CPU
frame count or presentation-completion token. Shutdown must still wait before
clearing retired queues and before destroying render passes/device. Explicit
library trimUnused remains blocking; it is not called by transition maintenance.
Automatic in-scene pressure eviction and retry when ready + retired allocations
cannot fit remain follow-up work. See docs/build-and-test.md for diagnostics and
limits, including temporary duplication on immediate re-entry.
