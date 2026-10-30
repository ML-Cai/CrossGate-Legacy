# Resource Preview：版本、同步方向與逐幀驗收

對應 Resource Preview 看板的 #96、#97、#98 及原 04。
本次只推進資源預覽，不推進地圖、編輯或存檔。後續不得僅因預覽可用就切到其他
Project；先核對本看板的剩餘驗收。

## 行為

版本／角色／動作選擇後，中央顯示選定方向，下方列出八個素材方向。
未提供方向與單方向的規劃／讀取錯誤均有獨立標示。點選方向或使用左右鍵切換大圖，
共用同一個 bitmap；不重新載入 metadata 或頁面。

所有可用方向共用一個 normalized phase。同步週期採第一個有效方向的原始總
duration；各方向以 `floor(phase * own_frame_count)` 取格。原始 duration 另列在
縮圖下方。若原始時長不同，此模式刻意對齊循環，不承諾每個方向同時維持自己的
原始速度。暫停、逐格、seek 一起更新，背景頁面暫停。

一次只執行一個影格請求，一批先取目前方向，再取其他可用方向；同一批用相同
phase 繪製全部 canvas。落後時跳到最新 phase，不累積未播放的工作。畫面最多
30 批／秒，不是每個方向各開一套 RAF。錯誤方向不自動反覆重試，重新載入可恢復。

## 所有權與預算

`Session.plan_animation(version, serial, direction, motion)` 產生 owned metadata plan，
沒有 RGBA、reader 或 Session owner；`metadata` 回傳獨立 dict。
`decode_animation_frame(plan, index, max_bytes=4 MiB)` 只解碼一格，回傳獨立 top-down
RGBA NumPy 陣列。整段與單格走同一個 C++ `AnimationDecoder::render`，不另寫解析、
對齊或色序邏輯。

Plan 只能交回建立它的 Session，`invalidate_resources`／`close` 後不能再用它解碼；
metadata 與先前已取得的 frame 仍有效。epoch 是 weak ownership，不會讓關閉的
Session 或 reader 繼續存活。`trim_cache` 不更換 reader，plan 可繼續使用。

- 每動作最多 256 frames，每格 4 MiB；原生邏輯仍有 256 MiB 的 logical clip
  payload 上限。規劃時驗證，超限方向顯示錯誤，不自動提高限制。
- 一組最多八個 metadata plans，不保留八組整段 RGBA。編碼後即釋放單幀 CPU owner。
- 後端 PNG cache 全方向合計最多 24 張／8 MiB；瀏覽器全方向合計最多 32 張／
  32 MiB bitmap。正在繪製的一批至多八格會被 pin，淘汰其他格，避免使用已 close
  的 bitmap。解碼替換時可短暫多一個最多 4 MiB bitmap；canvas／codec 等另計。
- 版本切換先清空舊 plans／PNG，再 invalidate 全部 readers；前端清除全部 bitmap。
  前後端 revision 阻止過時版本／選擇覆蓋新狀態。
- 這些是 payload／owner 上限，不是整個程序或實體 VRAM 上限。reader 索引、檔案
  cache、palette、HTTP、PIL、browser overhead 仍額外存在。

既有 `acquire_animation_pixels`、`/api/select` 與 `verify_preview.py` 保留相容性；
它們仍是整段輸出與舊預算。新版網頁使用 `/api/group`、`/api/direction/...png`，
不是偷偷放大舊整段預算。

## PUK 修正與邊界

既有 reader 固定讀 12-byte header，會把部分 PUK 的延伸 header 當成 frame ID。
現在保留 index 的 format flags，bit 0 表示 20-byte motion header，額外驗證 sentinel；
標準格式不靠 frame payload 的偶然位元組猜格式。`extended_flags` bit 16 依對稱
方向重用圖號的樣本作水平鏡像，連同錨點 X offset 一起反射；其他位元保留，不
自行倒播或猜測全部遊戲事件語意。合成非對稱圖與實際共用影格的方向對均有驗證。

