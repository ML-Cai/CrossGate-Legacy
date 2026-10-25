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
Full desktop MSVC ASan also passes **286/286**. Hosted results follow below.

No renderer/resource policy or game behavior is changed. Existing GPU/corpus,
Python binding and repeated lifecycle tests still require their normal local
validation lane. Linux success here establishes portability of this selected
subset, not an Android/Linux client port or browser client.


## Hosted evidence

[Hosted run 40184991978](https://github.com/ML-Cai/CrossGate-Legacy/actions/runs/40184991978)
validates code commit `3b4cb5675aa892cfa02a48aaecb6569dc1824780`:

| Runner | ASan | CTest passed | Skipped | Uploaded diagnostics |
| --- | --- | ---: | ---: | --- |
| Windows 2022 | OFF | 46/46 | 0 | Verified |
| Windows 2022 | ON | 46/46 | 0 | Verified |
| Ubuntu 24.04 | OFF | 46/46 | 0 | Verified |
| Ubuntu 24.04 | ON | 46/46 | 0 | Verified |

All four jobs conclude success. All four artifact archives were retrieved and
inspected: their JUnit reports each contain 46 tests, zero failures and zero
skips, with configure/build/test logs retained. The Linux jobs request
`detect_leaks=1:halt_on_error=1`; this does not replace race detection or GPU
resource validation. No application assets or desktop dependencies were installed
by these jobs.

Local full desktop timings: ordinary 287/287 in 64.15 s; MSVC ASan 286/286 in
269.03 s. Standalone MSVC ordinary 46/46 in 2.25 s; ASan 46/46 in 3.73 s.
These are test-suite durations, not game performance comparisons.

Ignored local evidence: `build/headless{,-asan}-*.log`,
`build/core-ci-{dev,asan}-tests.log`, `build/headless{,-asan}/results.xml` and
`build/ci-hosted/*.zip`. CI logs/artifacts live on the linked run for their
configured retention period; the committed table records the observed outcomes.
The PR's current checks remain authoritative for later commits/runs.
