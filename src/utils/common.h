// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#if __cplusplus >= 202002L
#define UNLIKELY(x) (x) [[unlikely]]

#elif defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#elif defined(_MSC_VER)

#define UNLIKELY(x) (x) // MSVC 沒有直接等價的 __builtin_expect，但其最佳化器通常能處理

#else

#define UNLIKELY(x) (x)
#endif
