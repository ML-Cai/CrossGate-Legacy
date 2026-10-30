# Resource Preview 04：可重跑的驗收

此文件記錄整段 clip API 的原驗收。新版多方向／PUK／逐幀介面請使用
[版本與同步方向驗證](resource-preview-directions.md)及 `verify_directions.py`；
下列舊整段 8 MiB 限制仍存在，但不再限制新版網頁一次只能預覽小動畫。

目標是驗收目前唯讀角色動畫工具，不擴充角色編輯、地圖功能或公開服務。
完成依賴與 `cgl_pybind` 建置後，從 repository 根目錄執行：

```powershell
python tools/AnimeViewer/verify_preview.py --root C:/CrossGate-Legacy --report build/preview-acceptance.json
```

Python 必須與 `cgl.pyd` 的版本／架構相符，並有 NumPy、Pillow。
非預設 build 位置用 `--module-dir <directory>`。
此指令不開啟瀏覽器、不初始化 Vulkan，不會連到已開啟的 viewer；使用臨時 loopback
port、獨立 Session，結束時停止服務並釋放 Session。素材不會被修改。

## 自動驗收的判定

預設使用 Classic `100000`、`100001`，32 次切換；可用
`--serials 100000 100001 --cycles 120` 增加循環（2..1000）。
指定的角色必須存在；工具交替角色，依實際可用 keys 輪流選擇動作與方向，
不是亂填編號再接受 fallback。循環次數不代表已遍歷整個素材庫。

- 首頁只取一次，後續以 HTTP API 切換；核對角色、動作、方向、畫布尺寸、
  offset、影格數與總 duration。
- 每次選擇的每張 PNG 解碼回 RGBA，逐像素比對目前 C++ CPU 組裝結果，包含
  alpha。這可發現傳輸／PNG 色序與透明度問題，**不是另一套獨立 decoder 的交叉驗證**。
- 切換到不同 key 後，上一個 NumPy owner 的 weak reference 必須失效；舊 token
  必須回應 410。每次讀格檢查 PNG cache 不超過 12 張／8 MiB。
- 無效角色回應 400，接著合法重試必須成功並重用目前 clip；超界影格回應 400。
- 結束後 HTTP thread 停止、最後的像素 owner 被釋放、Preview cache／revision
  清空，Session reader 清單為空。失敗路徑也停止服務並關閉 Session。

成功以 exit code 0 回傳 JSON，可選擇寫入 `--report`；失敗為非零 exit code。
報告只含 metadata／計數，不含圖片、素材根目錄或完整本機 log。
既有報告不代表本次通過，請以本次 exit code 為準。

若某個動作超過目前 8 MiB clip 或 4 MiB/frame 等限制，驗收會失敗並顯示 HTTP
422，不略過樣本、不自動放大預算。請記錄角色／動作並另行決定需求；
例如本機 `101406` 的 STANDARD 超過預設 clip 預算，不能把其 ATTACK 成功的
量測當作全部動作都可載入。

這些檢查驗證特定 owner 與 cache 上限，不量測瀏覽器／驅動／VRAM，也不等同
整個程序沒有 memory leak；reader／allocator 的額外記憶體不在 PNG cache 中。
報告明確保留 `browser_visual`、`independent_renderer_comparison` 為 `not_run`。

## 人工畫面驗收

先用[啟動指令](../tools/AnimeViewer/README.md)開啟工具。下表須實際操作後才記為
通過；自動 HTTP 或模擬 DOM 測試不能代填。遇到當機停止操作並記錄，不反覆壓測。

| 操作 | 通過條件 |
| --- | --- |
| 開啟 100000，依序選 STANDARD、WALK 與可用方向 | 首格可見、透明區正常、選項與畫面一致；載入後暫停 |
| 交替兩個角色與多個方向 | 不按 F5 即更新；最後一次選擇生效，無舊角色蓋回 |
| 播放、暫停、逐格與拖影格滑桿 | 暫停不自動前進，首尾循環正常，滑桿與影格標示一致 |
| 切到背景再回來、調整縮放 | 背景自動暫停；縮放不改變動畫資料或方向 |
| 有效／超限角色交替，失敗後改選或按載入動畫 | 顯示可理解錯誤、合法選擇能恢復，不需重新整理 |
| 與原生客戶端對照相同 serial／motion／素材方向／Daytime palette | 對齊、朝向、透明輪廓與總時長相符；不同 canvas 背景不當作素材錯誤 |

記錄日期、OS／瀏覽器版本、測試 key、結果與差異；只附必要畫面，不上傳遊戲素材
或系統 dump。舊 GIF 匯出會損失透明與時間精度，不能作為正確性基準。
先前 BSOD 的責任驅動仍未確認，這張表不代表已排除此問題。

## CI 與本機測試

```powershell
python -W error::ResourceWarning tools/AnimeViewer/tests/test_verify_preview.py
```

Windows／Linux viewer CI 用合成 NumPy 素材與實際 HTTP／PNG 執行此測試：正常
切換與關閉、損壞 PNG、合法 PNG 但像素錯誤、故意保留舊 owner，以及無效輸入。
這些負向案例確保驗收器會拒絕錯誤，而非只測試成功輸出。
CI 不包含商業素材，真實資源的驗收指令需在本機另跑。

## 本次本機結果

2026-10-29，Windows x64／Python 3.12.10／Classic `100000`、`100001`：
120 次切換、1,032 張 PNG 的 RGBA 比對通過，119 個被替換的像素 owner 均釋放。
PNG cache 觀察到最多 12 張／64,532 bytes；結束後最後像素 owner 釋放、reader
清單為空。上述錯誤恢復、過期 token 與關閉檢查亦通過。
這次循環只涵蓋兩個角色的部分 keys，不宣稱涵蓋全部素材。

先以 `101406` 作為第二個角色時，STANDARD 選擇被 8 MiB 限制拒絕，驗收器以
非零 exit code 正確報錯並關閉服務；沒有為了通過而調高限制。
瀏覽器人工清單及原生畫面對照**尚未執行**。
