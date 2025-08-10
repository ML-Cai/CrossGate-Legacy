// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <unordered_map>
#include <vector>
#include <utility>
#include <functional>
#include "cgl/core/version.h"
#include "cgl/character/direction_types.h"
#include "cgl/character/motion_types.h"

namespace cgl {

// -----------------------------------------------------------------------------
// Anime Index/Data related section
// -----------------------------------------------------------------------------
struct AnimeResourceSerialNum {
    cgl::CrossGateVersion version;
    uint32_t value;
};


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