配色優先順序為影格內嵌 palette、動畫 palette、Daytime。PUK2_V2／PUK3_V1 的
動畫 palette 由 PUK2_V1 圖資提供，安裝資料需包含該組檔案；PUK4 組使用自身圖資。
動畫 palette 索引另外檢查實際長度，禁止短 palette 造成越界。
參考[原作者格式研究](https://chivincent.net/blog/cross-gate-binary-format/)與
[header 研究實作](https://github.com/x-gate/xglib/blob/main/src/types/anime.rs)，並以本機
原始檔與合成反例驗證；這不是官方格式規格或全部私服／版本的相容性保證。

每個 enum 版本都提供選擇，但不以 enum 存在代表素材都已驗證。不存在的檔案、
缺少引用圖像或超限會明確回報。沒有在 Python／JS 寫另一套資源 parser。

## 可重跑驗收

```powershell
python tools/AnimeViewer/verify_directions.py --root C:/CrossGate-Legacy --cycles 128 --report build/direction-matrix.json
# 可只測指定版本；名稱採 native enum 去掉 CG_VERSION_ 前綴。
python tools/AnimeViewer/verify_directions.py --root C:/CrossGate-Legacy --versions Classic PUK1
# 選用的 CPU RSS 取樣；不影響工具執行依賴。
python -m pip install psutil
python tools/AnimeViewer/verify_directions.py --root C:/CrossGate-Legacy --cycles 128 --require-memory --report build/direction-memory.json
```

需要新版 binding、NumPy、Pillow；非預設位置用 `--module-dir`。
每個指定版本取一個代表角色，Classic 另含 101406 的 STANDARD；逐方向、逐格把 HTTP
PNG 與 native 單格結果比對，再反覆切換版本／角色並驗證錯誤恢復。比對用的是同一套
CPU 結果，不是獨立 GPU oracle。失敗版本不會靜默略過，報告與 exit code 都會失敗。
第一個指定版本無法啟動時直接非零退出，不假裝完成矩陣。

關閉後檢查計畫／frame weak owners、PNG cache、reader 清單。RSS 為選用程序取樣，
不含 browser；暖機兩輪後比較涵蓋同樣 target 組合的首尾窗口。樣本不足或未安裝
psutil 時欄位為 null，不當成零。程序 RSS 平穩與 weakref 都不等於全域 leak-freedom。

## 2026-10-29 本機結果

Windows x64、Python 3.12.10、NumPy 2.4.1、Pillow 12.1.0：八個版本的代表素材均通過
CPU／HTTP matrix，共 9 個角色／動作組、817 張 PNG，另驗證 9 組鏡像方向對；128 次跨版本切換後，991 次
plan owner 檢查與全部已取樣 frame owner 均釋放，reader 清單為空。

| 版本 | 實際 catalog IDs（去重後） | 代表角色 |
| --- | ---: | --- |
| Classic | 806 | 100000、101406 |
| PUK1 | 827 | 105000 |
| PUK2_V1 | 342 | 110350 |
| PUK2_V2 | 343 | 110300 |
| PUK3_V1 | 149 | 110505 |
| PUK4_V1 | 785 | 104000 |
| PUK4_V2 | 68 | 104854 |
| PUK4_V3 | 57 | 104854 |

101406 STANDARD 的整段 payload 超過舊 8 MiB，但單格 streaming 成功，不提高整段
預算。PNG cache 觀察峰值 1,073,128 B。該次 HTTP PNG median 7.49 ms、max 55.17 ms，
不含初始 catalog／plan、browser decode／paint，不當作端到端 UI 延遲或 FPS。

CPU RSS 取樣峰值 107,122,688 B；暖機後首尾窗口 median 為 61,460,480／61,472,768 B，
差 12,288 B。取樣範圍為切換循環，包含驗收程序本身的開銷，不是單獨 server 的
工作集或整個初始驗證期間的峰值；這是有限次本機觀察，不宣稱沒有 leak。
尚未完成的實際瀏覽器操作、browser 記憶體與原生 GPU 視覺對照，須另記結果；
報告明確保留 `browser_visual`／`independent_renderer_comparison` 為 `not_run`。
