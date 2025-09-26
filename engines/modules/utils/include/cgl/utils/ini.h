// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string>
#include <unordered_map>

namespace cgl {

using INI = std::unordered_map<
                std::string,    // section name
                std::unordered_map<std::string,     // key
                                   std::string> >;  // value

bool LoadIniFile(const std::string& filename, cgl::INI* pIni);

}   // namespace cgl
