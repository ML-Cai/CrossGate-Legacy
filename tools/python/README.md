# Python resource sessions

Current character preview: [streaming plans, PUK versions and synchronized directions](../../docs/resource-preview-directions.md).
`plan_animation(version, serial, direction, motion)` returns an owned `AnimationPlan`
with a copied metadata dict; `decode_animation_frame(plan, index, max_bytes=4 MiB)`
returns independently owned top-down RGBA for one frame. The plan holds no Session
or readers. Decoding requires its original open Session and resource epoch;
`invalidate_resources` and `close` expire plans, while prior metadata/pixels survive.
`trim_cache` does not expire plans. Access retains the GIL and must be serialized.
The full-clip API below remains supported; its original validation notes are historical.

Native/Python palette-reader parity and SDK-free regression setup are documented in
[animation palette validation](../../docs/resource-preview-native-palette.md).
The CPU test project now also runs the native AnimationBuilder fixtures; initialize
the pinned dependency with `git submodule update --init third_party/googletest` before configuring it.

Read-only MAP snapshots: see [map preview API](../../docs/map-preview-api.md)
for `Session.load_map`, bounded region copies, ownership and CPU-only tests.
The [map web viewer](../MapViewer/README.md) additionally wraps shared CPU map
planning, atlas pages, visible draw ordering and pan conversion without Vulkan.

The CPU-only `cgl.Session` binding supports native resource inspection without a
window or GPU. Existing `Session()` calls still load settings as before. Explicit
roots make tools/tests independent of the process settings file:

```python
import cgl

with cgl.Session(engine_root_path="C:/CrossGate-Legacy",
                 cache_budget_bytes=4 * 1024 * 1024) as session:
    frame = session.acquire_graphics_resource(cgl.CG_VERSION_Classic, 100)
    palette = session.acquire_env_palette(cgl.Daytime)
    if frame is not None and palette is not None:
        image = session.apply_palette(frame, palette)
    snapshot = session.cache_stats()  # JSON-compatible value copy
    session.trim_cache()
# Previously returned holders and NumPy images remain valid after close.
```

## Cache and lifetime controls

| Method | Effect |
| --- | --- |
| `cache_stats()` | Snapshot of current data readers and counters; does not load resources |
| `set_cache_budget(bytes)` | File-cache budget for each active and future data reader; zero bypasses caching |
| `reset_cache_stats()` | Reset current readers' counters; preserve occupancy and budgets |
| `trim_cache()` | Release file-cache blocks/decode scratch and invalidate palettes; retain indexes and open data readers |
| `clear_palette_cache()` | Invalidate palette values only |
| `invalidate_resources()` | Destroy all readers/indexes/palette cache and map storage; next acquisition reloads lazily |
| `close()` | Idempotent permanent close; release readers/settings and reject further acquisitions |

For in-place edits of graphics, anime data or indexes, invalidate resources before
reading again. Palette-only edits can use clear_palette_cache. Invalidation does
not modify previously returned data. A failed reader load is never published into
the session, so a repaired file can retry without recreating the Session.
Other successfully loaded readers may remain cached after a later request fails;
use invalidate_resources to discard those as well.

`close`, invalidation, trim, palette clear, counter reset and observation are safe
to repeat after closing. Acquisitions, budget changes and context entry then raise
RuntimeError. apply_palette is a pure conversion of owned holders and remains
usable after close; null holders raise ValueError. NumPy arrays own their converted
buffer through a capsule, independently of readers/holders. Their shape follows
actual decoded dimensions even if index dimensions disagree. Conversion retains
the existing BGR channel and native-row convention used by current tools.

Calls retain the Python GIL. Serialize calls per Session when designing a server;
no free-threaded/subinterpreter guarantee, background loader or HTTP server is
introduced. No raw pointers/ECS/GPU handles are exposed by the new API.

## Statistics boundaries

`readers` contains deterministic graphics/anime entries with version names,
budget/current/peak file-cache payload, block count, retained scratch, hits,
misses, evictions, stream reads/bytes/failures and bypass reads. `totals` sums
current payload, retained scratch, active-reader budgets and current-reader I/O.
Historical peaks are per reader and are not added into a misleading simultaneous
peak. Snapshots can be mutated or JSON-serialized without affecting native state.

Counters last until reset or reader destruction. Invalidation/close discard them;
the configured per-reader budget survives invalidation. Index-reader counts and
palette-reader existence are reported, but their exact native heap sizes are not.
Neither caller-owned decoded/NumPy data, allocator/container overhead, settings,
Python objects nor GPU allocations are included in cache payload statistics.
Stream counters are not physical disk traffic. This API is not a process memory
cap or a global budget coordinator.

