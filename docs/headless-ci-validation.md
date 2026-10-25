# SDK-free core CI validation

Base: merged PR #81 (`35b1091`). The standalone project at
`engines/tests/headless` builds existing production code and tests without entering
the desktop CMake dependency graph. Only the pinned googletest submodule and a
Python interpreter are needed; it never initializes GLFW, Vulkan or installed
game resources, nor does it require glslc or Python development headers.

## Shared boundaries

Five native suites (44 discovered cases) cover idle-cache/retirement ownership,
background metadata/decode admission and cancellation, partial scene-load
ownership, appearance priority and demand timing. Two Python suites exercise the
streaming and lifecycle runner protocols. All **46 CTest cases** are actual tests,
not placeholders or skipped GPU tests.

The desktop and standalone projects use `cmake/HeadlessCoreTests.cmake` to define
the same native targets from the same source files. No duplicate implementation
or copied tests are introduced. The scene-load test still uses the renderer's
pure interface type via `AnimationClip`; it does not link a renderer backend.
This is not a complete GPU-free build of all engine/resource modules.

Sanitizer flags move unchanged into `cmake/AddressSanitizer.cmake`, shared by the
root and standalone projects. Googletest is compiled from its pinned source in
the standalone project, with matching instrumentation. The ordinary installed
Googletest dependency used by the desktop build is unchanged.

## Workflow

`headless-core.yml` runs four hosted jobs: Windows 2022 / Ubuntu 24.04, each with
ASan OFF / ON. It uses only the googletest submodule (no GLFW/spdlog setup), a
pinned Python action/interpreter and SHA-pinned official checkout/artifact actions.
PRs targeting develop and pushes to develop trigger it; manual dispatch is also
defined. Jobs have timeouts, no fail-fast masking of other configurations and
read-only repository permissions. No custom credential, asset corpus or trusted
self-hosted runner is required.

Configure/build failures propagate nonzero status. CTest runs with
`--no-tests=error`, verbose logs and JUnit output. An always-run artifact step
retains configure/build/test logs, JUnit and CMake/CTest diagnostics for seven
days. Linux ASan requests leak detection; MSVC ASan is not LeakSanitizer.
Sanitizers do not prove absence of all leaks or races.

## Local verification

See [build and test](build-and-test.md#sdk-free-core-ci) for commands. On the
Windows development host, independent ordinary and ASan builds each pass
**46/46**, using only the test submodule and Python interpreter. The ordinary
full desktop suite passes **287/287** after migrating the target definitions.
Hosted results and final desktop ASan verification are recorded below.

No renderer/resource policy or game behavior is changed. Existing GPU/corpus,
Python binding and repeated lifecycle tests still require their normal local
validation lane. Linux success here establishes portability of this selected
subset, not an Android/Linux client port or browser client.
