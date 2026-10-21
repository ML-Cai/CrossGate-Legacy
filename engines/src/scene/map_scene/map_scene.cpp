#include "scene/scene_components.h"
#include "assets/map_library.h"
#include "assets/assets_components.h"
#include "animation/animation_components.h"
#include "animation/animation_system.h"
#include "map/map_render_data.h"
#include "map/map_hover.h"
#include "map/character_movement.h"
#include "engine/ecs.h"
#include "engine/engine_components.h"
#include "engine/error_system.h"
#include "render/frame_render_systems.h"
#include "window/window_components.h"
#include "cgl/trace/logger.h"
#include <stdexcept>
#include <chrono>
#include <random>
#include "benchmark/crowd_benchmark.h"

namespace {
class MapScene final : public cgl::IScene {
public:
    MapScene(cgl::MapLibrary& maps, cgl::AnimationLibrary& animations)
        : IScene(cgl::SceneTypes::MapScene), maps_(maps), animations_(animations) {}
    void onEnter(cgl::ECSCore* world) noexcept override {
        const auto loadStart = std::chrono::steady_clock::now();
        if (auto* b=world->getSingleton<cgl::component::CrowdBenchmark>()) *b={};
        try {
            const auto& settings = *world->getSingleton<cgl::component::EngineState>()->settings;
            const auto scale = settings.display.ViewportScale;
            if (!std::isfinite(scale) || scale > 16 || scale < 0.125)
                throw std::runtime_error("Map ViewportScale must be between 0.125 and 16");
            auto& render = *world->getSingleton<cgl::component::RenderDeviceState>();
            auto* pass = world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
            auto& readers = *world->getSingleton<cgl::component::AssetsReaderState>();
            map_ = maps_.acquire(std::filesystem::path(settings.general.EngineRootPath) / settings.map.FilePath,
                readers, render.pDevice.get(), pass);
            const cgl::MapPoint center{double(map_->layout.width / 2), double(map_->layout.height / 2)};
            world->addSingleton<cgl::component::MapCamera>(center, scale);
            world->addSingleton<cgl::component::MapHover>();
            outline_ = maps_.acquireHoverOutline(scale, render.pDevice.get(), pass);
            if (auto* benchmark = world->getSingleton<cgl::component::CrowdBenchmark>()) {
                enterCrowd(world, settings, readers, render.pDevice.get(), pass, *benchmark);
                benchmark->loadMs = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-loadStart).count();
                benchmark->ready = true;
                cgl::UpdateAnimations(*world,0);
                return;
            }
            if (settings.animation.Direction >= 8)
                throw std::runtime_error("Invalid initial character direction");
            cgl::component::CharacterMovement movement;
            movement.facing = settings.animation.Direction;
            // Load before entering the frame loop: turning never uploads GPU resources.
            for (uint32_t direction = 0; direction < 8; ++direction) {
                movement.idle[direction] = animations_.acquire({settings.animation.SerialNum, direction, 0},
                    readers, render.pDevice.get(), pass);
                movement.walk[direction] = animations_.acquire({settings.animation.SerialNum, direction, 1},
                    readers, render.pDevice.get(), pass);
            }
            auto clip = movement.idle[movement.facing];
            character_ = world->createEntity();
            world->addComponent<cgl::component::MapPosition>(character_, center);
            world->addComponent<cgl::component::Sprite>(character_, std::move(clip));
            world->addComponent<cgl::component::AnimationPlayer>(character_);
            world->addComponent<cgl::component::CharacterMovement>(character_, std::move(movement));
            cgl::UpdateAnimations(*world, 0);
            LOGI("Map scene entered");
        } catch (const std::exception& error) {
            if (auto* b=world->getSingleton<cgl::component::CrowdBenchmark>())
                b->loadMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-loadStart).count();
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Map scene failed: ", error.what());
        }
    }
    void onExit(cgl::ECSCore* world) noexcept override {
        if (auto* b=world->getSingleton<cgl::component::CrowdBenchmark>()) b->ready=false;
        for (auto entity : crowd_) world->destroyEntity(entity);
        crowd_.clear();
        crowdClips_.clear(); crowdWalks_.clear(); coldRequests_.clear();
        crowdFrames_=0; crowdSeconds_=0; crowdWalking_=false;
        if (character_) world->destroyEntity(character_);
        character_ = 0;
        world->removeSingleton<cgl::component::MapCamera>();
        world->removeSingleton<cgl::component::MapHover>();
        outline_.reset();
        map_.reset(); // Engine caches retain resources for pending submissions.
        drawItems_.clear();
        LOGI("Map scene exited");
    }
    void update(cgl::ECSCore* world) noexcept override {
        const auto seconds = world->getSingleton<cgl::component::FrameTime>()->deltaSeconds;
        if (auto* benchmark=world->getSingleton<cgl::component::CrowdBenchmark>()) {
            try { loadColdCharacter(world,*benchmark); }
            catch (const std::exception& error) {
                cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Cold character load failed: ",error.what());
                return;
            }
            crowdSeconds_ += seconds;
            const bool walking=(uint64_t(crowdSeconds_)%2)!=0;
            if (world->getSingleton<cgl::component::EngineState>()->settings->benchmark.SwitchMotions && walking!=crowdWalking_) {
                world->forEach<cgl::component::CrowdActor,cgl::component::Sprite,cgl::component::AnimationPlayer>(
                    [&](auto,auto* actor,auto* sprite,auto* player){ sprite->clip=walking?actor->walk:actor->idle;player->timeMs=0;player->frame=0; });
                crowdWalking_=walking; ++benchmark->switches;
            }
            cgl::UpdateAnimations(*world,seconds);
            return;
        }
        auto* input = world->getSingleton<cgl::component::KeyboardInputState>();
        cgl::UpdateCharacterMovement(*world, input->moveX, input->moveY, seconds,
            map_->layout.width, map_->layout.height);
        // Keep the character's ground anchor at the viewport center. Translating
        // the camera makes terrain and objects scroll opposite to movement.
        world->getSingleton<cgl::component::MapCamera>()->center =
            world->getComponent<cgl::component::MapPosition>(character_)->cell;
        cgl::UpdateAnimations(*world, seconds);
        const auto& window = *world->getSingleton<cgl::component::WindowHandle>();
        updateHover(world, {0, 0, window.width, window.height});
        if (input->restartSceneRequested) {
            input->restartSceneRequested = false;
            world->getSingleton<cgl::component::SceneState>()->nextScene = cgl::SceneTypes::MapScene;
        }
    }
    void draw(cgl::ECSCore* world, cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) noexcept override {
        try {
            auto* benchmark=world->getSingleton<cgl::component::CrowdBenchmark>();
            if (benchmark) {
                benchmark->drawnActors=benchmark->drawCalls=0;
                world->getSingleton<cgl::component::MapHover>()->cell.reset();
            } else updateHover(world, viewport);
            const auto& camera = *world->getSingleton<cgl::component::MapCamera>();
            auto submit = [&](const cgl::MapDrawItem& item) {
                if (record(item,camera,command,viewport) && benchmark) {
                    ++benchmark->drawCalls;
                    if (item.stableId & (1ull<<32)) ++benchmark->drawnActors;
                }
            };
            drawItems_.clear();
            const auto bounds = cgl::VisibleMapBounds(camera, viewport, map_->layout.width, map_->layout.height, map_->reachX, map_->reachY);
            for (int y = bounds.top; y <= bounds.bottom; ++y) for (int x = bounds.left; x <= bounds.right; ++x) {
                const auto& cell = *map_->layout.cell(x, y);
                if (const auto found = map_->images.find(cell.groundId); found != map_->images.end()) {
                    const auto& image = found->second;
                    submit({{double(x), double(y)}, map_->renderers.at(image.atlasIndex).get(), image.source, 0,
                        image.offsetX, image.offsetY, 0});
                }
                if (cgl::IsClassicMapImage(cell.objectId, true)) {
                    if (const auto found = map_->images.find(cell.objectId); found != map_->images.end()) {
                        const auto& image = found->second;
                        drawItems_.push_back({{double(x), double(y)}, map_->renderers.at(image.atlasIndex).get(), image.source, 0,
                            image.offsetX, image.offsetY, uint64_t(y) * map_->layout.width + x,
                            image.tileX, image.tileY});
                    }
                }
            }
            world->forEach<cgl::component::MapPosition, cgl::component::Sprite, cgl::component::AnimationPlayer>(
                [&](cgl::Entity entity, auto* position, auto* sprite, auto* player) {
                    const auto& clip = sprite->clip;
                    if (clip) drawItems_.push_back({position->cell, clip->renderer.get(), {0, 0, clip->width, clip->height},
                        player->frame, clip->offsetX, clip->offsetY, (1ull << 32) | entity, 0, 0});
                });
            const auto sortStart=std::chrono::steady_clock::now();
            cgl::SortMapDrawItems(drawItems_);
            if (benchmark) benchmark->sortMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-sortStart).count();
            for (const auto& item : drawItems_) submit(item);
            const auto& hovered = world->getSingleton<cgl::component::MapHover>()->cell;
            if (hovered && outline_) {
                const auto center = cgl::MapToScreen({double(hovered->x), double(hovered->y)}, camera, viewport);
                const double scale = camera.scale / outline_->scale;
                outline_->renderer->draw(command, viewport, 0,
                    float(center.x - outline_->width * scale * 0.5),
                    float(center.y - outline_->height * scale * 0.5), float(scale));
            }
        } catch (const std::exception& error) {
            cgl::RaiseError(world->getSingleton<cgl::component::SceneState>(), "Map drawing failed: ", error.what());
        }
    }
