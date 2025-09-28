// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string_view>
#include <ostream>
#include "cgl/common/results.h"
#include "cgl/common/version.h"
#include "cgl/common/motion.h"
#include "cgl/common/direction.h"
#include "cgl/common/assets.h"
#include "cgl/common/input.h"

namespace cgl {

std::string_view ToStr(const cgl::Results& type);
std::string_view ToStr(const cgl::CrossGateVersion& type);
std::string_view ToStr(const cgl::MotionTypes& type);
std::string_view ToStr(const cgl::DirectionTypes& type);
std::string_view ToStr(const cgl::EnvironmentPaletteTypes& type);
std::string_view ToStr(const cgl::GraphicsResourceSerialNumTypes& type);
std::string_view ToStr(const cgl::MouseButtonTypes& type);
std::string_view ToStr(const cgl::InputActionTypes& type);

std::string ToStr(const cgl::GraphicsResourceSerialNum& type);
std::string ToStr(const cgl::AnimeResourceSerialNum& type);

template <typename T>
requires requires(T v) {
    cgl::ToStr(v);
}
inline std::ostream& operator<<(std::ostream& os, T v) {
    return os << cgl::ToStr(v);
}

}   // namespace cgl
