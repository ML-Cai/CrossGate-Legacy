# 唯讀地圖網頁預覽

Python 啟動本機服務，以既有 C++ MAP reader、MapBuilder、可視範圍、座標與 footprint
排序，產生目前視角的 PNG。網頁支援 MAP 座標定位、拖曳平移、50%～200% 縮放、
地面／物件圖層切換。**目前不支援選格檢視、角色、編輯或存檔。只驗證 Classic。**

## 啟動

Python 3.12 與 extension 的版本／架構必須相符。先關閉使用舊 `cgl.pyd` 的工具
再重建，避免 Windows 鎖住 extension：

```powershell
python -m pip install -r tools/MapViewer/requirements.txt
cmake --build --preset dev --target cgl_pybind
python tools/MapViewer/web_viewer.py --root C:/CrossGate-Legacy --map map/0/100.dat --port 7875
```

開啟 <http://127.0.0.1:7875/>。另一張地圖用 `--map map/0/2100.dat`；目前由 CLI
選擇地圖，網頁開啟於地圖中心。非預設 binding 位置用 `--module-dir <directory>`。
也可以使用 [SDK-free production binding build](../../docs/map-preview-api.md) 的
`build/python-headless/bin`，不需要 Vulkan／視窗套件。關閉 terminal 用 Ctrl+C。

拖曳時即時平移已顯示的畫面，放開後由共用原生座標換算更新視角；也可以直接填
MAP X／Y 供鍵盤操作。滾輪以中心為基準縮放；更新失敗會顯示錯誤，可按更新重試，
不需重新載入頁面。切換參數期間的舊回應不會蓋回新畫面。

## 模組邊界與資源預算

- `Session.load_map_preview(file_path)` 產生 CPU plan；`MapPreview.draw_list` 共用
  原生 culling、offset 與地面先畫／物件 footprint 排序；`pan` 共用 `WorldToMap`。
  `acquire_map_page` 共用原生 atlas 解碼、Daytime／內嵌 palette、top-down RGBA。
  Python 只負責 viewport 合成與 HTTP；JavaScript 不解析檔案或另寫投影／深度排序。
- CPU plan 不保留 reader／Session／GPU；plan 查詢與既有 page 陣列可在 Session
  關閉後使用，新的 page 解碼仍須開啟的 Session 與相同素材根目錄。這版不承諾
  任意跨根目錄重用 plan。
- 一次只載入一張地圖。沿用原生 4,194,304 cells、最多八張 atlas page、每張
  32 MiB 上限；**不建立整張地圖的像素畫布**。
- PIL atlas LRU 最多兩張／64 MiB，先淘汰再解碼。每個視角的唯一可見 sprite
  複本總額最多 16 MiB，依 page 收集後按原生 painter order 合成，避免不同 page
  交錯造成反覆解碼。暫時複本在成功或失敗後都 close。
- Viewport 最多 1280×960、scale 0.5..2，最多 8,192 draw items／1,024 objects；
  超限顯示錯誤，請放大或關閉物件。先裁切再縮放，避免建立整張放大的巨大 sprite。
- Server 單一請求序列化 Session，socket timeout 10 秒、accept queue 2。前端
  debounce 120 ms，最多一個 viewport 請求執行中，完成後只更新最後需要的視角。
  不是每次 mousemove 都排一個解碼工作。前端只保留目前 bitmap，晚到結果與離頁
  都 close；替換時短暫同時存在新舊兩個 bitmap。
- 上述是明確 payload／owner 上限，不是整體 Python／瀏覽器／VRAM 上限；仍有
  native decode page、裁切／縮放、viewport、PNG、reader、allocator 等暫存。
  這是本機開發工具，不是遠端服務或即時遊戲 renderer。
- 只開放固定 HTML／JS、metadata、pan 與 viewport 路由。HTTP 不接受檔案路徑，
  不提供任意檔案讀取／寫入。MAP 路徑只在啟動時由本機 CLI 指定。

CPU nearest scaling 與 Vulkan rasterization 的次像素邊界可能相差一個像素；這版
共用 placement／排序規則，但不宣稱 PNG 與 GPU 畫面逐像素相同。尚未處理人物
遮擋或通用原格式編輯。

## 自動與人工驗收

```powershell
ctest --test-dir build/python-headless -R python_map_preview -V
python -W error::ResourceWarning tools/MapViewer/tests/test_web_viewer.py
node tools/MapViewer/tests/test_viewer.cjs
```

Native binding 6 個測試：原生像素／透明度／offset、圖層、物件 anchor 排序、
pan 方向與縮放、越界／預算／錯誤恢復，以及反覆 plan／page owner 釋放。
Python 6 個測試：真實 PNG 裁切／縮放、兩張 LRU／close、交錯 page 只解碼一次、
失敗時裁切圖釋放、可見資料預算與 HTTP 驗證／恢復／固定路由。
JS 以模擬 DOM／fetch／bitmap 驗證序列化、過時結果釋放、錯誤停止／重試、拖曳、
縮放與離頁；**不是實際瀏覽器驗收**。CI 在 Windows／Linux 跑 production binding
與 web tests；本機另跑 Windows ASan。

人工需確認：

1. 100.dat 與 2100.dat 均可顯示，拖曳、座標定位、縮放與圖層開關不需 F5。
2. 快速變更參數後最後視角正確；錯誤可重試；小視窗的拖曳距離對應正確。
3. 在相同 MAP 中心／比例／viewport 下，與原生客戶端對照房屋、樹木的 offset
   與遮擋，包含大型 footprint；原生人物先不納入此版比較。
4. 實際 Chrome 操作與驅動穩定性另行記錄，過去 BSOD 原因仍未確認。

## 本次本機觀察

2026-10-29，Windows x64／Python 3.12.10，實際 Classic `100.dat`（840×610）與
`2100.dat`（100×100），每張圖經同一 loopback server 連續取得 20 個 1024×720
視角，包含不同位置、0.5／1／2 倍與圖層切換，全數成功。

| 地圖 | 首次 HTTP view | 全部請求 min～max | Page cache 觀察峰值 |
| --- | ---: | ---: | ---: |
| 100 | 150.78 ms | 37.84～150.78 ms | 40,330,752 B |
| 2100 | 60.66 ms | 27.22～86.63 ms | 16,638,072 B |

此時間不含最初的 MAP／atlas metadata 建置、瀏覽器繪製，也不是 FPS 或跨機器
保證。結束後 page NumPy owners、CPU plan 均釋放，reader 清單為空；有限次觀察
不等同整個程序無 leak。檢視過 CPU 產生的 PNG，**尚未完成實際瀏覽器操作與原生
GPU 並排對照**，不因此標記整個地圖看板項目驗收完成。素材與 raw logs 不提交。