private:
    void enterCrowd(cgl::ECSCore* world, const cgl::Settings& settings,
                    cgl::component::AssetsReaderState& readers, cgl::graphics::IDevice* device,
                    cgl::graphics::IRenderPass* pass, cgl::component::CrowdBenchmark& state) {
        const auto& b=settings.benchmark;
        if (b.ActorCount>20000 || b.ActorsPerCell>10 || !b.UniqueCharacters || b.UniqueCharacters>20000)
            throw std::runtime_error("Invalid crowd density/count/unique limit");
        const auto& window=*world->getSingleton<cgl::component::WindowHandle>();
        state.width=window.width;state.height=window.height;
        const cgl::Viewport viewport(0,0,window.width,window.height);
        const auto& camera=*world->getSingleton<cgl::component::MapCamera>();
        const auto bounds=cgl::VisibleMapBounds(camera,viewport,map_->layout.width,map_->layout.height,0,0);
        std::vector<cgl::MapPoint> cells;
        for(int y=bounds.top;y<=bounds.bottom;++y) for(int x=bounds.left;x<=bounds.right;++x) {
            const cgl::MapPoint cell{double(x),double(y)};
            const auto p=cgl::MapToScreen(cell,camera,viewport);
            if(p.x>0 && p.y>0 && p.x<viewport.width && p.y<viewport.height) cells.push_back(cell);
        }
        state.visibleCells=uint32_t(cells.size());
        const size_t perCell=b.ActorsPerCell ? b.ActorsPerCell : 1;
        const size_t count=b.ActorCount ? b.ActorCount : cells.size()*perCell;
        if(cells.empty() || count>20000 || count>cells.size()*10 || b.UniqueCharacters>count)
            throw std::runtime_error("Crowd count exceeds visible cell capacity or unique count exceeds actors");
        std::vector<cgl::AnimationRequest> requests;
        auto& clips=crowdClips_; auto& walks=crowdWalks_;
        for(unsigned v=0;v<unsigned(cgl::CrossGateVersion::COUNT) && requests.size()<b.UniqueCharacters;++v) {
            const auto version=cgl::CrossGateVersion(v);
            auto* infoReader=readers.pAnimeInfoReader[version].get();
            auto* dataReader=readers.pAnimeDataReader[version].get();
            if(!infoReader || !dataReader) continue;
            std::vector<cgl::AnimeResourceSerialNum> serials;
            if(infoReader->queryAvailableSerialNums(&serials)!=cgl::Results::Success) continue;
            std::sort(serials.begin(),serials.end(),[](const auto& a,const auto& b){return a.value<b.value;});
            for(const auto& serial:serials) {
                cgl::AnimeResourceInfo info{}; cgl::AnimeResourceData data{};
                if(infoReader->query(serial,&info)!=cgl::Results::Success ||
                   dataReader->query(info,&data)!=cgl::Results::Success) continue;
                if (!cgl::IsCrowdCharacterDescription(data)) { ++state.filtered; continue; }
                const auto key=cgl::AnimeDataMapKey{cgl::DirectionTypes(0),cgl::MotionTypes(0)};
                auto motion=data.motionMap.find(key);
                if(motion==data.motionMap.end() || motion->second.motionGraphicsSerialNums.empty() || !motion->second.duration) continue;
                bool validMetadata = true;
                for (unsigned m=0;m<2;++m) {
                    const auto& frameIds=data.motionMap.at({cgl::DirectionTypes(0),cgl::MotionTypes(m)}).motionGraphicsSerialNums;
                    for (auto graphic:frameIds) {
                        cgl::GraphicsResourceInfo frame{};
                        if (!readers.pGfxInfoReader[version] || readers.pGfxInfoReader[version]->query(
                            {cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum,version,graphic},&frame)!=cgl::Results::Success ||
                            frame.width<=0 || frame.height<=0 || frame.width>4096 || frame.height>4096) {
                            validMetadata=false; break;
                        }
                    }
                    if(!validMetadata) break;
                }
                if(!validMetadata) { ++state.filtered; continue; }
                requests.push_back({serial.value,0,0,version});
                if(requests.size()==b.UniqueCharacters) break;
            }
        }
        if(requests.size()!=b.UniqueCharacters) throw std::runtime_error("Insufficient distinct animation resources in installed corpus");
        // Metadata is selected at entry; pixel decoding/upload for cold resources
        // deliberately runs inside the measured loop. This exposes synchronous stalls.
        if(b.LoadDuringPlayback) {
            std::mt19937 random(0xC61);
            std::shuffle(requests.begin(),requests.end(),random);
        }
        for(const auto& r:requests) state.resources.emplace_back(unsigned(r.version),r.serial);
        const size_t initial=b.LoadDuringPlayback ? 1 : requests.size();
        for(size_t i=0;i<initial;++i) loadCrowdClip(requests[i],readers,device,pass,b.SwitchMotions,state);
        coldRequests_=std::move(requests);
        for(size_t i=0;i<count;++i) {
            const auto cellIndex=i%cells.size();
            const auto entity=world->createEntity(); crowd_.push_back(entity);
            world->addComponent<cgl::component::MapPosition>(entity,cells[cellIndex]);
            const auto resource=cgl::CrowdResourceIndex(i,cells.size(),coldRequests_.size());
            world->addComponent<cgl::component::Sprite>(entity,clips[resource<clips.size()?resource:0]);
            const auto ready=resource<clips.size()?resource:0;
            world->addComponent<cgl::component::CrowdActor>(entity,clips[ready],b.SwitchMotions?walks[ready]:nullptr,resource);
            world->addComponent<cgl::component::AnimationPlayer>(entity,double((i*73)%1000));
            ++state.actors;
        }
        LOGI("Crowd ready: cells=" << cells.size() << ", actors=" << count << ", resources=" << clips.size());
    }
    void loadCrowdClip(const cgl::AnimationRequest& request,cgl::component::AssetsReaderState& readers,
        cgl::graphics::IDevice* device,cgl::graphics::IRenderPass* pass,bool switchMotions,
        cgl::component::CrowdBenchmark& state) {
        auto idle=animations_.acquire(request,readers,device,pass);
        auto walk=switchMotions ? animations_.acquire({request.serial,request.direction,1,request.version},readers,device,pass) : nullptr;
        state.animationBytes+=uint64_t(idle->width)*idle->height*idle->frameCount*4;
        if(walk) state.animationBytes+=uint64_t(walk->width)*walk->height*walk->frameCount*4;
        crowdClips_.push_back(std::move(idle)); crowdWalks_.push_back(std::move(walk));
        state.unique=uint32_t(crowdClips_.size());
    }
    void loadColdCharacter(cgl::ECSCore* world,cgl::component::CrowdBenchmark& state) {
        const auto& b=world->getSingleton<cgl::component::EngineState>()->settings->benchmark;
        if(!b.LoadDuringPlayback || crowdFrames_++<b.WarmupFrames || state.unique>=coldRequests_.size()) return;
        const auto start=std::chrono::steady_clock::now();
        const size_t resource=state.unique;
        auto& readers=*world->getSingleton<cgl::component::AssetsReaderState>();
        auto* device=world->getSingleton<cgl::component::RenderDeviceState>()->pDevice.get();
        auto* pass=world->getSingleton<cgl::component::PrimarySceneRenderFrame>()->renderPasses.at(cgl::PRIMARY_RENDER_PASS).get();
        loadCrowdClip(coldRequests_[resource],readers,device,pass,b.SwitchMotions,state);
        world->forEach<cgl::component::CrowdActor,cgl::component::Sprite,cgl::component::AnimationPlayer>(
            [&](auto,auto* actor,auto* sprite,auto* player) {
                if(actor->resource!=resource) return;
                actor->idle=crowdClips_[resource]; actor->walk=crowdWalks_[resource];
                sprite->clip=crowdWalking_?actor->walk:actor->idle;
                player->timeMs=0;player->frame=0;
            });
        state.coldLoadMs.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    }
    void updateHover(cgl::ECSCore* world, const cgl::Viewport& viewport) noexcept {
        auto& hover = world->getSingleton<cgl::component::MapHover>()->cell;
        hover.reset();
        const auto* cursor = world->getSingleton<cgl::component::CursorPositionState>();
        if (!cursor || !cursor->inside) return;
        const auto& window = *world->getSingleton<cgl::component::WindowHandle>();
        const auto point = cgl::WindowToViewport({cursor->x, cursor->y},
            {double(cursor->windowWidth), double(cursor->windowHeight)},
            {double(window.width), double(window.height)}, viewport);
        if (point) hover = cgl::PickMapCell(*point,
            *world->getSingleton<cgl::component::MapCamera>(), viewport, map_->layout);
    }
    static bool record(const cgl::MapDrawItem& item, const cgl::component::MapCamera& camera,
                        cgl::graphics::ICommandBuffer* command, const cgl::Viewport& viewport) {
        auto position = cgl::MapToScreen(item.cell, camera, viewport);
        position.x += item.offsetX * camera.scale;
        position.y += item.offsetY * camera.scale;
        if (!item.renderer || !cgl::MapImageVisible(position, item.source.width, item.source.height, camera.scale, viewport)) return false;
        item.renderer->drawRegion(command, viewport, item.frame, item.source,
            float(position.x), float(position.y), float(camera.scale));
        return true;
    }
    cgl::MapLibrary& maps_;
    cgl::AnimationLibrary& animations_;
    std::shared_ptr<const cgl::MapResources> map_;
    std::shared_ptr<const cgl::MapHoverOutline> outline_;
    cgl::Entity character_ = 0;
    std::vector<cgl::Entity> crowd_;
    std::vector<std::shared_ptr<const cgl::AnimationClip>> crowdClips_,crowdWalks_;
    std::vector<cgl::AnimationRequest> coldRequests_;
    uint32_t crowdFrames_=0;
    double crowdSeconds_=0;
    bool crowdWalking_=false;
    std::vector<cgl::MapDrawItem> drawItems_;
};
}
cgl::IScene::Ptr cgl::CreateMapScene(MapLibrary& maps, AnimationLibrary& animations) {
    return std::make_unique<MapScene>(maps, animations);
}
