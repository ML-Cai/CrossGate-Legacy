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

## File-cache budgets and statistics

Graphics/anime data readers accept `CreateInfo::cacheBudgetBytes` (default 4 MiB).
`setCacheBudget(bytes)` applies immediately, evicting least recently used blocks
when necessary. Payload capacity is rounded down to whole 4096-byte blocks, and a
partial last block still occupies one block. A budget below 4096 bytes, including
zero, performs direct reads into the caller's destination without allocating a
file-cache block. Cache misses evict before allocating a replacement, so there is
no additional transient file-cache block above budget. Blocks have unique ownership;
no reference-counted block handles escape the cache.

`trimCache()` releases all file-cache blocks and container storage, plus graphics
decoding scratch, without closing the file. Data already returned to the caller
remains valid. The next query reloads what it needs. `close()` also trims, and both
operations are idempotent. These operations require the same serialized access as
other reader methods.

`cacheStatistics()` returns a value snapshot:

- `budgetBytes`, `residentBytes`, `cachedBlocks`: current file-cache payload state.
- `peakResidentBytes`: highest successfully cached payload since the last reset;
  it can exceed the current budget after the budget is reduced.
- `hits`, `misses`: individual block lookups; direct reads do not count as lookups.
- `evictions`: blocks removed by replacement, budget reduction, trim, or close.
- `fileReadCalls`, `fileReadBytes`, `failedReads`: C++ stream read attempts, extracted
  bytes (including partial failed reads), and I/O failures. These do not measure
  physical disk traffic because the stream and OS may buffer data. Invalid API
  arguments do not count as I/O failures.
- `bypassReads`: direct read attempts while caching is disabled.
- `scratchBytes`: retained graphics decode scratch capacity, outside the file budget.

Counters and the selected budget survive close/load. `resetCacheStatistics()`
clears counters and sets the peak to current occupancy without discarding data.
These are per-reader CPU payload budgets, not process-memory limits: metadata,
stream buffers, decoded caller-owned data, indexes, graphics scratch, and GPU
resources are separate. A global coordinator and Python/HTTP telemetry bindings
are not introduced here. Native defaults remain unchanged.

## Decode scratch and palette reuse

Uncompressed graphics read directly into the final caller-owned allocation,
including any embedded palette. Compressed graphics reuse scratch only up to
1 MiB. Larger compressed input uses temporary storage released on both success
and failure, and discards any previously retained scratch before allocating.
`scratchBytes` reports retained capacity, not temporary peak memory. Large
compressed input and its decoded output still coexist during decoding; this is
not a streaming decoder or an overall process-memory cap. `trimCache()` and
`close()` release small retained scratch as before.

Embedded palettes must contain at most 256 complete BGR triplets. If fewer than
256 colors are present, all decoded indices must fit that palette before the
resource is returned. This protects native and Python consumers that use those
indices to address palette memory. Invalid resources leave prior output intact.

`IPaletteReader` caches one copied palette per supported environment, keyed by
resolved resource path: four entries, at most 3072 bytes of color payload plus
path/object overhead. Files are closed after loading, results are copied out,
and failed reads are not cached or published. Null output and unsupported palette
types are rejected. Access to the same reader must be serialized.

Call `clearCache()` after editing palettes in place; path changes automatically
replace the corresponding entry on its next read. A Python tool using the current
Session binding must recreate its session to invalidate cached palettes; the new
C++ invalidation method is not yet exposed there. No file watcher is introduced.

Truncated RLE opcode operands fail explicitly. The existing compatibility rule for
oversized output runs remains separate and unchanged.

AnimationLibrary now reuses one character description and decodes frames
sequentially after metadata validation; see engines/README.md for ownership
and budget boundaries.

Remaining work identified during this pass:

- Index preloading still allocates a whole input file plus lookup tables without
  an explicit index-memory budget. Large/corrupt inputs need a separate limit or
  streaming parse policy.
- File-cache budgets do not coordinate decoded CPU data or live GPU resources.
  A shared resource policy needs scene ownership and in-flight GPU lifetime rules.
- Python sessions need explicit invalidation and telemetry methods before adding
  live editing, rather than relying on session recreation.
