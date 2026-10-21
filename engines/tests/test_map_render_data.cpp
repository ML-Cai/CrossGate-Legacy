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

TEST(MapRenderTest, MultipleAtlasPagesPreserveEveryImageAndPadding) {
    const std::vector<cgl::MapImageSize> images{{1,5,5},{2,4,2},{3,5,5},{4,4,2}};
    const auto pages = cgl::PlanMapAtlases(images,8,8,4);
    ASSERT_TRUE(pages);
    ASSERT_GT(pages->size(),1);
    std::vector<uint16_t> found;
    for(const auto& page : *pages) {
        EXPECT_LE(page.width,8); EXPECT_LE(page.height,8);
        ASSERT_FALSE(page.regions.empty());
        for(const auto& region : page.regions) {
            found.push_back(region.id);
            const auto image = std::find_if(images.begin(),images.end(),[&](const auto& i){return i.id==region.id;});
            ASSERT_NE(image,images.end());
            EXPECT_EQ(region.source.width,image->width); EXPECT_EQ(region.source.height,image->height);
            EXPECT_GE(region.source.x,1); EXPECT_GE(region.source.y,1);
            EXPECT_LT(region.source.x+region.source.width,page.width);
            EXPECT_LT(region.source.y+region.source.height,page.height);
            for(const auto& other : page.regions) if(other.id!=region.id)
                EXPECT_TRUE(region.source.x+region.source.width<other.source.x ||
                    other.source.x+other.source.width<region.source.x ||
                    region.source.y+region.source.height<other.source.y ||
                    other.source.y+other.source.height<region.source.y);
        }
    }
    std::sort(found.begin(),found.end());
    EXPECT_EQ(found,(std::vector<uint16_t>{1,2,3,4}));
    auto reversed=images; std::reverse(reversed.begin(),reversed.end());
    const auto again=cgl::PlanMapAtlases(reversed,8,8,4); ASSERT_TRUE(again);
    ASSERT_EQ(again->size(),pages->size());
    for(size_t i=0;i<pages->size();++i) {
        ASSERT_EQ((*pages)[i].regions.size(),(*again)[i].regions.size());
        for(size_t j=0;j<(*pages)[i].regions.size();++j) {
            const auto& a=(*pages)[i].regions[j];const auto& b=(*again)[i].regions[j];
            EXPECT_EQ(a.id,b.id); EXPECT_EQ(a.source.x,b.source.x); EXPECT_EQ(a.source.y,b.source.y);
        }
    }
}

