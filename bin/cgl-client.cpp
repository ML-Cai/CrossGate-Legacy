// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/engine/engine.h"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <string_view>

// -----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    auto settings = std::filesystem::absolute(argv[0]).parent_path() / "settings.ini";
    uint32_t frameLimit = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--help") {
            std::cout << "Usage: cgl-client [--settings path] [--frames count]\n";
            return 0;
        }
        if (arg == "--settings" && i + 1 < argc) {
            settings = std::filesystem::absolute(argv[++i]);
        } else if (arg == "--frames" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            const auto result = std::from_chars(value.data(), value.data() + value.size(), frameLimit);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || !frameLimit) {
                std::cerr << arg << " requires a positive integer\n";
                return 1;
            }
        } else {
            std::cerr << "Invalid argument: " << arg << " (see --help)\n";
            return 1;
        }
    }
    auto engine = cgl::IEngine::create(settings.string());
    return engine && engine->run(frameLimit) ? 0 : 1;
}
