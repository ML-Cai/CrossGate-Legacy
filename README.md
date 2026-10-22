# CrossGate-Legacy

The project is inspired by the page : https://cgsword.com/filesystem_graphicmap.htm which is a great resource for learning how to decode the data of Cross Gate an online game that has been popular since 2000.

This project is not a private server or an attempt to infringe upon the rights of SOFTSTAR ENTERTAINMENT INC. (https://cg.softstar.com.tw/). It is simply a fan-made implementation for non-commercial use.


## Prerequisites

- Install and play Cross-Gate : https://cg.softstar.com.tw/

    > To test the CrossGate-Legacy project, please first enjoy the original Cross Gate game by installing and playing it from https://cg.softstar.com.tw/. As you explore the CG world and visit different maps, you will collect the map data that CrossGate-Legacy relies on.

- Python 3.10 or higher
- Python Development Headers
- Visual C++ Redistributable for Windows build
- miniforge


## Build

### 1. Clone the repository

```bash
git clone https://github.com/ML-Cai/CrossGate-Legacy.git
cd CrossGate-Legacy
```

### 2. Install requirements

```bash
# Open `x64 Native Tools Command Prompt for VS2022` as command prompt in Windows build
python -m venv .venv
.venv\Scripts\activate
python setup_requirements.py --build_type RelWithDebInfo
```

### 3. Building CGL

```bash
# Open `x64 Native Tools Command Prompt for VS2022` as command prompt in Windows build
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --config RelWithDebInfo
```

## Run

- cgl-client: run `build/bin/cgl-client.exe`. After loading resources, the client
  switches from its loading scene to two Classic characters in an 800x600 Vulkan
  window. They share one clip and play half a cycle apart. Press `R` to restart
  the animation scene; close the window to exit.
  It reads `settings.ini` beside the executable; set `General.EngineRootPath` to
  the game directory containing `bin/`. Relative roots resolve against the INI
  directory. Use `--settings path/to/settings.ini` to choose another configuration,
  or `--frames 240` for an automatically terminating startup check.
  With tests enabled, `ctest --test-dir build -R cgl_client --output-on-failure`
  verifies startup and missing-resource cleanup. These tests require a Vulkan-capable
  desktop; the successful startup test also requires the configured game files.
  The preview uses the daytime palette, original frame offsets, transparent
  palette index 0, and the clip's total duration. The default matches the character
  used by `tools/AnimeViewer`: serial `101406`, screen northwest-facing attack.

  ```ini
  [Animation]
  SerialNum=101406
  Direction=0
  Motion=5

  [Display]
  ViewportScale=1.0
  ```

  Directions are native animation indices: `0` screen northwest, `1` up,
  `2` northeast, `3` right, `4` southeast, `5` down, `6` southwest, `7` left. Common motions are
  `0` idle, `1` walk and `5` attack; available combinations depend on the character.
  `ViewportScale` must be positive. The engine retains the frame renderer across
  scene changes. AnimationLibrary retains clips across restarts; scene exit removes
  character entities, and engine shutdown releases the cache after GPU completion.
  See [scene lifecycle](engines/README.md) for ownership and tests.

  To view Classic map ground instead, add this to your settings file:

  ```ini
  [Map]
  FilePath=map/0/2100.dat
  ```

  The path resolves against `General.EngineRootPath`; absolute paths also work.
  Empty `FilePath` keeps the animation preview. The camera follows the character; `R`
  restarts it at the center. `Display.ViewportScale` controls the initial map scale
  (0.125 through 16). Ground, buildings and trees share image atlas pages; the
  configured animation character sorts with objects by ground position. Multi-cell
  buildings also use native tileX/tileY footprints, so standing outside a front
  wall can draw the character in front even above the building's placement anchor.
  WASD uses screen directions: W up, A left, S down, D right. W+A goes northwest,
  W+D northeast, S+D southeast and S+A southwest. Each input selects an integer
  neighboring MAP cell after inverse isometric direction conversion. Speed is 4 MAP coordinate units/second, so a
  one-axis step takes 0.25 seconds and a two-axis step takes sqrt(2)/4 seconds.
  Walking interpolates smoothly; releasing finishes the current step before idle,
  and turning takes effect at the next cell center. The character stays centered
  while terrain and objects scroll opposite to travel. Arrow keys do not pan.
  Spawn/restart uses integer cell (width/2, height/2), rounded down. A step outside
  map bounds is rejected entirely; object/terrain collision is ignored.
  Map mode uses all eight idle/walk directions of `Animation.SerialNum`;
  `Direction` sets its initial facing and `Motion` only applies to the standalone
  animation preview. Characters without these idle/walk clips fail at scene loading.
  Clips load once before play and remain cached across R restarts. Large characters
  (including default 101406) use substantially more texture memory than human 100000.
  R resets both camera and character. Classic placeholder/debug objects are skipped;
  other resource versions and CUT maps are not yet supported. Hover over ground
  to see a yellow diamond outline. It follows the camera and configured scale,
  and hides outside the map, over empty ground, or when the window loses focus.
  This is a hover indicator; clicking does not move the character.
  Large Classic maps such as `map/0/100.dat` use multiple atlas pages. Each page
  is at most 2048x4096 (32 MiB RGBA), with up to 8 pages per map; renderer/staging
  overhead is additional. Layout data and all unique images load at scene entry;
  each frame visits only camera-visible bounds expanded for large sprites.
  This is not chunk streaming, and map uploads remain cached until shutdown.
- GraphicsDataViewer : see [tools/GraphicsDataViewer](tools/GraphicsDataViewer)
- AnimeViewer : see [tools/AnimeViewer](tools/GraphicsDataViewer)


### Map 100: playable local server snapshot demo

Build `cgl-client`, then run from its binary directory:

```powershell
.\cgl-client.exe --settings .\server-demo100.ini
```

Set `General.EngineRootPath` in the INI to your installed game assets. The demo
loads `map/0/100.dat`, uses a 1280x720 window, and disables VSync and validation
for interactive performance testing. Enable `Display.Validation` for diagnostics.

- WASD moves the player with the camera following its ground anchor; diagonals
  are supported. No collision is applied.
- N replaces the simulated server roster with a fresh random batch; R restarts
  the scene. Close the window to exit.
- Each complete snapshot contains 128 remote actors whose ground anchors are
  in the viewport, plus 160 in the surrounding interest band. The interest
  rectangle has 1.5 times the viewport width and height (2.25 times its area).
  The local player is additional. Small maps clamp counts to at most 10 per cell.
- Snapshots refresh every two seconds, or earlier after crossing half the
  prefetch margin. Existing IDs and world positions inside the interest region
  survive updates; moving the camera can change the visible count between packets.
- The title shows wall-clock FPS and ready/total visible and prefetch actors.
  Initial placeholders share the player's clip, then change to the requested
  appearance when its fenced upload finishes. Map occlusion still applies.

This is a local server simulator, with no network dependency. Its CPU-only
`LocalCharacterFeed` produces complete snapshots; `RemoteCharacters` applies IDs,
positions, appearance changes and removals to ECS. Appearance loading prioritizes
visible actors before the outer band, with one background decode/upload in flight
and at most one upload poll per update. Removing an actor does not cancel an
in-flight upload in the frame loop. Scene exit cancels pending work, and the
engine retains ready GPU clips until safe teardown.

The demo chooses distinct appearances per snapshot when the installed catalog
contains enough choices; otherwise skins can repeat. It uses a metadata-only catalog
of up to 512 characters. Its cache is bounded by this finite catalog and a 512 MiB
animation texture payload budget, separate from map textures and driver overhead.
It is not a production eviction policy for infinitely many server-provided skins.
Invalid appearance loads are reported and remain placeholders. The geometry test
counts ground anchors, not unobscured pixels or sprites extending across an edge.
