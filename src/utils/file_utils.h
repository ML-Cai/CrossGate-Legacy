// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <assert.h>
#include <fstream>
#include <filesystem>
#include <optional>
#include <utility>
#include <string>
#include "cgl/core/results.h"

namespace cgl {

//------------------------------------------------------------------------------
struct FileOpenInfo {
    std::filesystem::path path;
    std::optional<std::ifstream> stream;
    cgl::Results result;
    std::string errorMsg;
};

//------------------------------------------------------------------------------
cgl::FileOpenInfo TryOpenBinaryFile(const std::filesystem::path& filePath) {
    std::ifstream file;

    try {
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        file.open(filePath, std::ios::in | std::ios::binary);
    } catch (const std::ios_base::failure& e) {
        return { filePath, std::nullopt, cgl::Results::InvalidFile, e.what() };
    }

    return { filePath, std::move(file), cgl::Results::Success, {} };
}

//------------------------------------------------------------------------------
bool IsFileOpen(const cgl::FileOpenInfo& openInfo) {
    return openInfo.stream.has_value() &&
           openInfo.stream.value().is_open();
}

//------------------------------------------------------------------------------
size_t GetFileSize(cgl::FileOpenInfo* pOpenInfo) {
    assert(pOpenInfo->stream.has_value());

    auto& f = pOpenInfo->stream.value();
    f.seekg(0, f.end);
    size_t bufferSize = static_cast<size_t>(f.tellg());
    f.seekg(0, f.beg);

    return bufferSize;
}


}   // namespace cgl
