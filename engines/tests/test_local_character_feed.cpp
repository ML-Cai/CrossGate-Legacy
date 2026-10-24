#include <gtest/gtest.h>
#include "map/local_character_feed.h"
#include "map/server_demo_refresh.h"
#include "map/streaming_stress.h"
#include <limits>
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

TEST(ServerDemoRefresh, TimeAndExplicitReplacement) {
    const cgl::component::MapCamera camera{{420,305},1};
    const cgl::Viewport viewport(0,0,1280,720);
    EXPECT_FALSE(cgl::ServerDemoRefreshDue(camera,camera.center,viewport,1.999,false));
    EXPECT_TRUE(cgl::ServerDemoRefreshDue(camera,camera.center,viewport,2,false));
    EXPECT_TRUE(cgl::ServerDemoRefreshDue(camera,camera.center,viewport,0,true));
    EXPECT_FALSE(cgl::ServerDemoRefreshDue(camera,camera.center,{0,0,0,720},3,true));
}
TEST(ServerDemoRefresh, MovementThresholdUsesScreenDistanceAndZoom) {
    const cgl::Viewport viewport(0,0,1280,720);
    const cgl::MapPoint previous{420,305};
    // Both map axes together move horizontally: 64 screen pixels per step.
    EXPECT_FALSE(cgl::ServerDemoRefreshDue({{422.5,307.5},1},previous,viewport,0,false));
    EXPECT_TRUE(cgl::ServerDemoRefreshDue({{422.51,307.51},1},previous,viewport,0,false));
    EXPECT_TRUE(cgl::ServerDemoRefreshDue({{421.3,306.3},2},previous,viewport,0,false));
    EXPECT_FALSE(cgl::ServerDemoRefreshDue({{422.51,307.51},.5},previous,viewport,0,false));
    // Opposite map axes move vertically, where the half-margin is 90 pixels.
    EXPECT_FALSE(cgl::ServerDemoRefreshDue({{421,304},1},previous,viewport,0,false));
    EXPECT_TRUE(cgl::ServerDemoRefreshDue({{422,303},1},previous,viewport,0,false));
}

TEST(StreamingStress, ValidatesLimitsAndSchedulesExactFrameBoundaries) {
    EXPECT_NO_THROW(cgl::ValidateDemoLimits(16,512,512,2048));
    EXPECT_THROW(cgl::ValidateDemoLimits(15,128,160,512),std::invalid_argument);
    EXPECT_THROW(cgl::ValidateDemoLimits(512,1024,1,2048),std::invalid_argument);
    EXPECT_THROW(cgl::ValidateDemoLimits(512,128,160,128),std::invalid_argument);
    EXPECT_THROW(cgl::StreamingStress(0,2,3,20),std::invalid_argument);
    EXPECT_THROW(cgl::StreamingStress(8,0,3,20),std::invalid_argument);
    EXPECT_THROW(cgl::StreamingStress(8,2,0,20),std::invalid_argument);
    EXPECT_THROW(cgl::StreamingStress(8,2,3,std::numeric_limits<double>::quiet_NaN()),std::invalid_argument);
    cgl::StreamingStress route(8,2,3,20);
    EXPECT_EQ(route.frames(),8);
    EXPECT_THROW(route.at(0),std::out_of_range);
    EXPECT_THROW(route.at(9),std::out_of_range);
    EXPECT_EQ(route.at(1).x,1);EXPECT_EQ(route.at(2).x,1);EXPECT_EQ(route.at(3).y,1);
    EXPECT_EQ(route.at(5).x,-1);EXPECT_EQ(route.at(7).y,-1);EXPECT_DOUBLE_EQ(route.at(8).seconds,.02);
    for(uint64_t i=1;i<=8;++i) EXPECT_EQ(route.at(i).replace,i==3 || i==6);
}
TEST(LocalCharacterFeed, Configurable1024ActorChurnIsDistinctBoundedAndReproducible) {
    cgl::LocalCharacterFeed first(2048,42,512,512),second(2048,42,512,512);
    cgl::component::MapCamera camera{{420,305},1};const cgl::Viewport viewport(0,0,1280,720);
    for(int step=0;step<16;++step) {
        camera.center.x+=1;camera.center.y+=.5;
        const auto& a=first.next(camera,viewport,840,610,step%3==0);
        const auto& b=second.next(camera,viewport,840,610,step%3==0);
        ASSERT_EQ(a.actors.size(),1024);ASSERT_EQ(b.actors.size(),a.actors.size());EXPECT_EQ(a.sequence,b.sequence);
        std::set<size_t> appearances;std::map<std::pair<int,int>,int> cells;size_t visible=0;
        for(size_t i=0;i<a.actors.size();++i) {
            const auto& actor=a.actors[i];const auto& other=b.actors[i];
            EXPECT_EQ(actor.id,other.id);EXPECT_EQ(actor.appearance,other.appearance);
            EXPECT_DOUBLE_EQ(actor.cell.x,other.cell.x);EXPECT_DOUBLE_EQ(actor.cell.y,other.cell.y);
            EXPECT_TRUE(appearances.insert(actor.appearance).second);
            EXPECT_LE(++cells[std::make_pair(int(actor.cell.x),int(actor.cell.y))],10);
            if(cgl::LocalCharacterFeed::region(actor.cell,camera,viewport)==0) ++visible;
        }
        EXPECT_EQ(visible,512);
    }
    EXPECT_THROW(cgl::LocalCharacterFeed(2048,42,1024,1),std::invalid_argument);
}
