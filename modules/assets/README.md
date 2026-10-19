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
