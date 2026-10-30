#pragma once
#include <cmath>
#include <memory>
#include <stdexcept>
#include <pybind11/pybind11.h>
#include "assets/map_builder.h"
#include "map/map_render_data.h"

namespace cgl::py {
// CPU plans and metadata only; pages are explicitly decoded through a live Session.
class MapPreview {
public:
    MapPreview(MapLayout layout, IGraphicsInfoReader& info, IPaletteReader& palette)
        : builder_(std::move(layout),info,palette) {}
    uint32_t width() const { return builder_.data().layout.width; }
    uint32_t height() const { return builder_.data().layout.height; }
    size_t pageCount() const { return builder_.pageCount(); }
    MapPagePixels page(size_t index, IGraphicsDataReader& reader) const {
        if(index>=pageCount()) throw pybind11::index_error("Map page index is outside the plan");
        return builder_.buildPage(index,reader);
    }
    pybind11::list drawList(double x,double y,uint32_t width,uint32_t height,double scale,bool ground,bool objects) const {
        if(!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>this->width()-1||y>this->height()-1||
           !width||!height||width>1280||height>960||!std::isfinite(scale)||scale<0.5||scale>2)
            throw std::invalid_argument("Map view requires an in-map center, viewport <=1280x960 and scale 0.5..2");
        const auto& data=builder_.data();
        const component::MapCamera camera{{x,y},scale};
        const Viewport viewport{0,0,width,height};
        const auto bounds=VisibleMapBounds(camera,viewport,this->width(),this->height(),data.reachX,data.reachY);
        std::vector<MapDrawItem> floor,items;
        const auto add=[&](uint16_t id,bool object,int cx,int cy) {
            if(!IsClassicMapImage(id,object))return;
            const auto found=data.images.find(id);if(found==data.images.end())return;
            const auto& image=found->second;
            auto p=MapToScreen({double(cx),double(cy)},camera,viewport);
            p.x+=image.offsetX*scale;p.y+=image.offsetY*scale;
            if(!MapImageVisible(p,image.source.width,image.source.height,scale,viewport))return;
            if(floor.size()+items.size()>=8192 || (object && items.size()>=1024))
                throw std::runtime_error("Visible map draw limit exceeded; zoom in");
            (object?items:floor).push_back({{double(cx),double(cy)},nullptr,image.source,image.atlasIndex,
                image.offsetX,image.offsetY,uint64_t(cy)*this->width()+cx,image.tileX,image.tileY});
        };
        for(int cy=bounds.top;cy<=bounds.bottom;++cy)for(int cx=bounds.left;cx<=bounds.right;++cx) {
            const auto& cell=*data.layout.cell(cx,cy);
            if(ground)add(cell.groundId,false,cx,cy);
            if(objects)add(cell.objectId,true,cx,cy);
        }
        SortMapDrawItems(items); // Same footprint precedence and tie-breaking as native rendering.
        pybind11::list result;
        const auto append=[&](const auto& draws) {
            for(const auto& item:draws) {
                const auto p=MapToScreen(item.cell,camera,viewport);
                pybind11::dict value;
                value["page"]=item.frame;
                value["source"]=pybind11::make_tuple(item.source.x,item.source.y,item.source.width,item.source.height);
                value["x"]=p.x+item.offsetX*scale;value["y"]=p.y+item.offsetY*scale;
                result.append(std::move(value));
            }
        };
        append(floor);append(items);return result;
    }
    pybind11::tuple pan(double x,double y,double dx,double dy,double scale) const {
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(dx)||!std::isfinite(dy)||
           !std::isfinite(scale)||scale<.5||scale>2||std::abs(dx)>100000||std::abs(dy)>100000)
            throw std::invalid_argument("Invalid map pan coordinates or scale");
        const auto delta=WorldToMap({-dx/scale,-dy/scale});
        return pybind11::make_tuple(std::clamp(x+delta.x,0.0,double(width()-1)),
                                   std::clamp(y+delta.y,0.0,double(height()-1)));
    }
private:
    MapBuilder builder_;
};
}
