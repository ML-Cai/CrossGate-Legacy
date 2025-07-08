// -----------------------------------------------------------------------------
//   The MIT License (MIT)
//
//   Copyright (c) 2024 MengLun,Cai
//
//   All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#include <stdint.h>

namespace cgl {

enum class Results : uint32_t;
enum class CrossGateVersion : uint8_t;
enum class GraphicsResourceIndexTypes : uint8_t;

const char* GetString(const cgl::Results& type);
const char* GetString(const cgl::CrossGateVersion& type);
const char* GetString(const cgl::GraphicsResourceIndexTypes& type);

}   // namespace cgl
