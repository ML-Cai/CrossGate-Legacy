// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

// -----------------------------------------------------------------------------
// UNLIKELY marco
// This marco is used to indicate that the condition is unlikely to be true.
#if __cplusplus >= 202002L
#define UNLIKELY(x) (x) [[unlikely]]

#elif defined(__GNUC__) || defined(__clang__)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#elif defined(_MSC_VER)
#define UNLIKELY(x) (x)

#else

#define UNLIKELY(x) (x)
#endif
