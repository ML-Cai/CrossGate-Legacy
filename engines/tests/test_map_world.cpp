#include <gtest/gtest.h>
#include "map/map_world_update.h"
#include "map/map_world_renderer.h"
#include <stdexcept>
namespace {
struct Draw {int id;uint32_t frame;};
class RecordingSprite final:public cgl::graphics::ISpriteRenderer {
public:
    RecordingSprite(std::vector<Draw>& calls,int id):calls_(calls),id_(id) {}
    cgl::graphics::SpriteUploadState pollUpload() override {++polls;return cgl::graphics::SpriteUploadState::Ready;}
    void draw(cgl::graphics::ICommandBuffer*,const cgl::Viewport&,uint32_t frame,float,float,float) override {record(frame);}
    void drawRegion(cgl::graphics::ICommandBuffer*,const cgl::Viewport&,uint32_t frame,cgl::graphics::SpriteRegion,float,float,float) override {record(frame);}
    int polls=0;bool fail=false;
private:
    void record(uint32_t frame) {if(fail) throw std::runtime_error("record failure");calls_.push_back({id_,frame});}
    std::vector<Draw>& calls_;int id_;
};
struct Fixture {
    std::vector<Draw> calls;
    cgl::ECSCore world;
    cgl::MapResources map;
    cgl::MapHoverOutline outline;
    cgl::MapWorldRenderer renderer;
    cgl::component::MapCamera camera{{0,0},1};
    cgl::Viewport viewport{0,0,800,600};
    std::shared_ptr<cgl::AnimationClip> clip=std::make_shared<cgl::AnimationClip>();
    cgl::Entity actor;
    Fixture() {
        map.layout={2,1,{{123,0,0},{123,124,0}}};
        map.images.emplace(123,cgl::MapImage{{0,0,64,47},-32,-23,1,1,0});
        map.images.emplace(124,cgl::MapImage{{0,0,64,100},-32,-90,1,1,1});
        map.renderers.push_back(std::make_unique<RecordingSprite>(calls,1));
        map.renderers.push_back(std::make_unique<RecordingSprite>(calls,2));
        clip->renderer=std::make_unique<RecordingSprite>(calls,3);
        clip->width=40;clip->height=80;clip->frameCount=10;clip->durationMs=1000;clip->offsetY=-80;
        outline.renderer=std::make_unique<RecordingSprite>(calls,4);outline.width=70;outline.height=53;outline.scale=1;
        actor=world.createEntity();world.addComponent<cgl::component::MapPosition>(actor,cgl::MapPoint{0,0});
        world.addComponent<cgl::component::Sprite>(actor,clip);
        world.addComponent<cgl::component::AnimationPlayer>(actor,250.0,1.0,2);
        world.addSingleton<cgl::component::MapHover>(cgl::MapCellCoord{9,9});
    }
    auto draw() {return renderer.draw(world,map,camera,cgl::MapCellCoord{0,0},&outline,nullptr,viewport);}
};
}
TEST(MapWorld, RecordingPreservesOrderPlaybackAndWorldHover) {
    Fixture f;
    const auto stats=f.draw();
    ASSERT_EQ(f.calls.size(),5);
    const int expected[]={1,1,2,3,4};
    for(size_t i=0;i<5;++i) EXPECT_EQ(f.calls[i].id,expected[i]);
    EXPECT_EQ(f.calls[3].frame,2);
    EXPECT_EQ(stats.drawCalls,4);EXPECT_EQ(stats.drawnActors,1); // Overlay excluded.
    f.draw();
    const auto* player=f.world.getComponent<cgl::component::AnimationPlayer>(f.actor);
    EXPECT_DOUBLE_EQ(player->timeMs,250);EXPECT_EQ(player->frame,2);
    EXPECT_DOUBLE_EQ(f.world.getComponent<cgl::component::MapPosition>(f.actor)->cell.x,0);
    EXPECT_EQ(f.world.getSingleton<cgl::component::MapHover>()->cell,(cgl::MapCellCoord{9,9}));
    EXPECT_EQ(static_cast<RecordingSprite*>(f.clip->renderer.get())->polls,0);
}
TEST(MapWorld, RecordingFailureAndReuseDoNotRetainClipOwnership) {
    Fixture f;
    auto* sprite=static_cast<RecordingSprite*>(f.clip->renderer.get());sprite->fail=true;
    EXPECT_THROW(f.draw(),std::runtime_error);
    sprite->fail=false;f.calls.clear();EXPECT_EQ(f.draw().drawnActors,1);
    std::weak_ptr<const cgl::AnimationClip> weak=f.clip;
    f.world.destroyEntity(f.actor);f.clip.reset();EXPECT_TRUE(weak.expired());
    f.calls.clear();EXPECT_EQ(f.draw().drawnActors,0);
    EXPECT_EQ(f.calls.size(),4); // Two grounds, one object and the outline.
    f.renderer.clear();f.renderer.clear();
}
TEST(MapWorld, ExplicitUpdateAdvancesMovementCameraAndAnimationWithoutWindow) {
    Fixture f;
    f.world.addSingleton<cgl::component::MapCamera>(cgl::MapPoint{0,0},1);
    cgl::component::CharacterMovement movement;
    movement.idle.fill(f.clip);movement.walk.fill(f.clip);
    f.world.addComponent<cgl::component::CharacterMovement>(f.actor,std::move(movement));
    cgl::UpdateMapWorld(f.world,f.actor,100,100,1,0,.1);
    const auto cell=f.world.getComponent<cgl::component::MapPosition>(f.actor)->cell;
    const auto center=f.world.getSingleton<cgl::component::MapCamera>()->center;
    EXPECT_GT(cell.x,0);EXPECT_GT(cell.y,0);
    EXPECT_DOUBLE_EQ(center.x,cell.x);EXPECT_DOUBLE_EQ(center.y,cell.y);
    EXPECT_DOUBLE_EQ(f.world.getComponent<cgl::component::AnimationPlayer>(f.actor)->timeMs,350);
}
