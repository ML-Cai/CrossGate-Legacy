// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <sstream>
#include "cgl/common/states.h"
#include "cgl/trace/logger.h"

namespace cgl {

template <typename T, typename... Args>
void RaiseError(T* pState, Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    pState->state = cgl::StateTypes::ERROR;
    pState->lastError = oss.str();

    LOGE("Error: " << pState->lastError);
}

}   // namespace
