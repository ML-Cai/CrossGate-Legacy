# Resource Preview：瀏覽器程序記憶體量測

此指令只讀取指定瀏覽器主程序與當下可發現的子程序，不操作網頁、不關閉程序，
不收集網址、命令列、環境變數、heap dump 或素材。它補充 #98／04 的人工驗收，
與 `verify_directions.py` 的 Python 程序 RSS、網頁的 bitmap bytes 分開記錄。

## 使用方式

```powershell
python -m pip install -r tools/AnimeViewer/requirements-memory.txt
python tools/AnimeViewer/record_browser_memory.py --pid 12345 --samples 60 --interval 1 --warmup 10 --window 5 --label switching --report build/browser-switching.json
```

`12345` 必須換成這次啟動的瀏覽器主程序 PID，不能直接沿用範例或上次的 PID。
可從作業系統的程序檢視工具取得；工具接受 Chrome／Chromium、Edge、Firefox 主程序，
拒絕同一瀏覽器的 renderer／子程序與非瀏覽器程序。Windows 可唯讀列出 Chrome 的程序關係：

```powershell
Get-CimInstance Win32_Process -Filter "name='chrome.exe'" | Select-Object ProcessId, ParentProcessId
```

選擇 ParentProcessId 不屬於同一瀏覽器的主程序。選錯、權限不足或 PID 不存在時會明確失敗。
`psutil` 為選用依賴，預覽 server 不需要安裝它；量測也不需要 C++ binding、Pillow 或 GPU 初始化。

## 人工操作流程

建議使用只開預覽頁面的測試瀏覽器，減少其他分頁與擴充功能干擾；既有使用者分頁不必關閉。
如果使用共用瀏覽器，務必記錄這個限制，不能把整棵程序樹的變化歸因於預覽器。

1. 啟動預覽，等待首批繪製，錄一份 `idle` 基線。
2. 用相同 PID 錄製 `switching`，同時反覆選取相同一組 Classic／PUK、角色與動作，
   包含 100000 WALK 與 101406 STANDARD，播放、暫停、逐格與重試。記錄實際操作次數／範圍。
3. 錄製 `released`，關閉測試預覽分頁或導向空白頁；保留瀏覽器主程序，等待回收。
4. 比較報告的主程序建立時間、操作範圍與取樣覆蓋，再比較窗口及峰值。

不要用停止 server 代替分頁釋放；瀏覽器仍可能持有已解碼影格。也不要為了取得漂亮數字，
強制 GC、修改安全設定或自動提高 cache 上限。瀏覽器本身可以保留 allocator／renderer pool，
關頁後 RSS 不一定立即回到基線。

## 報告與判讀

- `rss_sum_bytes`：主程序與這次發現的子程序 RSS 加總。共用頁面可能重複計入，
  **不是獨占實體記憶體、JS heap、bitmap payload 或 VRAM**。GPU 子程序的 CPU RSS 可包含在內，
  GPU 顯示記憶體不包含。
- `root_rss_bytes`、程序數與成功讀取數：保留取樣範圍。每次重新列舉子程序，涵蓋程序新增／退出；
  取樣不是跨所有程序的原子快照。
- 任一程序在取樣中退出、被重用或無權讀取，該筆標 `complete=false`、RSS 加總為 null；
  不把部分合計或零偽裝成完整數字。主程序退出、identity 改變或無法列舉時停止錄製。
- `warmup` 排除前 N 筆；其後至少需要兩個不重疊的 `window`，且穩態區間每筆都完整，
  才計算首尾窗口 median 差。資料不足或漏讀時差值為 null，不會挑掉漏讀筆後硬算。
- `rss_sum_peak_sampled_bytes` 只取完整樣本的觀察峰值，未必捕捉瞬間尖峰。
- `status=recorded` 只表示取樣完整，**不代表 leak test passed**。`incomplete` 仍寫出報告，
  exit code 為 2；無法開始／寫檔／參數錯誤為 1，完整錄製為 0。

預設 60 筆、間隔 1 秒，約 59 秒加取樣開銷；最多 600 筆，間隔 0.1–10 秒。
輸出 JSON 留在本機 `build/`；對外只提交範圍、操作、摘要與限制，不提交完整本機紀錄。

## 測試與驗收狀態

```powershell
python -W error::ResourceWarning tools/AnimeViewer/tests/test_browser_memory.py
```

CI 使用合成程序介面驗證 RSS 加總／去重、子程序動態變更、AccessDenied、退出／PID 重用、
取樣不足、窗口不重疊、排程與錯誤狀態，不需要開瀏覽器。實際 psutil 取樣另行 smoke test。

2026-10-30，Windows／psutil 7.2.2，既有 Chrome 共用程序樹：30 筆、間隔 0.5 秒，
30 筆完整、每筆 15 個程序；取樣峰值 2,251,755,520 B。排除前 6 筆後，首尾各 5 筆的
median 為 2,251,640,832／2,251,755,520 B，差 114,688 B；命令 exit 0。
這份結果僅驗證工具可讀取真實程序與輸出報告，期間未完成指定的資源切換流程，
包含其他分頁，不歸因於預覽器，也不是實體記憶體用量。

目前不能把共用瀏覽器的短時取樣或 CPU／DOM 模擬當成「Resource Preview 反覆操作記憶體驗收」。
完整人工操作期間的記憶體走勢與獨立原生 GPU 畫面比對仍由 #98／04 追蹤，
也不宣稱已修復先前 BSOD。
