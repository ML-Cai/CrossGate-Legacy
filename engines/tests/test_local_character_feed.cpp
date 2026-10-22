#include <gtest/gtest.h>
#include "map/local_character_feed.h"
#include <set>
TEST(LocalCharacterFeed, CompleteSnapshotAndExpandedInterest) {
    cgl::LocalCharacterFeed feed(512);
    const cgl::component::MapCamera camera{{420,305},1};
    const cgl::Viewport viewport(0,0,1280,720);
    const auto packet=feed.next(camera,viewport,840,610);
    ASSERT_EQ(packet.actors.size(),288);
    size_t visible=0,outer=0;
    std::set<uint64_t> ids;
    std::set<size_t> appearances;
    std::map<std::pair<int,int>,int> cells;
    for(const auto& actor:packet.actors) {
        EXPECT_TRUE(ids.insert(actor.id).second);
        EXPECT_LT(actor.appearance,512);
        EXPECT_TRUE(appearances.insert(actor.appearance).second);
        EXPECT_LE(++cells[std::make_pair(int(actor.cell.x),int(actor.cell.y))],10);
        const auto region=cgl::LocalCharacterFeed::region(actor.cell,camera,viewport);
        if(region==0) ++visible; else if(region==1) ++outer; else ADD_FAILURE();
    }
    EXPECT_EQ(visible,128);EXPECT_EQ(outer,160);
    const auto same=feed.next(camera,viewport,840,610);
    EXPECT_EQ(same.sequence,2);
    ASSERT_EQ(same.actors.size(),288);
    for(const auto& actor:same.actors) EXPECT_TRUE(ids.contains(actor.id));
    const auto fresh=feed.next(camera,viewport,840,610,true);
    for(const auto& actor:fresh.actors) EXPECT_FALSE(ids.contains(actor.id));
}
TEST(LocalCharacterFeed, RetainsWorldPositionsAsOuterActorsEnterScreen) {
    cgl::LocalCharacterFeed feed(512);
    cgl::component::MapCamera camera{{420,305},1};
    const cgl::Viewport viewport(0,0,1280,720);
    const auto first=feed.next(camera,viewport,840,610);
    camera.center.x+=2;camera.center.y+=2;
    const auto second=feed.next(camera,viewport,840,610);
    size_t entered=0;
    for(const auto& before:first.actors) for(const auto& after:second.actors) if(before.id==after.id) {
        EXPECT_DOUBLE_EQ(before.cell.x,after.cell.x);EXPECT_DOUBLE_EQ(before.cell.y,after.cell.y);
        if(cgl::LocalCharacterFeed::region(before.cell,{{420,305},1},viewport)==1 &&
           cgl::LocalCharacterFeed::region(after.cell,camera,viewport)==0) ++entered;
    }
    EXPECT_GT(entered,0);
    EXPECT_EQ(second.actors.size(),288);
}
TEST(LocalCharacterFeed, EdgesAndTinyMapStayBounded) {
    cgl::LocalCharacterFeed feed(2);
    const cgl::Viewport viewport(0,0,1280,720);
    for(const auto center:std::vector<cgl::MapPoint>{{0,0},{839,0},{0,609},{839,609}}) {
        const auto packet=feed.next({center,1},viewport,840,610);
        for(const auto& actor:packet.actors) {
            EXPECT_GE(actor.cell.x,0);EXPECT_LT(actor.cell.x,840);
            EXPECT_GE(actor.cell.y,0);EXPECT_LT(actor.cell.y,610);
        }
    }
    cgl::LocalCharacterFeed tiny(1);
    EXPECT_EQ(tiny.next({{0,0},1},viewport,1,1).actors.size(),10);
    EXPECT_TRUE(tiny.next({{0,0},1},viewport,0,0).actors.empty());
    EXPECT_THROW(cgl::LocalCharacterFeed(0),std::invalid_argument);
}
