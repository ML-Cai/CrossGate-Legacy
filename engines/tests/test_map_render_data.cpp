#include "map/map_atlas.h"
#include "map/map_render_data.h"
#include <gtest/gtest.h>

namespace {
TEST(MapRenderTest, AtlasPreservesImageSizeAndSeparatesRegions) {
    std::vector<cgl::MapImageSize> images{{1,64,47}, {2,580,622}, {3,188,299}, {4,32,19}};
    const auto plan = cgl::PlanMapAtlas(images);
    ASSERT_TRUE(plan);
    ASSERT_EQ(plan->regions.size(), images.size());
    for (const auto& region : plan->regions) {
        const auto found = std::find_if(images.begin(), images.end(), [&](const auto& image) { return image.id == region.id; });
        ASSERT_NE(found, images.end());
        EXPECT_EQ(region.source.width, found->width);
        EXPECT_EQ(region.source.height, found->height);
        EXPECT_GE(region.source.x, 1);
        EXPECT_GE(region.source.y, 1);
        EXPECT_LT(region.source.x + region.source.width, plan->width);
        EXPECT_LT(region.source.y + region.source.height, plan->height);
        for (const auto& other : plan->regions) if (region.id != other.id)
            EXPECT_TRUE(region.source.x + region.source.width < other.source.x ||
                other.source.x + other.source.width < region.source.x ||
                region.source.y + region.source.height < other.source.y ||
                other.source.y + other.source.height < region.source.y);
    }
    std::reverse(images.begin(), images.end());
    const auto reversed = cgl::PlanMapAtlas(images);
    ASSERT_TRUE(reversed);
    for (size_t i = 0; i < images.size(); ++i) {
        EXPECT_EQ(plan->regions[i].id, reversed->regions[i].id);
        EXPECT_EQ(plan->regions[i].source.x, reversed->regions[i].source.x);
        EXPECT_EQ(plan->regions[i].source.y, reversed->regions[i].source.y);
    }
}

TEST(MapRenderTest, AtlasRejectsInvalidOrUnpackableImagesBeforeAllocation) {
    EXPECT_TRUE(cgl::PlanMapAtlas({})->regions.empty());
    EXPECT_FALSE(cgl::PlanMapAtlas({{1,0,1}}));
    EXPECT_FALSE(cgl::PlanMapAtlas({{1,UINT32_MAX,1}}));
    EXPECT_FALSE(cgl::PlanMapAtlas({{1,1,UINT32_MAX}}));
    EXPECT_FALSE(cgl::PlanMapAtlas({{1,5,5}, {2,5,5}}, 8, 8));
    const auto shelves = cgl::PlanMapAtlas({{1,4,2}, {2,4,2}}, 8, 8);
    ASSERT_TRUE(shelves);
    EXPECT_EQ(shelves->regions[0].source.y, 1);
    EXPECT_EQ(shelves->regions[1].source.y, 5);
}

TEST(MapRenderTest, PainterOrderUsesGroundAnchorsAndDeterministicTies) {
    cgl::MapDrawItem building{{10,10}, nullptr, {0,0,580,622}, 0, -200, -600, 100};
    cgl::MapDrawItem actor{{10,9}, nullptr, {0,0,64,100}, 0, -32, -100, (1ull << 32) | 1};
    EXPECT_TRUE(cgl::MapDrawLess(actor, building)); // Actor behind the building.
    actor.cell.y = 11;
    EXPECT_TRUE(cgl::MapDrawLess(building, actor)); // Actor in front, despite image top.
    actor.cell = building.cell;
    EXPECT_TRUE(cgl::MapDrawLess(building, actor)); // Equal anchor: stable object before actor.
    EXPECT_FALSE(cgl::MapDrawLess(actor, actor));
    const cgl::MapDrawItem left{{9,9}, nullptr, {}, 0, 0, 0, 999};
    EXPECT_TRUE(cgl::MapDrawLess(left, building));
    std::vector<cgl::MapDrawItem> items{actor, building, left};
    std::sort(items.begin(), items.end(), cgl::MapDrawLess);
    EXPECT_EQ(items[0].stableId, left.stableId);
    EXPECT_EQ(items[1].stableId, building.stableId);
    EXPECT_EQ(items[2].stableId, actor.stableId);
}

TEST(MapRenderTest, LargeImagesRemainVisibleWithOffscreenGroundAnchors) {
    const cgl::Viewport viewport{0,0,800,600};
    EXPECT_TRUE(cgl::MapImageVisible({790,-400}, 580, 622, 1, viewport));
    EXPECT_TRUE(cgl::MapImageVisible({-500,100}, 580, 622, 1, viewport));
    EXPECT_FALSE(cgl::MapImageVisible({800,0}, 580, 622, 1, viewport));
    EXPECT_FALSE(cgl::MapImageVisible({-580,0}, 580, 622, 1, viewport));
    EXPECT_FALSE(cgl::MapImageVisible({0,-622}, 580, 622, 1, viewport));
}

TEST(MapRenderTest, ClassicPlaceholdersAreFilteredOnlyForTheirLayer) {
    for (uint16_t id : {0,2}) {
        EXPECT_FALSE(cgl::IsClassicMapImage(id, false));
        EXPECT_FALSE(cgl::IsClassicMapImage(id, true));
    }
    for (uint16_t id : {200,251,265}) {
        EXPECT_TRUE(cgl::IsClassicMapImage(id, false));
        EXPECT_FALSE(cgl::IsClassicMapImage(id, true));
    }
    EXPECT_TRUE(cgl::IsClassicMapImage(10058, true));
    EXPECT_TRUE(cgl::IsClassicMapImage(266, true));
}
}
