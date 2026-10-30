# 角色動畫資源預覽

Python 啟動本機網頁，透過共用 C++ API 預覽 Classic 與 PUK 角色素材。
中央顯示選定方向，下方列出八方向縮圖，缺少方向／錯誤會明確標示。
所有方向同步播放、暫停、逐格與 seek；切換大圖共用 bitmap，不刷新頁面。
**唯讀工具，沒有角色編輯、原格式寫回、遊戲 server 或地圖功能。**

## 啟動

Python 3.12／架構必須與編譯好的 extension 相符。Windows 需先關閉使用舊 `cgl.pyd`
的工具，避免鎖住 extension。從 repository 根目錄，在完成依賴與原生設定後執行：

```powershell
python -m pip install -r tools/AnimeViewer/requirements.txt
cmake --build --preset dev --target cgl_pybind
python tools/AnimeViewer/web_viewer.py --root C:/CrossGate-Legacy --serial 100000 --port 7874
```

開啟 <http://127.0.0.1:7874/>。不需要 Gradio、Vulkan、遊戲 server 或 Node.js；Node
僅用於測試。非預設 extension 位置用 `--module-dir <directory>`。
也可用[無桌面 SDK 的 binding 建置](../../docs/map-preview-api.md#驗證)所得模組。

```powershell
python tools/AnimeViewer/web_viewer.py --root C:/CrossGate-Legacy --version PUK1 --port 7874
```

`--version` 可為 Classic、PUK1、PUK2_V1、PUK2_V2、PUK3_V1、PUK4_V1、PUK4_V2、
PUK4_V3；啟動後亦可由下拉選擇。根目錄需有對應的 `bin` 素材。沒有 `--serial`
就選該版本的 100000（若存在）或第一個 ID；缺檔／不支援的素材會明確報錯。
PUK2_V2／PUK3_V1 的動畫 palette 還需要 PUK2_V1 圖資。
Ctrl+C 停止服務，等待 worker 結束再清空資料與關閉 Session。

## 操作與資源

- 選版本、角色、動作；點縮圖切換中央大圖，左右鍵可在可用方向間移動。
- 播放全部方向共用 normalized phase；同步週期以第一個有效方向的原始 duration
  為準。各方向原始時長另列，時長不同時採等比例同步循環。
- 背景頁暫停，錯誤方向不會自動無限重試；可改選或按重新載入恢復。
- 後端只持有最多八個 metadata plans，按需解碼一格；不把八段 RGBA 一起載入。
  每格最多 4 MiB，每方向最多 256 frames，native logical clip 上限仍為 256 MiB。
- 全方向合計 PNG cache 最多 24 張／8 MiB，browser bitmap cache 最多 32 張／
  32 MiB；替換解碼可短暫多一格，canvas、codec、reader 等另計。
- 一次一個 frame fetch，一個播放時鐘；大圖／對應縮圖共用同一 bitmap。
  淘汰、切換、離頁都 close bitmap。新版本載入前先釋放舊 plans／readers。
- `--max-mib` 保留舊整段 API 的 1..32 MiB 預算；streaming frame 額外受 4 MiB
  限制。增加它不會繞過每格上限，不把這些 payload cap 當程序或 VRAM cap。

未增加 palette 編輯／選擇；使用共用 decoder 的內嵌、動畫專用 palette 與 Daytime。
未知擴充 flags 保留為 metadata，不宣稱完整遊戲事件、鏡像或倒播效果已還原。

## 驗收

```powershell
python tools/AnimeViewer/verify_directions.py --root C:/CrossGate-Legacy --cycles 128 --report build/direction-matrix.json
```

此指令不啟動瀏覽器或 Vulkan。逐版查詢代表素材、逐方向／逐格比對 HTTP PNG、
反覆切換、錯誤恢復，最後檢查 owner 與 reader 釋放；失敗不靜默略過。
選用 RSS：安裝 `psutil` 再加 `--require-memory`。詳細契約、限制、本機結果及尚待
人工驗收見 [版本與同步方向驗證](../../docs/resource-preview-directions.md)。

```powershell
ctest --test-dir build/dev -R "python_session_cache|python_animation_stream|AnimationBuilderTest" --output-on-failure
python tools/AnimeViewer/tests/test_preview.py
python tools/AnimeViewer/tests/test_direction_preview.py
python tools/AnimeViewer/tests/test_web_viewer.py
python tools/AnimeViewer/tests/test_verify_preview.py
python tools/AnimeViewer/tests/test_verify_directions.py
node tools/AnimeViewer/tests/test_playback.cjs
node tools/AnimeViewer/tests/test_viewer.cjs
```

CI 用合成資料，不需要商業素材／GPU。DOM／bitmap 模擬不等於實際瀏覽器驗收。
`verify_preview.py` 與 [04 原驗收](../../docs/resource-preview-acceptance.md)保留作舊
整段 API 檢查；新版網頁改用 `verify_directions.py`。原先 101406 STANDARD 超過
整段 8 MiB 的案例，現在可按單格預覽，不提高整段預算。

## 舊 GIF 工具

`anime_viewer.py` 仍是固定 Classic 101406、只匯出第一組動作的歷史工具，使用編譯
時預設根目錄。它會丟失 alpha／時間精度，不能作為新預覽器正確性基準：

```powershell
$env:PYTHONPATH = (Resolve-Path build/dev/bin).Path
python tools/AnimeViewer/anime_viewer.py
```

盤點與沿革見 [Resource Preview 01](../../docs/resource-preview-audit.md)。
