// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string>
#include "cgl/common/states.h"
#include "cgl/settings/settings.h"

namespace cgl {
namespace component {

struct EngineState {
    cgl::StateTypes state;
    std::string lastError;
    cgl::Settings::Ptr settings;
};

}   // namespace component
}   // namespace cgl
