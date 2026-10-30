# Resource Preview：原生／Python 動畫調色盤一致性

進行 #98 的原生對照檢查時，發現兩個 caller 的 reader 適配不同：
Python Session 已為 PUK2_V2／PUK3_V1 提供 PUK2_V1 的動畫 palette 來源，
原生 AnimationBuilder 只傳入該角色版本的 frame readers。
對使用共用動畫 palette 的素材，原生可能在錯誤的檔案查 marker，最後採用 Daytime 而顯示不同顏色。

## 修正

- 兩個 caller 使用共用 `AnimationPaletteVersion` 規則：PUK2_V2／PUK3_V1 的 marker 來源為 PUK2_V1，
  其他版本維持原版本。角色描述、圖像 ID、frame pixels 仍從角色本身的版本讀取。
- 原生適配層傳入正確的非 owning reader pointers；使用時仍持有原本的 reader query mutex。
  不新增 reader、執行緒或長期持有來源，不改變 CPU／GPU 快取上限與生命週期。
- 跨版本 palette readers 不完整時，在 decode 階段明確報錯；metadata-only requiredBytes
  不讀取 palette 或 pixels。正確來源存在但沒有該角色的 marker，仍採用既有 Daytime fallback。
- 像素顏色優先序不變：frame 內嵌 palette、動畫 marker palette、Daytime。

## 測試

合成檔使用與 Daytime 明顯不同的 BGR 顏色，經正式 C++ readers 解碼；不提交遊戲素材。

- 原生 Builder：PUK2_V2 與 PUK3 皆從 PUK2_V1 取色，保留 frame 版本、offset、透明度、duration；
  Classic 仍使用原本的顏色。
- 缺少 info／data reader、短 palette 會失敗；補回或換回有效來源可以重試，先前傳回的 owned RGBA 不變。
- Python Session：相同跨版本來源下的逐格與整段 RGBA 相等；Session 關閉後已傳回的像素仍可讀取。
- SDK-free Python 測試專案現在也編譯正式原生 AnimationBuilder 與同一份 fixture tests。
  Windows／Linux binding CI 初始化既有 pinned googletest，並在 adapter／fixture 修改時觸發。

```powershell
git submodule update --init third_party/googletest
cmake -S tools/python/tests/headless -B build/python-headless -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/python-headless --parallel 2
ctest --test-dir build/python-headless --output-on-failure
```

須先啟用編譯器環境並使用已安裝 NumPy 的 Python；需要時指定 `-DPython3_EXECUTABLE=...`。
ASan 依現有 `CGL_ENABLE_ASAN` 選項使用另一個 build 目錄；Windows 載入 instrumented binding 時
仍須讓 ASan runtime DLL 可被 Python 找到。

## 驗收範圍

2026-10-30 本機 Windows：一般與 ASan 的 SDK-free 測試各 17/17 個 CTest 項目通過
（13 個原生 Builder tests、4 組 Python binding tests，其中 streaming 為 7 cases）。
實際八版本矩陣通過：817 張 PNG、9 組鏡像方向對、128 次切換；991 次 plan owner 檢查、
frame owners 釋放，關閉後 reader 為零。這些是正式 CPU readers 的結果，未啟動 Vulkan。

這次驗證的是原生與工具進入同一 CPU decoder 前的 reader 來源與像素結果，
尚未完成原生 Vulkan 畫面與網頁的獨立視覺對照，也不以共用 CPU 結果代填 GPU 驗收。
完整 browser 操作記憶體與 GPU 對照仍由 #98／04 追蹤。
