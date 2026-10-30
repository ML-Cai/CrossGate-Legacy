# Resource Preview 載入時間驗收

這是 [#98](https://github.com/ML-Cai/CrossGate-Legacy/issues/98) 的載入／首批畫面量測補強，
接續已合併的版本選擇、八方向同步及逐格解碼。沒有提高快取上限或新增解碼執行緒。

## 網頁操作

依 [AnimeViewer 說明](../tools/AnimeViewer/README.md) 啟動工具，選取角色或動作。
狀態列下方顯示「規劃 X ms · 首批繪製 Y ms（N/M 方向）」。

- 規劃：從本次選取開始，到 `/api/group` 回應解析完成，包含 HTTP 等待與原生 metadata 規劃。
- 首批繪製：從同一時間起算，直到第一批有效影格完成 canvas draw；包含規劃、
  按順序取得各方向 PNG、bitmap 解碼與 draw。選取方向必須有有效 bitmap 才會記錄。
- N/M 為這批成功影格／可規劃方向數；部分失敗時不得當成全方向成功。
- 數值只記一次；切換方向、逐格或播放不會變成新的載入樣本。切換角色／動作、版本或重試會重設。
- 過期請求不能覆寫新選取的時間；失敗顯示未完成或繪製失敗。

兩項時間從相同起點計算，不能相加。版本清單查詢在選取前，未包含在網頁數值；
canvas draw 返回亦不代表 compositor／螢幕呈現完成，不可轉成 FPS。

2026-10-29，Windows、VS Code 整合式 Chromium 實際操作 Classic 100000 STANDARD：
規劃 11.4 ms、首批繪製 103.7 ms，8/8 方向。這是單次已暖機環境的觀察，沒有清除 OS 檔案快取，
不代表所有硬體／素材都能達到相同數字，也不構成效能改善前後比較。

## HTTP 報告

```powershell
python tools/AnimeViewer/verify_directions.py --root C:/CrossGate-Legacy --module-dir build/python-headless/bin --cycles 128 --report build/direction-timing.json
```

`--module-dir` 請改為本機新 binding 位置；CPU matrix 不需要視窗或 Vulkan。
原有像素、鏡像、版本切換、owner 與 reader 回收檢查繼續執行。

| 欄位 | 範圍 |
| --- | --- |
| 每版本 `catalog_http_ms` | 查詢版本清單到 JSON 解析完成 |
| 每樣本 `group_http_ms` | 選取到方向 metadata 回應解析完成 |
| `first_png_http_ms` | 第一個方向第 0 格的 HTTP 請求到 PNG 回應讀取完成 |
| `selection_to_first_png_ms` | 同一次選取到首張 PNG 讀取完成；尚未執行像素比對 |
| `first_direction`、`first_png_cache` | 實際先查詢的方向與 HTTP `X-Frame-Cache` header |
| `group_cache_hit` | 該次方向規劃是否重用既有 group |
| `directions` | 每方向的畫布、offset、frame count、duration 及 bytes，供人工對照 |

第一個方向採報告列舉順序，不宣稱等於網頁目前選取方向。HTTP 時間不包含 Session／Preview
建構、瀏覽器 bitmap 解碼／draw、螢幕呈現或 GPU。PNG 收到後若解碼／像素比對失敗，
仍記收到回應的時間，但樣本／版本整體必須為 failed；不得只憑時間欄位宣稱素材成功。

OS 檔案快取不受控，`miss` 只代表 PNG cache miss，不等於磁碟冷啟動。
原有 CPU RSS 只採樣驗收程序；bitmap bytes 也不等於 browser process RSS／VRAM。

同日八版本實測的 HTTP 樣本如下，單位 ms，均為 PNG cache miss。這與上方網頁樣本
是不同次操作，不能拿它們相減推估 browser 開銷，也不作 CI 效能門檻。

| 版本／角色 | 規劃回應 | 首張 PNG | 選取至首張 PNG |
| --- | ---: | ---: | ---: |
| Classic 100000 | 127.57 | 48.62 | 176.24 |
| Classic 101406 | 2.63 | 4.75 | 7.43 |
| PUK1 105000 | 214.74 | 3.48 | 218.25 |
| PUK2_V1 110350 | 13.94 | 5.52 | 19.50 |
| PUK2_V2 110300 | 45.15 | 5.16 | 50.35 |
| PUK3_V1 110505 | 14.98 | 5.84 | 20.86 |
| PUK4_V1 104000 | 310.83 | 4.84 | 315.72 |
| PUK4_V2 104854 | 54.66 | 5.85 | 60.57 |
| PUK4_V3 104854 | 30.70 | 22.44 | 53.20 |

817 張 PNG 像素比對與 128 次切換通過，plan／frame owner 釋放且 reader 為零。
本次 CPU RSS 暖機窗口差 98,304 B，僅為有限次取樣，不宣稱沒有記憶體洩漏。

## 仍未完成的驗收

瀏覽器程序整體記憶體走勢，以及對照原生 GPU 畫面的方向、offset、透明輪廓與時間，
仍由 04／#98 追蹤。這次量測不代填上述結果，也不宣稱已解決先前 BSOD。
