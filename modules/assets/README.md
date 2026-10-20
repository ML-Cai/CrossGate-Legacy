# Map layout reader

`IMapLayoutReader::create()->read(path, &layout)` reads local MAP `.dat` files
without requiring a window, GPU, settings, or installed game data.

The supported layout consists of a 20-byte header (`MAP`, nine reserved bytes,
little-endian width and height) followed by three row-major uint16 planes:
ground IDs, object IDs, and flags. Reserved bytes are ignored. Trailing bytes,
truncated planes, zero dimensions, dimensions over 65,536, and maps over
4,194,304 cells are rejected before allocation. Failed reads leave output intact.

IDs and flags remain raw. Resource-version ID remapping and interpreting movement
or teleport flags are separate policies; no legacy heuristics are applied here.
CUT files are not supported. The client still starts in AnimationScene.

Run `ctest --test-dir build -R MapLayoutReaderTest --output-on-failure`.
Tests generate their own files and cover rectangular indexing, little-endian
planes, raw values, every truncation point, invalid headers and unsafe sizes.
## Binary reader ownership and lifecycle

Graphics/anime data and index readers expose `close() noexcept`. It is safe to
call repeatedly, and `load()` can be used again afterward. Destruction also
releases owned resources. Calls on one reader (including close) must be serialized;
use separate instances for independent concurrent work.

Data readers retain bounded file-block caches (4096 bytes x 1024 blocks each).
`close()` releases cached blocks, container storage, file handles, and graphics
scratch storage. Index readers close the input file immediately after successful
preloading; `close()` releases indexes and their backing buffer. Anime index
reload replaces old entries. Graphics index load remains idempotent until close.
An invalid load leaves no queryable partial index or open invalid graphics file.

Returned animation vectors and graphics `pData` belong to the caller and survive
reader closure. `pPaletteData` is a borrowed pointer into that same `pData`
allocation; it must not be freed separately or retained after its owner is reset.
Failed animation and graphics queries leave the previous output unchanged.

Animation queries allow at most 65,536 frames per motion and 1,048,576 frames per
query, with file bounds checked before allocation. Unknown direction/motion records
are skipped together with their payload. Graphics queries require positive
sizes and bound encoded and decoded payloads to 256 MiB each; these are defensive
per-query limits, not a global CPU/GPU memory budget. Existing legacy RLE overflow
tolerance is retained; this change does not redefine legacy decoding policy.

Synthetic tests need no installed game data or GPU:

```sh
ctest --test-dir build -R "ResourceReaderTest|FileBlockCacheTest" --output-on-failure
```

They cover repeated close/reload, deletion after close, cache eviction/replacement,
short reads, animation truncation and oversized frame counts, unchanged output on
failure, and embedded palette ownership. These checks verify specific ownership
and failure paths; they do not prove an application-wide absence of leaks.
