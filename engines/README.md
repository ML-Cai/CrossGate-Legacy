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
