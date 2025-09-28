// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string_view>
#include "cgl/common/engine.h"

namespace cgl {
namespace component {

struct EngineState {
    EngineState() : state(cgl::EngineStateTypes::UNKNOWN) { }

    EngineState(cgl::EngineStateTypes state_, std::string last_error_message_)
        : state(state), last_error_message(last_error_message_) { }

    cgl::EngineStateTypes state;
    std::string last_error_message;
};

}   // namespace component
}   // namespace cgl
