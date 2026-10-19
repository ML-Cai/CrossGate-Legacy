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

When `Map.FilePath` is set, InitScene requests MapScene instead. GroundMapLibrary
loads its MAP layout and unique nonzero ground IDs from Classic resources. It
decodes BGR palettes into top-down RGBA (index 0 transparent), honoring per-image
palettes and offsets. One immutable texture array contains padded frames; it is
limited to 512x512 per frame, 128 MiB of RGBA data, and the device's texture-array
limits. Unsupported/missing IDs and upload failures report a scene error. No ID
remapping or debug-ID suppression is applied.

MapScene owns a MapCamera singleton. Arrow keys pan at 300 screen pixels/second;
the center is clamped to map bounds. The camera uses fractional cell coordinates
and the Classic 64x47 isometric projection: file x increases northeast and file y
increases southeast (`worldX = 32 * (x+y)`, `worldY = 23.5 * (y-x)`). This follows
the MAP ordering described in https://cgsword.com/filesystem_graphicmap.htm and the
legacy coordinate conversion. Rendering transforms the viewport
corners back to map space, expands for image offsets, visits that bounded region,
and rejects individual offscreen rectangles. Camera movement changes positions
without rebuilding or uploading textures. R resets the camera; scene exit removes
it. The engine retains GroundMapLibrary across scene exits and clears it after GPU
completion and scene teardown, before destroying frame/device resources.

MapCameraTest covers projection/inverse, elapsed-time pan, diagonal speed, zoom,
edge clamps and culling against a brute-force reference. A fixed, numbered 3x2
layout additionally pins actual Classic axis directions, since round-trip tests
alone cannot detect two transforms that share the same incorrect orientation.
DesktopSceneTest also
draws map 2100, pans, restarts three times, checks upload reuse and camera/cache
cleanup. `cgl_client_map_*` verifies startup and invalid path/scale cleanup. These
desktop tests need original Classic data including `map/0/2100.dat` and Vulkan.

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
