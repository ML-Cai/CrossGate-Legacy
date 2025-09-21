// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <stdint.h>
#include <memory>
#include <array>
#include <optional>
#include <string_view>
#include <vector>
#include <unordered_map>
#include "cgl/common/version.h"
#include "cgl/common/direction_types.h"
#include "cgl/common/motion_types.h"


namespace cgl {

// -----------------------------------------------------------------------------
// Palette data
// -----------------------------------------------------------------------------
#pragma pack(1)
union PaletteData {
    struct BGRColorVector {
        uint8_t B;
        uint8_t G;
        uint8_t R;
    } BGR;
    uint8_t data[3];
};
#pragma pack()

using PaletteData256 = std::array<PaletteData, 256>;

// EnvironmentPaletteTypes enum
#define EnvironmentPaletteTypes_ENUM_LIST \
    CGL_X(Daytime)                        \
    CGL_X(Evening)                        \
    CGL_X(Night)                          \
    CGL_X(EarlyMorning)                   \
    CGL_X(Unknown)                        \

enum class EnvironmentPaletteTypes : uint8_t {
#define CGL_X(name) name,
    EnvironmentPaletteTypes_ENUM_LIST
#undef CGL_X
};

std::string ToStr(const cgl::EnvironmentPaletteTypes& type);

// -----------------------------------------------------------------------------
// Graphic Index/Data related section
// -----------------------------------------------------------------------------

// GraphicsResourceSerialNumTypes
#define GRAPHICS_ASSETS_SERIAL_NUM_TYPE_ENUM_LIST    \
    CGL_X(GraphicsSerialNum)           \
    CGL_X(MapSerialNum)                \

enum class GraphicsResourceSerialNumTypes : uint8_t {
#define CGL_X(name) name,
    GRAPHICS_ASSETS_SERIAL_NUM_TYPE_ENUM_LIST
#undef CGL_X
};

std::string ToStr(const cgl::GraphicsResourceSerialNumTypes& type);

// GraphicsResourceSerialNum
struct GraphicsResourceSerialNum {
    using Key = uint64_t;

    cgl::GraphicsResourceSerialNumTypes type;
    cgl::CrossGateVersion version;
    uint32_t value;

    bool operator==(const cgl::GraphicsResourceSerialNum& other) const {
        return (type == other.type) &&
               (value == other.value) &&
               (version == other.version);
    }

    bool operator!=(const cgl::GraphicsResourceSerialNum& other) const {
        return (type != other.type) ||
               (value != other.value) ||
               (version != other.version);
    }

    bool operator<(const cgl::GraphicsResourceSerialNum& other) const {
        if (this->type != other.type) {
            return false;
        }
        return key() < other.key();
    }

    GraphicsResourceSerialNum::Key key() const noexcept {
        return static_cast<uint64_t>(version) * INT32_MAX +
               static_cast<uint64_t>(value);
    }
};

std::string ToStr(const cgl::GraphicsResourceSerialNum& type);

// GraphicsResourceIndex
struct GraphicsResourceInfo {
    cgl::CrossGateVersion version;
    cgl::GraphicsResourceSerialNum gfxBasedsIdx;
    cgl::GraphicsResourceSerialNum mapBasedsIdx;
    uint32_t dataOffset;
    uint32_t dataSize;
    int32_t offsetX;
    int32_t offsetY;
    int32_t width;
    int32_t height;
};


// Graphic resource data
struct GraphicsResourceData {
    cgl::CrossGateVersion version;
    int32_t width;
    int32_t height;
    std::unique_ptr<uint8_t[]> pData;
    uint8_t* pPaletteData;
};



// -----------------------------------------------------------------------------
// Anime Index/Data related section
// -----------------------------------------------------------------------------
struct AnimeResourceSerialNum {
    cgl::CrossGateVersion version;
    uint32_t value;
};

std::string ToStr(const cgl::AnimeResourceSerialNum& type);

struct AnimeResourceInfo {
    AnimeResourceSerialNum serialNum;
    uint32_t dataOffset;
    uint16_t motionCount;
};


using AnimeDataMapKey = std::pair<cgl::DirectionTypes, cgl::MotionTypes>;

struct AnimeDataMapKeyHash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1, T2>& p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);      // combine hashes
    }
};


struct AnimeMotionDesc {
    cgl::CrossGateVersion version;
    cgl::DirectionTypes direction;
    cgl::MotionTypes motion;
    uint32_t duration;
    std::vector<uint32_t> motionGraphicsSerialNums;
};


struct AnimeResourceData {
    AnimeResourceSerialNum serialNum;

    std::unordered_map<
        cgl::AnimeDataMapKey,
        cgl::AnimeMotionDesc,
        cgl::AnimeDataMapKeyHash> motionMap;
};


}   // namespace cgl
