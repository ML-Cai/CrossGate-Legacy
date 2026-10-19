# Sprite rendering

`ISpriteRenderer` uploads equally sized, top-down RGBA8 frames to a Vulkan texture
array. Draw calls select a frame and place it at pixel coordinates relative to
the viewport. Filtering is nearest-neighbor; blending uses straight alpha.

Create the renderer with a device and a compatible single-sample, color-only
render pass. Call `draw` between that pass's `begin` and `end`, and keep the
renderer alive until submitted GPU work finishes. Creation performs a synchronous
upload on the graphics queue and must be serialized with other queue submissions.
Draw calls bind their own pipeline, descriptors, viewport and scissor state.

The Vulkan SDK's `glslc` is required at build time. Compiled shaders are embedded
in the library, so execution does not require shader files or a particular
working directory.

```powershell
cmake --build build
ctest --test-dir build -R SpriteRendererTest --output-on-failure
```

These tests use synthetic images and offscreen GPU pixel readback to check color
channels, top-down orientation, transparent/half-alpha blending, nearest scaling,
positioning, frame selection, invalid input, and resource cleanup. A Vulkan-capable
desktop and validation layer are required; original game resources are not needed.

Character decoding, animation playback and scene management are handled by the
client; this module only uploads and draws the selected RGBA frame.
