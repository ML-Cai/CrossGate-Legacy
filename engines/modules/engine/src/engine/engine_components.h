// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string_view>
#include "cgl/common/states.h"

namespace cgl {
namespace component {

struct EngineState {
    EngineState() : state(cgl::StateTypes::UNKNOWN) {}
    cgl::StateTypes state;
    std::string lastError;
};

}   // namespace component
}   // namespace cgl
