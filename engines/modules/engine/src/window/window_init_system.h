// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <memory>

struct GLFWwindow;

namespace cgl {

class ECSCore;

namespace component {
struct WindowCreateInfo;
}   // namespace component

class WindowInitSystem {
public:
    WindowInitSystem();

    ~WindowInitSystem() = default;

    void update(cgl::ECSCore* pECS);

 private:
    bool createWindow(const cgl::component::WindowCreateInfo* pCreateInfo);
    GLFWwindow *window_;
};

}   // namespace cgl
