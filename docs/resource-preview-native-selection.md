# Resource Preview：原生版本與固定影格對照

獨立 AnimationScene 現在可讀取與網頁相同的版本、角色、方向、動作，並暫停在指定影格。
不需要 Python 或工具 server；既有未填新設定的 INI 仍採 Classic、持續播放與原本兩個示範角色的半週期相位差。

## 執行

重新建置原生客戶端後，輸出目錄會有 `animation-reference.ini`：

```powershell
cmake --build build/dev --target cgl-client --parallel 4
.\build\dev\bin\cgl-client.exe --settings .\build\dev\bin\animation-reference.ini
```

Windows 須先使用專案既有的 C++／Vulkan 建置環境。素材位置沿用 Settings 的 EngineRootPath 預設值；
若安裝位置不同，在 INI 加上 `[General]` 的 `EngineRootPath`。不要覆寫程式旁的既有 `settings.ini`。
設定範例不含素材，也不需要啟動任何網頁工具。

```ini
[Map]
FilePath=

[Animation]
Version=3
SerialNum=110300
Direction=7
Motion=0
Paused=true
StartFrame=1
```

- `Version`：0 Classic、1 PUK1、2 PUK2_V1、3 PUK2_V2、4 PUK3_V1、5 PUK4_V1、6 PUK4_V2、7 PUK4_V3。
- `Direction`／`Motion`：採共用 enum 編號，與 Python API 相同；不將缺少的動作／方向換成其他素材。
- `StartFrame`：**從 0 起算**，必須存在於 clip；預設 0。網頁「影格 2 / N」對應 `StartFrame=1`。
  非零起始影格使用該格時間區間的中點，避免浮點數在邊界選到前一格。
- `Paused=true`：兩個既有示範角色都固定在相同指定影格；false 維持播放與兩個角色的半週期差。
- 標題顯示實際版本編號、serial、方向、動作與 paused frame；載入完成 log 記錄第一個角色的實際影格。
- `Map.FilePath` 必須留空才會進入獨立動畫場景。這些新選項不改變地圖人物或 server demo 的版本／播放規則。

無效版本／方向／動作會在規劃前回報；缺少素材依既有 reader 錯誤處理。
超出範圍的 StartFrame 在取得 clip metadata 後報錯，不會 clamp 或循環到另一格。
不提高貼圖預算、不新增 renderer／快取、不改變 GPU 提交完成後釋放的既有順序。

## 對照方法與限制

在網頁選相同 Version／Serial／Motion／Direction，暫停並 seek 到對應影格，再對照原生的像素方向、
顏色、透明輪廓與 frame padding。可先比較 Classic 100000、PUK2_V2 110300、PUK3_V1 110505 的 STANDARD。

網頁會依所有方向的共同畫布範圍自動縮放，原生會把選取 clip 的畫布置中，
因此兩邊畫面的絕對位置、顯示比例不是 offset 正確性的直接證據。
本次提供可重現的版本／影格選取能力；完整錨點／遮擋、逐版本視覺與播放時間比對仍需另記結果。
自動啟動成功不代表已完成上述人工驗收，也不取代 browser process 記憶體走勢的量測。

## 測試

```powershell
ctest --test-dir build/python-headless -R "AnimationPreview|AnimationBuilder|python_animation_stream" --output-on-failure
ctest --test-dir build/dev -R "^cgl_client_(animation_reference_|invalid_animation_|startup$)" --output-on-failure
```

第一組不需視窗／Vulkan：版本／identity、無效輸入、INI 載入與舊預設、各種 frame count 的精確起始格。
第二組需要 Vulkan 桌面、validation layer 與對應外部素材，會建立暫時 INI，測完清除，
確認指定素材／暫停影格、單一共享 clip 上傳、場景進出、錯誤 exit code、Engine close 且沒有 validation error。

2026-11-02 本機 Windows：一般及 ASan CPU 測試各 21/21 CTest 通過；實際 Vulkan 測試 9/9 通過，
包含上述三種版本的方向 7、STANDARD、第 1 格（從 0 起算），以及無效版本與無效影格。
這是實際 GPU 啟動與生命週期檢查，未宣稱已完成跨網頁的獨立像素／時間視覺驗收。
