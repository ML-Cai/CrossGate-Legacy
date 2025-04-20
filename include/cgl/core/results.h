// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

namespace cgl {

// -----------------------------------------------------------------------------
// Results
// -----------------------------------------------------------------------------
#define Results_ENUM_LIST              \
    Results_X(Success)                 \
    Results_X(Fail)                    \
    Results_X(OutOfMemory)             \
    Results_X(InvalidFile)             \
    Results_X(InvalidArgs)             \
    Results_X(IndexNotExist)           \
    Results_X(UnknownError)

enum class Results : unsigned int {
#define Results_X(name) name,
    Results_ENUM_LIST
#undef Results_X
};

}   // namespace cgl
