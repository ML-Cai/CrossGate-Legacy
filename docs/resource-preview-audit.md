# Resource Preview 01：現況盤點與重用決策

對應 [Resource Preview 看板](https://github.com/users/ML-Cai/projects/11) 的 01。
盤點基準是已合併 PR #89 的 `develop`（`3fce584`）。本步驟只交付盤點、
啟動說明與重用決策，不修改解析、播放或 UI 行為，也不宣稱角色網頁預覽已完成。

## 現有工具與可重用部分

| 項目 | 目前能力 | 決策 |
| --- | --- | --- |
| [AnimeViewer](../tools/AnimeViewer/anime_viewer.py) | 固定 Classic／101406，取得動畫描述、讀取影格、依 offset 對齊並匯出第一組動作的 GIF | 保留為舊工具參考；不是網頁 Viewer，不直接當作正確性基準 |
| [GraphicsDataViewer](../tools/GraphicsDataViewer/graphics_data_viewer.py) | Gradio 版本／調色盤選單、單張圖片分頁 Gallery | 沿用 Gradio 與現有啟動經驗；角色 UI 另外完成，不先換 server／前端框架 |
| [Python Session](../tools/python/cgl_pybind.cpp) | C++ reader 的 Python 包裝，查詢動畫描述、圖片、調色盤、轉 NumPy、管理 reader cache | 重用；不在 Python 重寫資源格式解析 |
| [AnimationBuilder](../engines/src/assets/animation_builder.h) | CPU 動畫畫布規劃、影格對齊、上下翻轉、RGBA／透明度、尺寸與 payload 檢查 | 優先重用組裝規則，避免 Python 與原生端各維護一套 |
| [AnimationLibrary](../engines/src/assets/animation_library.h) | GPU 快取、背景準備、上傳與提交後釋放 | 留在原生呈現層；網頁預覽不需要它或 Vulkan handle |

`AnimationBuilder` 本身不呼叫 Vulkan，但輸入仍是引擎內部的
`component::AssetsReaderState`，且只使用 Daytime 環境調色盤；目前沒有 Python
綁定。不能因為已有 CPU builder，就宣稱公開的動畫組裝 API 已完成。
02 應只整理重用它所需的 reader／資料邊界，不建立通用資源管理框架或公開 ECS。

## 已有的 Python 資料邊界

| 介面 | 可用資料／行為 | 尚缺的部分 |
| --- | --- | --- |
| `Session(engine_root_path=..., cache_budget_bytes=...)` | 明確指定素材根目錄；不建立視窗／GPU | 兩個舊 Viewer 仍呼叫無參數 `Session()`，使用編譯時預設設定，沒有根目錄選項 |
| `acquire_available_serial_nums(version)` | **圖片**編號清單 | 不是角色動畫清單；動畫 reader 已有 `queryAvailableSerialNums`，但尚未綁到 Python |
| `acquire_anime_resource(version, serial)` | holder 的 `keys()` 與 `acquire(direction, motion)`；版本、方向、動作、總 duration、依序的圖片編號 | 沒有已對齊的動畫 RGBA 結果或播放介面；可用方向／動作應從該角色的 keys 取得 |
| `acquire_graphics_resource`、`offset()` | 圖片索引與解碼資料由 holder 持有；offset 是索引中的像素偏移 | Python 不直接公開所有索引欄位；圖片尺寸可由解碼陣列 shape 取得 |
| `acquire_env_palette`、`apply_palette` | NumPy `uint8[height,width,4]`，目前為 BGR 通道順序與原生 row 順序，索引 0 的 alpha 為 0；優先採用圖片內嵌 palette | 不能把現有輸出直接當瀏覽器 RGBA；需要保留 alpha、換色序及翻轉 |
| `close`／context manager、`trim_cache`、`invalidate_resources`、`cache_stats` | reader 清理、重新讀取、值快照；返回的 holder／NumPy buffer 獨立持有資料 | cache budget 是每個 reader 的檔案 cache，不包含 UI 持有的影格、NumPy／PIL、索引或整體程序記憶體 |

完整 ownership 與 cache 邊界見 [Session 說明](../tools/python/README.md)。
目前呼叫保留 GIL；未提供非同步工作或跨執行緒 Session 保證。網頁工具應先
序列化同一個 Session 的操作，避免一開始就加入多 worker／跨請求共享可變世界。

方向編號、遊戲方位與螢幕方向不是同一件事。Viewer 應使用素材的
`DirectionTypes`／實際 keys 標示方向，不套用 WASD 或地圖座標的轉換。
`AnimeMotionDesc.duration` 是整組動作的毫秒數，目前組裝使用均分影格時間，
不是每張影格各有獨立時間的 API。

## 已確認的缺口

1. **角色工具沒有互動預覽**：固定角色，遍歷 unordered map 的 keys 後即
   `break`，只匯出第一組；第一組不是穩定的預設動作契約。沒有方向／動作選取、
   播放暫停、逐格、縮放或網頁。
2. **舊工具丟棄 alpha**：兩者都以 `[:, :, [2, 1, 0]]` 轉成 RGB，角色工具又
   貼到不透明黑色畫布。不能照搬這條路徑做透明動畫預覽。GIF 也可能合併相同
   影格、量化時間與顏色，不適合作為新的主要播放資料格式。
3. **圖片分頁混用編號與位置**：`on_page_load` 用最小 serial 當頁索引，
   `on_image_change` 實際把輸入當清單位置；版本切換時 maximum 又是清單長度。
   少於 16 張、空清單或超界可能產生負索引、例外或空頁。這些是程式檢查所得，
   不是這次實跑已逐一覆蓋的測試案例。
4. **palette 沒有獨立更新事件**：只綁定版本和 slider change，單改 palette
   不會立即刷新。新角色 UI 需明確處理選擇變更。
5. **啟動／依賴不完整**：工具只附加 `build/bin`，不會自動找到 preset 的
   `build/dev/bin`；原圖片 README 的 `bin/GraphicsDataViewer` 路徑不正確。
   `tools/python/requirements.txt` 沒列 Pillow，Gradio 未固定版本。下列實跑
   版本是一次可用組合，不代表所有版本相容，也尚未決定新的依賴鎖定方式。
6. **公開 API 仍缺動畫列表與共用組裝**：原始 reader 與原生 CPU 組裝邏輯已在，
   下一步是接出工具需要的值資料，不需加入遊戲 server、GPU API 或另一套 parser。

## 目前怎麼啟動

從 repository 根目錄，在已完成原生依賴設定的 Visual Studio x64 環境中：

```powershell
python -m pip install numpy pillow gradio
cmake --preset dev -DPython3_EXECUTABLE="$(python -c 'import sys; print(sys.executable)')"
cmake --build --preset dev --target cgl_pybind
$env:PYTHONPATH = (Resolve-Path build/dev/bin).Path
python tools/GraphicsDataViewer/graphics_data_viewer.py
# 或舊版 GIF 匯出（檔案寫入目前工作目錄）
python tools/AnimeViewer/anime_viewer.py
```

Python 版本／架構需與編譯的 extension 相符，確認 `cgl.__file__` 指向本次 build。
完整原生環境需求見 [build-and-test](build-and-test.md)。目前兩個入口沒有素材根
目錄參數；無參數 Session 使用設定預設的 `C:/CrossGate-Legacy`，不會自動套用
客戶端 `--settings`。若素材在別處，可先用下列 API probe；不是假裝舊 UI 已支援它：

```python
import cgl

with cgl.Session(engine_root_path="D:/your-assets") as session:
    bundle = session.acquire_anime_resource(cgl.CrossGateVersion.CG_VERSION_Classic, 100000)
    if bundle is not None:
        for direction, motion in bundle.keys():
            desc = bundle.acquire(direction, motion)
            print(direction.name, motion.name, desc.duration, len(desc.motionGraphicsSerialNums))
```

## 01 的實際查核結果

2026-10-27，Windows x64、Python 3.12.10，`cgl_pybind` 與
`test_animation_builder` 由上述 develop 基準確認建置。沒有修改 C++／Python
實作，沒有重跑整套 Vulkan 壓測，也沒有新增鏡射實作的測試。

- `ctest --test-dir build/dev -R "python_session_cache|AnimationBuilderTest" --output-on-failure`：
  **12/12 CTest 通過**，其中 Python entry 包含現有 **8 個 unittest**，另有
  **11 個 AnimationBuilder 案例**。這些覆蓋合成素材 ownership、cache 清理、
  對齊、色序、透明度與失敗路徑；不是網頁動畫驗收。
- 實際 Classic 101406：取得 80 組 keys；本次第一組 NORTH／ATTACK 為
  16 張影格、總 duration 900 ms。第一張 NumPy 為 248×236×4，offset
  (-142,-192)，有 35,392 個透明像素。這只是本機素材樣本，不是跨版本保證。
- 舊 AnimeViewer：成功匯出 347×355 GIF；Pillow 產生 8 個影格，逐格讀取
  duration 合計 **880 ms**，第一格 RGBA **0 個透明像素**。由此確認它不能
  當作保留原始時間／透明度的正確輸出。
- Gradio **6.5.0**、Pillow **12.1.0**、NumPy **2.4.1** 的既有本機環境：
  `GraphicsDataViewerApp` 可建構，`on_page_load()` 回傳 16 張 RGB 圖片。
  綁定 `127.0.0.1:7873`、`share=False` 啟動後，首頁 HTTP **200**；隨後
  關閉 server 與 Session。此查核是 callback／HTTP smoke，不是瀏覽器視覺驗收。
- Probe 先把本次 `build/dev/bin` 放進 import path，GIF／log 放在忽略的
  `build/` 內；沒有修改主工作目錄、提交遊戲素材或產生的 GIF。獨立環境的
  套件下載已取消，以上 UI 結果使用既有環境，未更新其套件。

## 下一步的最小範圍

**02**：補動畫編號列舉，以及可供 Python 重用的 CPU 動畫結果／metadata；
沿用既有 Session ownership 與 builder 規則，處理必要的輸入邊界與資源上限。
先以 Classic 與可選 serial 完成，其他版本依實際素材驗證後再宣告支援。

**03**：用既有 Gradio 基礎接角色／動作／方向與播放控制，明確指定 root；
瀏覽器讀取保留透明與正確時間的影格資料，不以 GIF 匯出代替播放模型。

**04**：依 02／03 的實際資料形狀與 UI 補上角色切換、錯誤、回收和啟動驗收。
本文件先記錄既有查核與缺口；完整驗收方式留待 01 review 後另外補充。
