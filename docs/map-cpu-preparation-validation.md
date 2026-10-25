# CPU map preparation validation

Base: develop `da10a84` (PR #83). This is the first map-loading boundary change;
scene entry and uploads remain synchronous. Background preparation, cancellation,
per-frame upload scheduling and loading UI are separate follow-ups.

## Data and ownership

`MapBuilder` consumes a parsed `MapLayout` and queries the existing Classic
graphics-info/palette readers. The existing `IMapLayoutReader` remains responsible
for file parsing. No resource parser or coordinate implementation is duplicated.
The builder owns layout cells, copied graphics metadata/palette, atlas plans and
image placement/footprint data. It retains no reader pointer, ECS/world, window,
device or renderer. Source readers must remain valid only during their calls.

`SpriteRegion` is now a standalone pixel rectangle header; including map CPU
data or atlas planning does not require `ISpriteRenderer`. Its namespace, layout
and availability through the existing renderer header remain unchanged.
`MapImage`/Classic artwork filtering are likewise separate from rendering/sorting.

Construction deduplicates IDs and plans all pages before decoding pixels. It
rejects invalid layout dimensions/cell counts, missing resources and unsupported
atlas sizes. Existing shelf order, one-pixel borders, eight-page limit, image
offsets, footprints, reach and payload accounting are preserved. A page is at
most 2048x4096 RGBA8 (32 MiB); total planned payload remains at most 256 MiB.

`buildPage(index, reader, maxBytes)` allocates only the requested page, checking
the caller's RGBA limit before allocation or decoder access. It converts the
existing bottom-up indexed data to owned top-down RGBA, preserving embedded
palette precedence, BGR conversion and index-zero alpha. A failed call returns
no partial pixels and leaves preparation reusable. Returned pages are independent
of reader caches, other page results and the builder's lifetime.

The native path holds one page at a time, uploads it synchronously and releases
its CPU result before producing the next. This preserves the previous one-page
working pattern instead of introducing an all-pages RGBA collection. Individual
decoded source images, reader caches, staging and allocator overhead are extra;
32 MiB is not a total host-memory bound. External callers can deliberately retain
several page results and must budget their own ownership.

`takeData()` consumes metadata by moving cells/image containers, invalidating
further page requests. The builder is movable and noncopyable to avoid accidental
large layout/plan copies. The test checks that layout storage transfers without
copying. Passing an lvalue layout into construction still intentionally copies;
the native path passes an rvalue.

`MapLibrary` retains all GPU ownership and device/pass validation. It publishes
the cache entry only after every page is uploaded. CPU failure on a later page
unwinds the previously created synchronous renderers without publishing a partial
map; retry starts cleanly. Existing keyed retirement, submission fences, cache
restoration and teardown order are unchanged.

Reader calls are externally serialized. The engine holds `queryMutex` during
layout/metadata preparation and each page's decode, releasing it for GPU creation.
All GPU submission remains on the owner thread. This allows existing animation
CPU preparation to access readers during a map upload; it does not make readers,
the builder, or the graphics queue generally thread-safe. Readers still must not
be replaced/destroyed while engine jobs are using them.

## Validation and reproduction

The shared SDK-free registry now adds ten production map-builder tests: six
native suites / 54 native cases, plus two Python runner cases = **56 CTest cases**.
Map-builder tests link GTest/Threads and compile the production CPU source; there
is no asset-reader implementation, renderer backend, Vulkan SDK or game corpus
dependency in this target.

Cases cover metadata ownership, ID filtering/deduplication, flags/footprints,
exact RGBA output, transparent borders, embedded palette precedence, invalid
layouts/metadata, page-count limits, per-page budget admission, out-of-range
pages, decoder result/null/size/exception failures, retry, independent pixel
ownership, move-only metadata transfer and maps with no artwork.

The additional native Vulkan test injects a real decoder failure for an image
planned on page two of map 100. Page one has already uploaded. It checks no
partial cache entry/payload, restores the decoder, reloads successfully and
releases the completed map. The fixture checks validation through device cleanup.
Existing map 100/2100 startup, rendering, occlusion, cache retirement and lifecycle
tests remain enabled.

```powershell
# Use an x64 Visual Studio developer shell on Windows.
cmake -S engines/tests/headless -B build/headless -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/headless --parallel 2
ctest --test-dir build/headless -V --no-tests=error

cmake -S engines/tests/headless -B build/headless-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCGL_ENABLE_ASAN=ON
cmake --build build/headless-asan --parallel 2
ctest --test-dir build/headless-asan -V --no-tests=error

# Desktop/corpus tests require the installed assets configured in settings.ini.
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev -V --no-tests=error
cmake --preset asan
cmake --build --preset asan
ctest --test-dir build/asan -V --no-tests=error

python tools/LifecycleSoak/run_soak.py --test-exe build/dev/bin/test_scene_lifecycle.exe --cycles 100 --warmup-cycles 20 --require-memory --output build/map-cpu-soak
```

Desktop validation runs on the development Radeon 880M integrated GPU. It does
not establish physical GPU-memory bounds on dedicated 4/8 GiB devices. MSVC ASan
does not provide LeakSanitizer coverage; the hosted Linux core lane enables leak
detection for the CPU-only cases. This refactor makes no measured FPS or loading
latency improvement claim.

## Local results

Measured 2026-10-25. Full ordinary CTest passes **302/302** and full MSVC ASan
passes **301/301**, including the late map-page failure/reload test. The ordinary
Python binding test accounts for the one-case difference. Separate SDK-free ordinary and MSVC ASan projects
each pass **56/56**. These standalone builds use the shared production source and
test registration, rather than linking the desktop engine.

Map startup descriptors match the previous PR #83 regression logs on the same
installed corpus:

| Map | Dimensions | Distinct images | Atlas pages | RGBA payload bytes |
| --- | --- | ---: | ---: | ---: |
| 100.dat | 840x610 | 953 | 2 | 40,330,752 |
| 2100.dat | 100x100 | 291 | 1 | 16,638,072 |

The independent **100/100-cycle** ordinary lifecycle run alternates these maps,
renders/changes remote actors, exercises CPU/submitted-upload cancellation and
checks zero Sprite/remote entities and zero map/animation cache/retired/pending
payload at every quiescent checkpoint. Validation is enabled through teardown.
After 20 warmup cycles, first/last ten-sample median RSS is
434,888,704 -> 435,007,488 bytes, a difference of **118,784 bytes (0.113 MiB)**.
Peak sampled RSS is 488,427,520 bytes. This observed near-flat trend and ownership
assertions are evidence for this workload, not a proof of all leak freedom or a
hard process/device-memory cap.

Ignored evidence: `build/map-cpu-tests.log`, `build/map-cpu-asan-tests.log`,
`build/map-cpu-final-headless-tests.log`, `build/map-cpu-headless-asan-tests.log`
and `build/map-cpu-soak/summary.json` plus its named native log. Complete positive
logs are checked for VUID, Vulkan validation and sanitizer error markers.
No executable, installed game resource or raw log is committed.
