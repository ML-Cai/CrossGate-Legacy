# 地圖預覽第一步：唯讀 CPU 地圖快照

本步驟讓 Python 重用原生 `IMapLayoutReader`，不建立視窗或 Vulkan。
**尚未提供網頁地圖繪製、平移／縮放、選格、編輯或寫回。**

## 使用

以相符 Python 版本／架構建置 `cgl_pybind` 後：

```powershell
cmake --build --preset dev --target cgl_pybind
$env:PYTHONPATH = (Resolve-Path build/dev/bin).Path
python
```

```python
import cgl

with cgl.Session(engine_root_path="C:/CrossGate-Legacy") as session:
    world = session.load_map("map/0/100.dat")
    print(world.width, world.height)
    print(world.cell(0, 0))
    cells = world.region(0, 0, 128, 128)

print(world.cell(0, 0))  # Snapshot survives Session.close().
print(cells.shape, cells.dtype)  # (128, 128, 3), uint16
```

`file_path` 為本機路徑：相對路徑從 Session 的 `engine_root_path` 解析，也接受
絕對路徑。這不是 HTTP 介面或 root sandbox；未來路由不能把不受信任的 URL
參數直接當任意檔案路徑。

## 資料與所有權

| API | 契約 |
| --- | --- |
| `Session.load_map(file_path)` | 每次讀一份獨立 `MapSnapshot`，沒有隱含地圖 cache；讀完即關閉檔案 |
| `snapshot.width`、`height` | 唯讀 MAP 格數，不是螢幕像素 |
| `snapshot.cell(x, y)` | 新 dict：`ground_id`、`object_id`、`flags`，皆保留原始 uint16 值 |
| `snapshot.region(x, y, width, height)` | 獨立 NumPy uint16 複本，shape `[height,width,3]`，channels 依序 ground_id、object_id、flags |

座標為 MAP 格子，左上 `(0,0)`，row-major：`array[y,x]`；不是螢幕方向、遊戲方位
或素材朝向。此 PR 不新增投影換算。原始地圖 ID 不保證等同圖片 serial，後續
呈現仍需沿用原生圖資映射與 offset 規則；此層不解釋或清除 flags 高位元。

快照持有 C++ cell vector，不保留 Session／reader／ECS／GPU 指標。
Session `trim_cache()`、`invalidate_resources()`、`close()` 不使快照失效。
region 可修改且獨立於快照；刪除快照後陣列仍有效。兩者由 Python ownership
釋放，不另加 handle／close 協定。重讀會看見檔案變更，舊快照保持原值。

沿用 reader 的 4,194,304 格上限，單份 cell payload 最多約 24 MiB；region
一次最多 65,536 格，即 384 KiB。零／負尺寸或越界為 `IndexError`，超過 region
格數預算為 `ValueError`，不默默截斷。空路徑、缺檔、損壞或超過 reader 上限
為 `ValueError`；分配失敗為 `MemoryError`；已關閉 Session 再 load 為 `RuntimeError`。

Session 不保留地圖，`cache_stats()` 的 file-cache counters 不包含呼叫端持有的
快照／region；多份結果仍會累積記憶體。後續預覽層應保留目前地圖及可視區域，
不是預載全部地圖。API 目前保留 GIL，不承諾非同步或跨執行緒 Session 安全。

## 驗證

```powershell
ctest --test-dir build/dev -R "python_map_snapshot|python_session_cache|MapLayoutReaderTest" --output-on-failure
```

7 個新增 binding 測試涵蓋三個 plane／高位 flags、非正方形與 row-major、複本
隔離、Session 關閉／檔案刪除後存活、重讀、極端座標、region 上限邊界、
損壞／超大檔案與恢復、100 次 load／region 的 owner 釋放。原 MAP reader 的
截斷等測試繼續沿用。

不需桌面 SDK 的 production binding 測試入口：

```powershell
cmake -S tools/python/tests/headless -B build/python-headless -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/python-headless --parallel 2
ctest --test-dir build/python-headless -V --output-on-failure
```

需 Python development files、NumPy、C++20 compiler，以及首次取得 pybind11
v3.0.0 的網路。Windows 在 Visual Studio x64 開發環境執行；可用
`-DPython3_EXECUTABLE=<python>` 指定 interpreter。Windows／Linux CI 建置真實
binding 並執行 Session／map 測試，不需要 Vulkan 或遊戲素材；Linux extension
使用 `.so`、Windows 使用 `.pyd`，不代表原生客戶端已完成 Linux 移植。

2026-10-29 本機結果：普通與 Windows ASan 的 binding 測試通過，SDK-free binding
建置／測試通過。實際 `100.dat` 為 840×610，單次 CPU 讀取約 142 ms，128×128
region 可讀取；Session 關閉後快照仍可用，刪除快照後 owner 釋放。單次時間不是
效能保證。沒有進行瀏覽器地圖畫面或原生渲染對照驗收。
