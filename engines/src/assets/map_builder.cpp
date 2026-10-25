#include "assets/map_builder.h"
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>

namespace cgl {
namespace {
void checkCancelled(const std::atomic_bool* cancelled) {
    if(cancelled && cancelled->load()) throw std::runtime_error("Map preparation cancelled");
}
}
MapBuilder::MapBuilder(MapLayout layout, IGraphicsInfoReader& infoReader, IPaletteReader& paletteReader,
    const std::atomic_bool* cancelled) {
    checkCancelled(cancelled);
    const uint64_t cells=uint64_t(layout.width)*layout.height;
    if(!layout.width || !layout.height || cells>4194304 || cells!=layout.cells.size())
        throw std::invalid_argument("Invalid map layout dimensions/cell count");
    data_.layout=std::move(layout);
    if(paletteReader.read(EnvironmentPaletteTypes::Daytime,&palette_)!=Results::Success)
        throw std::runtime_error("Could not read map palette");
    constexpr auto version=CrossGateVersion::CG_VERSION_Classic;
    std::set<uint16_t> ids;
    for(const auto& cell:data_.layout.cells) {
        checkCancelled(cancelled);
        if(IsClassicMapImage(cell.groundId,false)) ids.insert(cell.groundId);
        if(IsClassicMapImage(cell.objectId,true)) ids.insert(cell.objectId);
    }
    std::vector<MapImageSize> sizes;
    for(const auto id:ids) {
        checkCancelled(cancelled);
        GraphicsResourceInfo info{};
        if(infoReader.query({GraphicsResourceSerialNumTypes::MapSerialNum,version,id},&info)!=Results::Success)
            throw std::runtime_error("Classic map image ID not found: "+std::to_string(id));
        if(info.width<=0 || info.height<=0)
            throw std::runtime_error("Invalid map image dimensions: "+std::to_string(id));
        sizes.push_back({id,uint32_t(info.width),uint32_t(info.height)});
        infos_.emplace(id,info);
    }
    auto plans=PlanMapAtlases(std::move(sizes));
    if(!plans) throw std::runtime_error("Map images exceed 8 atlas pages of 2048x4096 or contain an oversized image");
    pages_=std::move(*plans);
    for(size_t page=0;page<pages_.size();++page) {
        checkCancelled(cancelled);
        const auto& plan=pages_[page];
        data_.rgbaBytes+=uint64_t(plan.width)*plan.height*4;
        for(const auto& region:plan.regions) {
            const auto& info=infos_.at(region.id);
            data_.images.emplace(region.id,MapImage{region.source,info.offsetX,info.offsetY,
                std::max(uint8_t(1),info.tileX),std::max(uint8_t(1),info.tileY),uint32_t(page)});
            data_.reachX=std::max({data_.reachX,std::abs(double(info.offsetX)),std::abs(double(info.offsetX)+info.width)});
            data_.reachY=std::max({data_.reachY,std::abs(double(info.offsetY)),std::abs(double(info.offsetY)+info.height)});
        }
    }
}

MapPagePixels MapBuilder::buildPage(size_t index, IGraphicsDataReader& reader, uint64_t maxBytes,
    const std::atomic_bool* cancelled) const {
    checkCancelled(cancelled);
    const auto& plan=pages_.at(index);
    const uint64_t bytes=uint64_t(plan.width)*plan.height*4;
    if(bytes>maxBytes) throw std::runtime_error("Map atlas page exceeds CPU pixel budget");
    MapPagePixels result{plan.width,plan.height,std::vector<uint8_t>(size_t(bytes),0)};
    for(const auto& region:plan.regions) {
        checkCancelled(cancelled);
        const auto& info=infos_.at(region.id);
        GraphicsResourceData data{};
        if(reader.query(info,&data)!=Results::Success || !data.pData || data.width!=info.width || data.height!=info.height)
            throw std::runtime_error("Could not decode map image: "+std::to_string(region.id));
        const auto* colors=data.pPaletteData ? data.pPaletteData : reinterpret_cast<const uint8_t*>(palette_.data());
        for(int y=0;y<data.height;++y) {
            checkCancelled(cancelled);
            for(int x=0;x<data.width;++x) {
                const auto color=data.pData[(data.height-1-y)*size_t(data.width)+x];
                auto* dst=result.rgba.data()+((size_t(region.source.y)+y)*plan.width+region.source.x+x)*4;
                dst[0]=colors[color*3+2];dst[1]=colors[color*3+1];dst[2]=colors[color*3];dst[3]=color?255:0;
            }
        }
    }
    return result;
}
}
