// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <filesystem>
#include <memory>

namespace cgl {

struct Settings {
    /**
     * @brief This value will pointer to the resource director of Cross Gate
     * game, the struct of folder will be :
     *   crossGateDataRootDir
     *     |- bin
     *     |- map
     *     | ....
     */
    std::filesystem::path crossGateDataRootDir;
};

}   // namespace cgl
