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
transparent), honoring per-image palettes. A deterministic shelf atlas with
one-pixel transparent borders replaces the padded ground texture array. The atlas
is bounded to 2048x4096 (32 MiB RGBA); packing is checked before image decoding.
Images must fit within that atlas including borders. Unknown IDs and upload
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
objects and characters then share one back-to-front list ordered by anchor depth
`y-x`, then horizontal position `x+y`, then stable object/entity identity. Equal
anchors draw objects before characters. Image height and animation pose never
change sorting. The color-only sprite renderer preserves transparent holes
without writing depth. This is whole-sprite anchor sorting; intersecting building
pieces and finer multi-tile occlusion would need separate artwork/geometry rules.

CharacterMovement stores integer cell/target coordinates, step progress, speed,
facing, movement state and shared idle/walk clips. MapScene spawns at integer
(width/2, height/2), rounded down, and preloads all eight directions before play.
UpdateCharacterMovement runs before UpdateAnimations. WASD is screen-relative:
W/A/S/D means north/west/south/east. W+A/W+D/S+D/S+A means NW/NE/SE/SW. The input
signs are converted to a neighboring MAP cell with dx=sign(horizontal-vertical),
dy=sign(horizontal+vertical). A step takes its MAP-coordinate distance divided
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
tests need Classic data including `map/0/2100.dat` and Vulkan; pure map tests and
sprite readback do not require original game data.

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
