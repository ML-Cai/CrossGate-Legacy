# Resource Preview 02：CPU animation API

本步驟提供角色預覽需要的值資料，不建立網頁、視窗、Vulkan 物件或遊戲 server。
基於已合併的 01（PR #90），共用既有原生動畫組裝，沒有在 Python 重寫解析器。

## 介面與重用邊界

- `Session.acquire_available_anime_serial_nums(version)`：該版本動畫索引中的
  serial，排序且去重。不是圖片編號，也不代表每個動畫的全部素材都完整。
- `Session.acquire_animation_pixels(version, serial, direction, motion, max_bytes=...)`：
  一次取得對齊的動畫 RGBA 與 metadata。完整欄位、單位、錯誤與 ownership 見
  [Python API 說明](../tools/python/README.md#cpu-animation-preview-api-resource-preview-02)。
- 共用核心為 `modules/assets` 的 `AnimationDecoder`，只接收非 owning、單版本
  reader pointers。呼叫端負責序列化、維持 reader 存活，替換／銷毀前清除描述快取。
  這是 C++ 邊界；Python 不會取得這些 pointers。
- 原生 `AnimationBuilder` 留作小型 adapter，保留原來的 shared reader mutex。
  原生串流、預算、GPU 上傳／退休流程沿用。相同畫布規劃、色序、透明與像素組裝
  已移到共用核心，不保留兩份實作。
- Session 的 GIL 行為不變，不宣稱並行 server 安全。它最多快取一份動畫描述，
  不快取產生的動畫陣列；close／invalidate／trim 清除描述。NumPy capsule
  獨立持有像素 vector，陣列與 view 的生命週期不依賴 Session。

只先提供 Daytime palette（有內嵌 palette 時優先），與現行原生行為一致。
預設單次輸出上限 64 MiB，可指定 1..256 MiB；這不限制 caller 同時保留多份陣列，
也不是總 CPU 記憶體上限。開檔／索引／metadata 的成本在 pixel admission 之前。

## 本步驟的驗收方式

| 檢查 | 通過條件 | 對應查核 |
| --- | --- | --- |
| 動畫列舉 | serial 排序去重，invalidate 後可重新列舉，close 後拒絕呼叫 | Python 合成索引案例 |
| 像素與座標 | 兩張負／正 offset 的素材，組成預期畫布；top-down RGBA、透明來源與 padding 正確 | Python 合成 4×4 畫布＋既有 11 個 builder 案例 |
| 時間 | 101 ms 等非均整的總時長原樣返回，不經 GIF／整數逐格時間量化 | Python metadata 斷言；瀏覽器播放時間留給 03 |
| 失敗與預算 | 錯誤動作／方向、無效上限與 payload 不足被拒絕；重試成功且舊結果不受影響 | Python 失敗／重試案例；reader 已開啟後，重複拒絕不增加圖片 I/O |
| ownership | trim／invalidate／close 後舊陣列及 view 仍正確；最後一個 view 釋放後 wrapper 消失 | NumPy weakref＋現有 capsule／holder 測試 |
| 重複使用 | 100 次 decode／invalidate，不殘留輸出陣列或 reader ownership | Python weakref、reader snapshot、Windows temp file 清理 |
| 純資料邊界 | 未提供 reader／無效 request／取消都不需要 SDK、視窗或裝置 | 新增 SDK-free C++ suite，Windows/Linux ordinary/ASan CI |
| 原生相容 | 原有場景、動畫組裝與 GPU 資源清理仍通過 | 完整普通測試與相關 ASan 場景案例 |

這些是 02 的驗收，不把 HTTP 200 或舊 GIF 當作動畫視覺正確性的替代。
03 需再補瀏覽器播放、切換／取消、逐格與實際畫面驗收；不在本 PR 提前實作。

## 本機執行

在 Visual Studio x64 環境、已建好專案依賴及 Python／NumPy 的前提下：

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure
ctest --test-dir build/dev -R 'AnimationBuilderTest|AnimationDecoder\.|python_session_cache' --output-on-failure
```

Python 使用與 `cgl.pyd` 相同的 interpreter／架構，且 CMake 的
`Python3_EXECUTABLE` 指向它。SDK-free 建置指令見 [build-and-test](build-and-test.md)。
此處 Python extension 的完整專案建置仍需要桌面 build dependencies；
「API 使用不需 GPU」不代表完整 CMake 已拆成 SDK-free Python build。

ASan preset 預設不建 Python；若要同時驗證這個 binding，configure 時額外指定
`-DCGL_WITH_TOOLS=ON -DPython3_EXECUTABLE=<same interpreter>`，再建置／執行。
Windows Python 載入 ASan PYD 前，須把該編譯器對應的
`clang_rt.asan_dynamic-x86_64.dll` 放在 PYD 旁（或用 Python DLL search directory）。
單靠 PATH 不一定能讓 Python 的 extension loader 找到它。

## 實際素材查核與限制

Windows x64、Python 3.12.10、NumPy，素材 `C:/CrossGate-Legacy`：
動畫索引列出 806 個 Classic serial。本次逐格讀取原始 frame／palette／offset，
獨立組出預期 RGBA，與新 API 的**每一張完整影格**比對一致：

| Serial／動作（NORTH） | 影格 | 總時長 | 回傳 shape | RGBA bytes |
| --- | ---: | ---: | --- | ---: |
| 100000／STANDARD | 4 | 1000 ms | 4×75×44×4 | 52,800 |
| 100000／WALK | 6 | 1000 ms | 6×85×51×4 | 104,040 |
| 101406／ATTACK | 16 | 900 ms | 16×355×347×4 | 7,883,840 |

使用 metadata 的 duration、圖片解碼 shape 與 offset 計算預期畫布，包含 anchor
原點；沒有使用 GIF。invalidate／Session close 後，最後取得的 RGBA 仍可存取。
素材樣本只驗證上述 Classic clips，不宣稱所有版本、角色或畸形檔案皆已覆蓋。

測試與 CI 結果記錄在 PR。Windows ASan 不等同完整 leak detector；weakref、
reader 清理與有限次循環驗證 ownership，不宣稱已證明所有記憶體洩漏都不存在。
本步驟沒有修改繪製效能，也沒有 FPS 改善主張。素材、raw log 與輸出不提交。
