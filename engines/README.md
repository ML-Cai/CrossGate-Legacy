# Scene lifecycle

Startup creates the window, device, render pass, frame command buffers and
synchronization objects in ClientEngine. InitScene owns asynchronous reader
initialization; when ready it requests AnimationScene. Reader results remain in
engine state after the loading scene is destroyed.

Each frame polls input, applies pending scene transitions, updates the active
scene, and runs FrameRenderDrawSystem. The renderer begins its persistent render
pass and calls IScene::draw to record scene content before submitting/presenting.

AnimationScene owns a single CharacterAnimation. Press R to request a new
AnimationScene instance. The manager validates the destination, exits and destroys
the old scene, then enters the new one. Requests made by onEnter survive to the
next update. Failed entries remain owned so shutdown still calls their cleanup.

Before releasing its texture and pipeline, AnimationScene waits for GPU work.
Frame resources and readers survive transitions. Final engine shutdown exits the
scene, destroys frame resources, then destroys the device and window. This stage
does not introduce shared animation caches or ECS playback components.

```powershell
cmake --build build
ctest --test-dir build -R "SceneManagerTest|DesktopSceneTest|cgl_client" --output-on-failure
```

SceneManagerTest uses lightweight scenes to verify transition ordering, requests
from onEnter, same-scene restart, unsupported destinations and failed-entry cleanup.
DesktopSceneTest loads the real animation, draws and restarts it three times, and
checks that render-pass/frame resources and reader instances survive. The desktop
tests require Vulkan, validation layers and configured original game files.