TEST(MapRenderTest, MultipleAtlasLimitsRejectBeforePixelAllocation) {
    EXPECT_TRUE(cgl::PlanMapAtlases({})->empty());
    EXPECT_FALSE(cgl::PlanMapAtlases({{1,5,5},{2,5,5}},8,8,1));
    EXPECT_TRUE(cgl::PlanMapAtlases({{1,5,5},{2,5,5}},8,8,2));
    EXPECT_FALSE(cgl::PlanMapAtlases({{1,7,1}},8,8));
    EXPECT_FALSE(cgl::PlanMapAtlases({{1,1,7}},8,8));
    EXPECT_FALSE(cgl::PlanMapAtlases({{1,0,1}}));
    EXPECT_FALSE(cgl::PlanMapAtlases({{1,UINT32_MAX,1}}));
    EXPECT_FALSE(cgl::PlanMapAtlases({{1,2,2},{1,1,1}}));
    EXPECT_FALSE(cgl::PlanMapAtlases({},2049,4096));
    EXPECT_FALSE(cgl::PlanMapAtlases({},2048,4097));
    EXPECT_FALSE(cgl::PlanMapAtlases({},2048,4096,0));
    EXPECT_FALSE(cgl::PlanMapAtlases({},2048,4096,9));
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

// Map 2100 house 11283: 4x6 cells, image offset (-210,-437), 372x460.
// The actor is outside the front/left wall but has a smaller anchor depth.
TEST(MapRenderTest, HouseFrontAndSidesUseFootprintInsteadOfSingleAnchor) {
    const cgl::MapDrawItem house{{50,61},nullptr,{0,0,372,460},0,-210,-437,1,4,6};
    auto actor=cgl::MapDrawItem{{49,58},nullptr,{0,0,48,84},0,-24,-70,2,0,0};
    ASSERT_TRUE(cgl::MapDrawLess(actor,house)); // Old ordering incorrectly hides actor.
    for(const cgl::MapPoint cell : {cgl::MapPoint{49,58}, {52,62}}) {
        actor.cell=cell;
        std::vector<cgl::MapDrawItem> items{actor,house};
        EXPECT_EQ(cgl::SortMapDrawItems(items),0);
        EXPECT_EQ(items.back().stableId,actor.stableId);
    }
    for(const cgl::MapPoint cell : {cgl::MapPoint{54,58}, {52,55}}) {
        actor.cell=cell;
        std::vector<cgl::MapDrawItem> items{house,actor};
        EXPECT_EQ(cgl::SortMapDrawItems(items),0);
        EXPECT_EQ(items.back().stableId,house.stableId);
    }
    const auto footprint=cgl::GroundFootprint(house);
    EXPECT_DOUBLE_EQ(footprint.minX,49.5);EXPECT_DOUBLE_EQ(footprint.maxX,53.5);
    EXPECT_DOUBLE_EQ(footprint.minY,55.5);EXPECT_DOUBLE_EQ(footprint.maxY,61.5);
}

TEST(MapRenderTest, SingleCellAndNonOverlappingSpritesKeepAnchorOrdering) {
    cgl::MapDrawItem tree{{50,61},nullptr,{0,0,64,120},0,-32,-100,1};
    cgl::MapDrawItem actor{{49,58},nullptr,{0,0,48,84},0,-24,-70,2,0,0};
    std::vector<cgl::MapDrawItem> items{tree,actor};
    EXPECT_EQ(cgl::SortMapDrawItems(items),0);
    EXPECT_EQ(items.front().stableId,actor.stableId);
    tree.tileX=4;tree.tileY=6;tree.offsetX=10000; // No visible overlap.
    items={tree,actor};cgl::SortMapDrawItems(items);
    EXPECT_EQ(items.front().stableId,actor.stableId);
}

TEST(MapRenderTest, FootprintOrderingIsStableAcrossInputOrderAndRepeatedFrames) {
    std::vector<cgl::MapDrawItem> items{
        {{50,61},nullptr,{0,0,372,460},0,-210,-437,1,4,6},
        {{49,58},nullptr,{0,0,48,84},0,-24,-70,2,0,0},
        {{54,58},nullptr,{0,0,48,84},0,-24,-70,3,0,0},
        {{52,62},nullptr,{0,0,48,84},0,-24,-70,4,0,0}};
    cgl::SortMapDrawItems(items);
    std::vector<uint64_t> expected;
    for(const auto& item:items) expected.push_back(item.stableId);
    for(int frame=0;frame<8;++frame) {
        std::rotate(items.begin(),items.begin()+1,items.end());
        cgl::SortMapDrawItems(items);
        for(size_t i=0;i<items.size();++i) EXPECT_EQ(items[i].stableId,expected[i]);
    }
    std::vector<cgl::MapDrawItem> empty;EXPECT_EQ(cgl::SortMapDrawItems(empty),0);
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

TEST(MapRenderTest, DensePointActorsKeepStableBaselineOrder) {
    std::vector<cgl::MapDrawItem> actors;
    for(uint64_t i=0;i<6000;++i) actors.push_back({{double(i%30),double((i/30)%20)},nullptr,{0,0,64,100},0,-32,-90,i,0,0});
    std::reverse(actors.begin(),actors.end());
    EXPECT_EQ(cgl::SortMapDrawItems(actors),0);
    EXPECT_TRUE(std::is_sorted(actors.begin(),actors.end(),cgl::MapDrawLess));
}
TEST(MapRenderTest, DenseActorsRespectOneLargeBuildingPrecedence) {
    std::vector<cgl::MapDrawItem> items{{{4,5},nullptr,{0,0,300,350},0,-150,-300,999,3,4}};
    for(uint64_t i=0;i<169;++i) items.push_back({{double(i%13),double(i/13)},nullptr,{0,0,64,120},0,-32,-110,i,0,0});
    EXPECT_EQ(cgl::SortMapDrawItems(items),0);
    const auto building=std::find_if(items.begin(),items.end(),[](auto& item){return item.stableId==999;});
    ASSERT_NE(building,items.end());
    auto bp=cgl::MapToWorld(building->cell);auto bf=cgl::GroundFootprint(*building);
    for(auto it=items.begin();it!=items.end();++it) {
        if(it==building) continue;
        auto p=cgl::MapToWorld(it->cell);auto f=cgl::GroundFootprint(*it);
        if(p.x-32>=bp.x+150 || p.x+32<=bp.x-150 || p.y-110>=bp.y+50 || p.y+10<=bp.y-300) continue;
        const bool buildingBehind=bf.minX>=f.maxX || bf.maxY<=f.minY;
        const bool actorBehind=f.minX>=bf.maxX || f.maxY<=bf.minY;
        if(buildingBehind!=actorBehind) EXPECT_EQ(building<it,buildingBehind);
    }
}
