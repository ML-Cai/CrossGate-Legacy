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
  used by `tools/AnimeViewer`: serial `101406`, north-facing attack.

  ```ini
  [Animation]
  SerialNum=101406
  Direction=0
  Motion=5

  [Display]
  ViewportScale=1.0
  ```

  Directions are `0` north, clockwise through `7` north-west. Common motions are
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
  Empty `FilePath` keeps the animation preview. Arrow keys pan the map, and `R`
  restarts it at the center. `Display.ViewportScale` controls the initial map scale
  (0.125 through 16). This stage renders ground only; scene objects and the hover
  diamond follow in separate PRs. Classic ground IDs are used directly; other
  resource versions and CUT maps are not yet supported.
- GraphicsDataViewer : see [tools/GraphicsDataViewer](tools/GraphicsDataViewer)
- AnimeViewer : see [tools/AnimeViewer](tools/GraphicsDataViewer)
