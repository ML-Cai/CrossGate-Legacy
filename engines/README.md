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
device/render pass. Scene exit deletes its entities; the cache retains clips for
pending GPU work and later scene entries. Final shutdown waits for GPU completion,
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
