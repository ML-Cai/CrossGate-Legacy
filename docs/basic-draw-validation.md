# Basic draw validation and fixture cleanup

Base: develop `f2cf0c8` (PR #70). Validated 2026-10-24 on the development
PC's Radeon 880M with Vulkan validation enabled.

## Change

BasicDrawFlow previously submitted an empty command buffer and presented an image
still in UNDEFINED layout. The revised test uses the real built-in color render
pass to clear and transition each acquired image to PRESENT_SRC_KHR. It records
2 * swapchain image count + 1 frames, with a render-finished semaphore per image,
a graphics fence before command/acquire-semaphore reuse, and the image acquisition
boundary before reusing that image's presentation semaphore.

The fixture owns GPU children so fatal assertions cannot destroy submitted work
before teardown waits for the device. It then releases command buffers, render
pass/framebuffers, synchronization objects, swapchain and device before destroying
the GLFW window and terminating GLFW. Validation errors are checked after device
destruction while the backend wrapper/counter still exists.

A backend-only atomic counter records ERROR-severity VALIDATION-type messages,
including repeated identical messages. General loader diagnostics and warnings
remain logged but do not increment this counter. No public IDevice API or gameplay
rendering path changed. The VkDevice handle now starts null, making cleanup before
initialization safe. A windowless unit test exercises the counter's severity/type
filter, duplicate messages, combined type flags, and repeated unprepared cleanup.
CTest labels that test unit/memory; BasicDrawFlow remains a serialized desktop test.

## Results

- Full normal CTest: **249/249 passed**.
- Full MSVC ASan CTest: **248/248 passed** (the normal-only Python session test
  accounts for the difference). No ASan error markers in the inspected logs.
- BasicDrawFlow and diagnostic unit test: five successful CTest repeats each.
- BasicDrawFlow: five successful iterations inside one process, exercising repeated
  GLFW initialization/window/device creation, rendering and complete fixture teardown.
- Negative control: temporarily omitted render-pass begin/end, rebuilt the test,
  and reproduced VUID-VkPresentInfoKHR-pImageIndices-01430. Seven validation errors
  were counted; both the post-draw and post-device-destruction assertions failed
  as expected. Correct source was restored exactly, rebuilt, and passed the repeated
  positive checks. No negative-control code or switch is retained in the source.
- Inspected complete positive normal/ASan suite logs and same-process repetition
  log: **no VUID markers**. Synthetic messages in the counter unit are intentional;
  stale local Steam layer-manifest loader messages may still be logged.
- ASan native-client smoke with validation enabled: map100, VSync off, 240 scripted
  updates, 128 visible/160 prefetch actors, catalog512, 64 MiB animation budget,
  leg60/replace120. Completed 7 snapshots/3 full batches, zero appearance-load
  failures, peak accounted animation payload 14,224,984 bytes, normal shutdown,
  and no VUID/ASan error markers. Pending work at scripted exit is cleaned up by
  the normal client shutdown path. Debug smoke telemetry was 36.94 FPS, max42.94 ms,
  12 frames over 1000/30 ms; ASan plus validation is not a release-performance run.

Only BasicDrawFlow adopts the new teardown assertion in this PR; other fixtures
retain their existing checks. Whole-suite log inspection is therefore still
necessary. These tests and ASan do not establish absence of every driver/OS leak
or correctness on untested platforms. Cache budgets/admission/quality policy are
unchanged; demand-to-ready work remains in the follow-up plan.

## Reproduce

From a configured developer-shell build:

```powershell
cmake --build --preset dev
ctest --test-dir build/dev -R "BasicDrawTest|DeviceValidationDiagnostics" --repeat until-fail:5 --output-on-failure
.\build\dev\bin\test_graphics_basic_draw.exe --gtest_repeat=5
ctest --test-dir build/dev --output-on-failure
```

The diagnostic-counter unit needs no window or GPU initialization. BasicDrawFlow
needs a desktop Vulkan device and installed validation layer. Run ASan executables
inside the Visual Studio developer environment so its runtime DLL is available.
Run the normal and ASan GPU suites sequentially. The existing StreamingStress
runner can reproduce the native smoke with --frames 240 --leg-frames 60
--replace-frames 120 --catalog 512 --budgets 64 --validation and the appropriate
client executable/asset root. Keep positive logs separate from intentional negative
controls when scanning for VUIDs.
