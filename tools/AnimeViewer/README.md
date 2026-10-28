# AnimeViewer（舊版 GIF 匯出）

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

現況、共用 API、實跑結果與後續範圍見
[Resource Preview 01 盤點](../../docs/resource-preview-audit.md)。