## Build, tests and baseline measurement

Build with CGL_WITH_TOOLS=ON and CGL_WITH_TEST=ON, using a Python development
installation and NumPy (`python -m pip install numpy`). CMake's Python3_EXECUTABLE
must name that same interpreter. Build the cgl_pybind target, then run:

```sh
ctest --test-dir build -R python_session_cache --output-on-failure
```

Thirteen synthetic unittest cases need no game installation or GPU. They cover
snapshots, hot reads, current/future budgets, integer-safe budget totals,
invalidation/reload, repaired failed loads, context cleanup, mismatched dimensions,
NumPy view ownership and 100 load/invalidate cycles. Allocator failure is not
fault-injected; capsule exception ownership is protected by RAII.

For a reproducible real-resource baseline, optionally install psutil to sample
process RSS (`python -m pip install psutil`), then:

```sh
python tools/python/benchmark_session.py --module-dir build/bin --root C:/CrossGate-Legacy --iterations 20 --output baseline.json
```

The script initializes NumPy before taking the empty-session baseline.
It measures Classic animation-description queries and one referenced frame's
conversion through cold/hot/trim/invalidate/reload/close phases. Repeated cycles
release returned data and readers. It does not run a map scene, assemble all
animation frames or allocate GPU resources. Without psutil RSS fields are null.
Use an explicit output path because existing native trace logs go to stdout.
RSS includes interpreter/NumPy/allocator retention and need not return to startup;
finite cycles or a plateau do not prove absence of leaks. Record the environment,
corpus and iteration count when comparing runs. On Windows an ASan-instrumented
PYD also needs the matching ASan runtime DLL beside it (or in Python's DLL search
paths); the interpreter and NumPy themselves are not instrumented by this build.

## CPU animation preview API (Resource Preview 02)

```python
import cgl

with cgl.Session(engine_root_path="C:/CrossGate-Legacy") as session:
    version = cgl.CrossGateVersion.CG_VERSION_Classic
    serials = session.acquire_available_anime_serial_nums(version)  # sorted, unique
    description = session.acquire_anime_resource(version, 100000)
    direction, motion = sorted(description.keys(), key=lambda key: (int(key[0]), int(key[1])))[0]
    clip = session.acquire_animation_pixels(version, 100000, direction, motion,
                                            max_bytes=64 * 1024 * 1024)
    frames = clip["rgba"]  # uint8[frame_count, height, width, 4], top-down RGBA
# frames and views remain usable after Session close.
```

Unlike legacy `apply_palette`, this result already has browser-compatible RGBA
channel/row order and an aligned transparent canvas. Do not flip or swap it again.
The dict contains `width`, `height`, `frame_count`, `duration_ms`, `offset_x`,
`offset_y` and `rgba`. Offsets locate the canvas top-left relative to the character
anchor, in unscaled pixels. `duration_ms` is the exact total motion duration;
playback should derive frame indices from elapsed time rather than round each
frame to integer milliseconds or GIF delays. The existing format/model divides
that duration uniformly among frames. Browser playback is Project 03, not part
of this API.

Enumeration lists animation-index entries, not graphics IDs or a guarantee that
all referenced pixels are valid. Existing `description.keys()` supplies available
direction/motion pairs (unordered); select/sort these, not every enum combination.
The first version verified with installed resources is Classic. Runtime results
for other resource versions remain subject to their reader support and real data.
Environment palette is Daytime, with embedded frame palettes taking precedence,
as in the native decoder. Palette selection is not added in this step.

`max_bytes` limits one aligned output payload (default 64 MiB, allowed 1..256 MiB),
checked from metadata before frame pixel decoding. It is not a whole-Session or
process cap: readers/indexes/metadata and caller-retained outputs are additional.
Opening a reader can read file headers before admission. Invalid bounds/enums
raise ValueError (out-of-range Python integers may raise TypeError/OverflowError);
missing/malformed animation or budget rejection raises RuntimeError. Reader-open
failures retain the existing ValueError behavior. Acquiring after close raises
RuntimeError. Failed calls return no partial clip and preserve earlier outputs.

The native core `AnimationDecoder` accepts a non-owning per-version reader bundle;
it has no ECS/window/GPU dependency. The engine's existing `AnimationBuilder`
adapter retains its shared reader mutex. Session retains the GIL and one cached
animation description, serializes access as before, and clears that description
before reader invalidation, close or trim. Returned pixels use a NumPy capsule
owning the moved vector; the Session does not cache returned clips. Metadata and
Python-owned pixels are not added to `cache_stats()`'s file-cache byte counters.

For this step's checks and reproducible examples, see
[Resource Preview 02 validation](../../docs/resource-preview-animation-api.md).
