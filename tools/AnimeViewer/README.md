# 角色動畫網頁預覽

Python 啟動本機 HTTP 服務，瀏覽器預覽 Classic 角色動畫。使用既有 C++
`Session` API，支援角色、動作與素材方向選擇、播放／暫停、逐格、影格滑桿與縮放。
頁面保留不重建，切換不需要重新整理；載入後預設暫停，只取第一格。

## 啟動

使用 Python 3.12，先建置包含 Resource Preview 02 API 的 `cgl_pybind`
（PR #91 或之後），並使用與 extension 相同的 interpreter／架構：

```powershell
python -m pip install -r tools/AnimeViewer/requirements.txt
cmake --build --preset dev --target cgl_pybind
python tools/AnimeViewer/web_viewer.py --root C:/CrossGate-Legacy --serial 100000 --port 7874
```

開啟 <http://127.0.0.1:7874/>。此角色工具不需要 Gradio、遊戲 server、視窗或 Vulkan。
既有 GraphicsDataViewer 仍有自己的 Gradio 需求。
若 module 不在 `build/dev/bin`，指定 `--module-dir <compiled module directory>`。
沒有 `--serial` 時先用 100000（若索引有它），否則用第一個編號。
關閉 terminal 中的工具用 Ctrl+C；服務會等待 HTTP worker 結束，再關閉 Session。

根目錄必須包含遊戲的 `bin` 素材。若載入的 `cgl` 太舊，工具會顯示實際 module
位置與重建提示。新 API 回傳的 top-down RGBA 已對齊，不再額外翻轉或丟棄 alpha。
Daytime／內嵌 palette 與原生解碼一致；目前不提供調色盤切換或素材編輯。

## 切換與資源限制

- 角色、動作、方向變更都直接取新的 JSON metadata，不重傳 Base64 HTML／iframe。
- 新選擇會清空舊畫面並中止舊 HTTP；仍在執行的 native decode 不強制中斷。
  前後端 revision 檢查會拒絕舊結果覆蓋新選擇。
- 後端只保留目前一個解碼 clip；切換前釋放舊 clip。預設 RGBA 上限 **8 MiB**，
  `--max-mib 1..32` 可調整；另有 **256 frames**、每格畫布 **4 MiB** 的上限。
- PNG 按需編碼，最多 **12 張／8 MiB**；同一格再次讀取可重用。
  完全相同的選擇也重用目前 clip。
- 瀏覽器最多保留 **12 張／8 MiB** 解碼 bitmap，一次一個影格請求；淘汰與離頁時
  呼叫 `ImageBitmap.close()`。播放最多每秒更新 30 次，仍依原始總時長計算影格，
  來不及顯示的格會跳過。背景頁面自動暫停。
- HTTP worker 上限為 8；Session 存取序列化。忙碌或錯誤會顯示在頁面，
  可改選或按「載入動畫」重試，不需 F5。

這些限制不是整個 Python／Chrome／驅動的記憶體上限，reader、索引、暫存、
HTTP 傳送、Python／瀏覽器 overhead 仍另外存在。超限素材會顯示錯誤，不自動提高限制。

## 測試與驗收

完成上述依賴與 binding 建置後，一個指令執行實際素材的 CPU／HTTP 驗收：

```powershell
python tools/AnimeViewer/verify_preview.py --root C:/CrossGate-Legacy --report build/preview-acceptance.json
```

不啟動瀏覽器或 Vulkan；檢查逐格 RGBA、切換／錯誤恢復、舊資源與關閉回收。
成功為 exit code 0；完整判定與人工畫面清單見
[Resource Preview 04 驗收](../../docs/resource-preview-acceptance.md)。

```powershell
python tools/AnimeViewer/tests/test_preview.py
python tools/AnimeViewer/tests/test_web_viewer.py
python tools/AnimeViewer/tests/test_verify_preview.py
# JavaScript tests need Node.js; running the viewer itself does not.
node tools/AnimeViewer/tests/test_playback.cjs
node tools/AnimeViewer/tests/test_viewer.cjs
```

這些測試不需要安裝遊戲或 GPU；selection 單元測試只用 standard library 與 fake
backend，HTTP 測試另用 NumPy／Pillow 驗證實際 PNG。CI 的 viewer lane 會在
Windows／Linux 執行上述檢查。
實際素材的像素／HTTP／資源回收量測與瀏覽器驗收限制，見
[網頁預覽驗證](../../docs/resource-preview-web-validation.md)。

## 舊版 GIF 匯出

目前腳本固定讀取 Classic 角色 `101406`，只匯出第一組方向／動作的 GIF，
輸出位於目前工作目錄。它不是網頁動畫預覽器，也沒有角色編輯能力。

在完成 `cgl_pybind` 建置、Python 環境已安裝 NumPy 與 Pillow 後，
從 repository 根目錄執行：

```powershell
$env:PYTHONPATH = (Resolve-Path build/dev/bin).Path
python tools/AnimeViewer/anime_viewer.py
```

Python 版本與架構必須和 `cgl.pyd` 相符。工具尚無素材根目錄參數，
`Session()` 使用編譯時設定預設值；目前為 `C:/CrossGate-Legacy`。
腳本會丟棄原始 alpha，GIF 時間也有量化，不能作為新預覽器的正確性基準。

歷史盤點、共用 API、當時實跑結果見
[Resource Preview 01 盤點](../../docs/resource-preview-audit.md)。
