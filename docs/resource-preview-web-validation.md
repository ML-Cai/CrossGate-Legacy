# Resource Preview 03：固定頁面與有上限的動畫載入

本文件保留 PR #92 的歷史驗證。新版網頁已改為多版本、多方向、按單格解碼，
當前契約與驗收見[版本與同步方向驗證](resource-preview-directions.md)。

原本尚未提交的 Gradio／iframe 試作，每次切換會重新把所有影格編成 PNG、Base64、
HTML，再重建 iframe 並預載全部圖片。使用者回報切換慢、必須重新整理，並在切換
動作附近發生 BSOD。這版改為 Python standard-library 的本機 HTTP service 與固定
HTML／JavaScript 頁面；不另寫資源解析或新增前端框架。原 GraphicsDataViewer 不變。

## 實作與邊界

`Preview` 共用 PR #91 的 C++ Session。`/api/catalog` 取得編號，`/api/select`
取得目前動畫 metadata 與 opaque token，`/api/frame/<token>/<index>.png` 按需取得
PNG。只有固定 whitelist 的頁面／腳本可讀取，不提供任意檔案路由。服務只綁定
127.0.0.1，供單機開發預覽，不是多人服務或對外部署方案。

選擇與 PNG 都由同一 Session lock 序列化。後端一個 RGBA clip、12 張／8 MiB PNG
cache；瀏覽器一個待完成影格請求、12 張／8 MiB bitmap cache。切換釋放舊 owner，
前端 abort 舊 HTTP 並檢查 revision；後端也拒絕該分頁較舊、仍排隊的選擇。
revision 紀錄最多 16 個 client，不保留無上限歷史。舊 token 回傳 410。

預設 clip payload 為 8 MiB，可調至 32 MiB；最多 256 frames，每格畫布最多 4 MiB。
native 解碼的限制仍在 API 層；browser frame 限制在產生 PNG／canvas 前檢查。
native call 一旦開始不強行停止；abort 只取消等待／過時呈現。8 個 HTTP worker
限制了同時保留的回應，超載回 503，不建立無上限執行緒。

載入後暫停，只請求第一格。播放依 total duration 的連續 phase 計算 frame index，
不把 101 ms 之類總時長逐格量化。顯示最多 30 次／秒，讀取落後時跳格而不累積
無限待顯示影格；背景頁暫停。多於 cache 可容納的動畫在循環時可能重新請求影格，
不承諾每一格從此不再讀 server。沒有 GIF 轉換、整段 Base64 或 iframe 重建。

## 自動驗收

| 層級 | 具體檢查 |
| --- | --- |
| Python selection，9 個 unittest | metadata 不預載圖片；同選擇／PNG cache hit；無效 serial／動作資料／大小拒絕；失敗後重試；舊 clip 先釋放；PNG count／bytes 淘汰；舊 revision 不覆寫；100 次切換及 close 回收 |
| 真實 HTTP，8 個 unittest | 固定頁面／腳本與 catalog 不讀像素；不重新整理連續切換；只請求指定格；PNG hit／舊 token；實際 PNG round-trip 保留 RGBA；400／410／422 錯誤恢復；任意路徑拒絕；worker 超載 503；server shutdown；舊 binding／錯誤 root 的啟動提示 |
| Playback JS | 精確 period、暫停／續播、101 ms／7 frames 的 seek、首尾循環及無效時間拒絕 |
| Viewer JS（DOM／HTTP／bitmap 模擬） | 即使 abort 無法停止舊 backend，舊回應仍被忽略；啟動只請求一格；PNG 失敗不造成遞迴請求風暴；reload 恢復；晚到 bitmap 關閉而不繪製；依 count／bytes 淘汰與離頁釋放 |

Python selection case 也加入完整與 SDK-free CTest；另外的 viewer workflow 在
Windows／Linux 跑 Python HTTP 與 Node tests。Node 僅是測試需求；工具啟動只需
Python、NumPy、Pillow 與編譯好的 C++ binding。未改 C++／Vulkan 實作。

## 實際素材與量測

2026-10-28，Windows x64、Python 3.12.10、Pillow 12.1.0、NumPy 2.4.1，Classic
素材。在同一程序先暖機，再交錯量測四次；比較舊試作的「native decode＋整段 PNG／
Base64 frame list」與新流程的「native decode＋metadata＋第一格 PNG」。
兩者都沒有 browser／網路／Gradio 初始化時間，不能把此表當作完整畫面延遲。

| Serial／NORTH 動作 | Frames | 舊整段編碼 median | 新首格 median | 舊 Base64 frame list | 新 metadata＋首格 PNG |
| --- | ---: | ---: | ---: | ---: | ---: |
| 100000／STANDARD | 4 | 3.50 ms | 2.44 ms | 19,484 B | 5,400 B |
| 100000／WALK | 6 | 5.17 ms | 2.82 ms | 29,476 B | 5,533 B |
| 101406／ATTACK | 16 | 122.27 ms | 12.89 ms | 560,704 B | 37,821 B |

舊 bytes 僅計 escaped Base64 frame list，未計完整 iframe／Gradio shell；新 bytes
計 metadata JSON 與單格 PNG，皆未計 HTTP headers。新 PNG 使用較低壓縮等級，
用 CPU／較大單格傳輸換取載入時間；不是相同壓縮參數的 encoder 比較。
每個測量樣本的第一格 PNG 解碼回 RGBA，與 native aligned frame 逐像素一致。

實際 100000 的 STANDARD／WALK 與八方向共 120 次切換（前 20 次暖機），每次只
讀第一格，cache 維持界限；暖機後首／尾 10 點 RSS median 均為 86,147,072 B。
關閉後 Session reader 清單為空。RSS 包含 Python／allocator／reader overhead，
有限次平穩不證明所有 leak 都不存在。

真實 HTTP 使用同一服務連續從 idle／north 切換到 walk／east，回傳正確 metadata
及 PNG，不重新載入首頁。這是實際 native／HTTP 的整合檢查，不是 GPU 畫面測試。

## BSOD 與尚未驗證部分

本機 Windows 事件記錄有 0x133、parameter 1 = 0，對應 DPC／ISR 逾時；
原因說明見 [Microsoft bug-check 文件](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-0x133-dpc-watchdog-violation)。
kernel dump 的讀取被檔案權限拒絕，因此沒有判定責任驅動；重開機後的裝置警告
也不能直接當作當機原因。沒有修改驅動、Windows／Chrome 的安全或硬體加速設定。

本次刻意不重跑疑似觸發 BSOD 的 GPU 畫面操作。JS 測試中的 DOM／bitmap 是
模擬物件；上述 cache 數字不等於 Chrome 全程序或實體 VRAM 上限。已修正輸入／
載入流程與資源保留方式，**不宣稱 BSOD 已修復**，實際 Chrome 視覺與驅動驗證仍待
後續低負載測試。原 dump、事件識別資料、素材及 raw logs 不提交。
