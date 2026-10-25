#include <gtest/gtest.h>
#include "assets/map_builder.h"
#include <array>
#include <stdexcept>
#include <type_traits>

namespace {
constexpr auto Classic=cgl::CrossGateVersion::CG_VERSION_Classic;
class InfoReader final:public cgl::IGraphicsInfoReader {
public:
    InfoReader():IGraphicsInfoReader({nullptr,Classic}) {}
    std::unordered_map<uint16_t,cgl::GraphicsResourceInfo> entries;
    mutable size_t queries=0;mutable bool classicMapKeys=true;
    void add(uint16_t id,int32_t width=2,int32_t height=2) {
        cgl::GraphicsResourceInfo info{};info.version=Classic;
        info.mapBasedsIdx={cgl::GraphicsResourceSerialNumTypes::MapSerialNum,Classic,id};
        info.width=width;info.height=height;entries.emplace(id,info);
    }
    cgl::Results load() override {return cgl::Results::Success;}
    void close() noexcept override {entries.clear();}
    size_t infoCount() const noexcept override {return entries.size();}
    bool mightContain(const cgl::GraphicsResourceSerialNum& key) const noexcept override {return entries.contains(uint16_t(key.value));}
    cgl::Results queryAvailableSerialNums(std::vector<int32_t>*) noexcept override {return cgl::Results::Fail;}
    cgl::Results query(const cgl::GraphicsResourceSerialNum& key,cgl::GraphicsResourceInfo* out) const noexcept override {
        ++queries;classicMapKeys&=key.type==cgl::GraphicsResourceSerialNumTypes::MapSerialNum && key.version==Classic;
        const auto found=entries.find(uint16_t(key.value));
        if(found==entries.end()) return cgl::Results::IndexNotExist;
        *out=found->second;return cgl::Results::Success;
    }
};
class PaletteReader final:public cgl::IPaletteReader {
public:
    PaletteReader():IPaletteReader({nullptr}) {
        for(unsigned i=0;i<256;++i) {
            colors[i].BGR.B=uint8_t(i*10+1);colors[i].BGR.G=uint8_t(i*10+2);colors[i].BGR.R=uint8_t(i*10+3);
        }
    }
    cgl::PaletteData256 colors{};size_t reads=0;bool fail=false;
    void clearCache() noexcept override {}
    cgl::Results read(cgl::EnvironmentPaletteTypes env,cgl::PaletteData256* out) override {
        ++reads;EXPECT_EQ(env,cgl::EnvironmentPaletteTypes::Daytime);
        if(fail) return cgl::Results::Fail;
        *out=colors;return cgl::Results::Success;
    }
};
class DataReader final:public cgl::IGraphicsDataReader {
public:
    DataReader():IGraphicsDataReader({nullptr,Classic}) {}
    enum class Mode {Normal,Fail,Null,WrongWidth,WrongHeight,Throw};
    Mode mode=Mode::Normal;size_t queries=0;bool embedded=false;
    std::array<uint8_t,768> palette{};
    cgl::Results load() override {return cgl::Results::Success;}
    void close() noexcept override {}
    void setCacheBudget(size_t) noexcept override {}
    void trimCache() noexcept override {}
    cgl::ResourceCacheStatistics cacheStatistics() const noexcept override {return {};}
    void resetCacheStatistics() noexcept override {}
    cgl::Results query(const cgl::GraphicsResourceInfo& info,cgl::GraphicsResourceData* out) override {
        ++queries;
        if(mode==Mode::Throw) throw std::runtime_error("injected decode exception");
        if(mode==Mode::Fail) return cgl::Results::Fail;
        out->version=Classic;out->width=info.width+(mode==Mode::WrongWidth);out->height=info.height+(mode==Mode::WrongHeight);
        if(mode==Mode::Null) return cgl::Results::Success;
        const auto size=size_t(out->width)*out->height;
        out->pData=std::make_unique<uint8_t[]>(size);
        for(size_t i=0;i<size;++i) out->pData[i]=uint8_t((i+1)%4);
        out->pPaletteData=embedded?palette.data():nullptr;return cgl::Results::Success;
    }
};
std::array<uint8_t,4> pixel(const cgl::MapPagePixels& p,uint32_t x,uint32_t y) {
    const auto* v=p.rgba.data()+(size_t(y)*p.width+x)*4;return {v[0],v[1],v[2],v[3]};
}
}

TEST(MapBuilder, PlanningOwnsMetadataDeduplicatesIdsAndPreservesFootprints) {
    InfoReader info;PaletteReader palette;DataReader pixels;info.add(123);info.add(124,3,4);
    info.entries.at(123).offsetX=-12;info.entries.at(123).offsetY=-40;
    info.entries.at(123).tileX=0;info.entries.at(123).tileY=3;
    cgl::MapLayout layout{3,1,{{123,124,7},{123,200,8},{2,265,9}}};
    cgl::MapBuilder builder(layout,info,palette);layout.cells.clear();info.entries.clear();palette.colors={};
    const auto& data=builder.data();
    EXPECT_EQ(data.layout.cells.size(),3);EXPECT_EQ(data.layout.cells[2].flags,9);
    EXPECT_EQ(data.images.size(),2);EXPECT_EQ(info.queries,2);EXPECT_TRUE(info.classicMapKeys);
    EXPECT_EQ(palette.reads,1);EXPECT_EQ(pixels.queries,0);EXPECT_EQ(builder.pageCount(),1);
    const auto& image=data.images.at(123);EXPECT_EQ(image.offsetX,-12);EXPECT_EQ(image.offsetY,-40);
    EXPECT_EQ(image.tileX,1);EXPECT_EQ(image.tileY,3);EXPECT_EQ(image.atlasIndex,0);
    EXPECT_DOUBLE_EQ(data.reachX,12);EXPECT_DOUBLE_EQ(data.reachY,40);
    const auto page=builder.buildPage(0,pixels);EXPECT_EQ(data.rgbaBytes,page.rgba.size());
    EXPECT_EQ(pixel(page,image.source.x,image.source.y),(std::array<uint8_t,4>{33,32,31,255}));
}

TEST(MapBuilder, CopiesBottomUpBgrToOwnedTopDownRgbaAndKeepsBordersTransparent) {
    cgl::MapPagePixels page{};
    {
        InfoReader info;PaletteReader palette;DataReader data;info.add(123);
        cgl::MapBuilder builder({1,1,{{123,0,0}}},info,palette);
        page=builder.buildPage(0,data);
        EXPECT_EQ(page.width,4);EXPECT_EQ(page.height,4);
        EXPECT_EQ(pixel(page,1,1),(std::array<uint8_t,4>{33,32,31,255}));
        EXPECT_EQ(pixel(page,2,1),(std::array<uint8_t,4>{3,2,1,0}));
        EXPECT_EQ(pixel(page,1,2),(std::array<uint8_t,4>{13,12,11,255}));
        EXPECT_EQ(pixel(page,2,2),(std::array<uint8_t,4>{23,22,21,255}));
    } // All readers and preparation destroyed; returned pixels remain owned.
    EXPECT_EQ(pixel(page,1,1),(std::array<uint8_t,4>{33,32,31,255}));
    for(uint32_t i=0;i<4;++i) for(const auto p:{pixel(page,i,0),pixel(page,i,3),pixel(page,0,i),pixel(page,3,i)})
        EXPECT_EQ(p,(std::array<uint8_t,4>{0,0,0,0}));
}

TEST(MapBuilder, EmbeddedPaletteOverridesEnvironmentAndPageResultsAreIndependent) {
    InfoReader info;PaletteReader palette;DataReader data;info.add(123);
    cgl::MapBuilder builder({1,1,{{123,0,0}}},info,palette);
    auto original=builder.buildPage(0,data);data.embedded=true;
    data.palette[9]=70;data.palette[10]=80;data.palette[11]=90;
    const auto embedded=builder.buildPage(0,data);data.palette.fill(0);
    EXPECT_EQ(pixel(embedded,1,1),(std::array<uint8_t,4>{90,80,70,255}));
    EXPECT_EQ(pixel(original,1,1),(std::array<uint8_t,4>{33,32,31,255}));
    original.rgba.clear();EXPECT_EQ(pixel(embedded,1,1)[0],90);
}

TEST(MapBuilder, InvalidLayoutsFailBeforeReaderAccess) {
    InfoReader info;PaletteReader palette;
    for(auto layout:{cgl::MapLayout{},cgl::MapLayout{1,1,{}},cgl::MapLayout{1,1,{{},{}}},
            cgl::MapLayout{UINT32_MAX,UINT32_MAX,{}},cgl::MapLayout{4194305,1,{}}})
        EXPECT_THROW(cgl::MapBuilder(std::move(layout),info,palette),std::invalid_argument);
    EXPECT_EQ(info.queries,0);EXPECT_EQ(palette.reads,0);
}

TEST(MapBuilder, MissingPaletteOrMetadataAndInvalidImageDimensionsFailDuringPlanning) {
    InfoReader info;PaletteReader palette;const cgl::MapLayout layout{1,1,{{123,0,0}}};
    palette.fail=true;EXPECT_THROW(cgl::MapBuilder(layout,info,palette),std::runtime_error);EXPECT_EQ(info.queries,0);
    palette.fail=false;EXPECT_THROW(cgl::MapBuilder(layout,info,palette),std::runtime_error);
    for(const auto size:{std::pair{0,2},std::pair{2,-1},std::pair{2047,2},std::pair{2,4095}}) {
        info.entries.clear();info.add(123,size.first,size.second);
        EXPECT_THROW(cgl::MapBuilder(layout,info,palette),std::runtime_error);
    }
}

TEST(MapBuilder, MultiPagePreparationPlansAllPagesButBuildsOnlyTheRequestedPage) {
    InfoReader info;PaletteReader palette;DataReader data;
    for(uint16_t id:{123,124,125}) info.add(id,1024,4094);
    cgl::MapBuilder builder({3,1,{{123,0,0},{124,0,0},{125,0,0}}},info,palette);
    ASSERT_EQ(builder.pageCount(),3);EXPECT_EQ(data.queries,0);
    EXPECT_EQ(builder.data().images.at(125).atlasIndex,2);
    const auto bytes=uint64_t(1026)*4096*4;
    EXPECT_EQ(builder.data().rgbaBytes,3*bytes);
    EXPECT_THROW(builder.buildPage(2,data,bytes-1),std::runtime_error);EXPECT_EQ(data.queries,0);
    {
        const auto page=builder.buildPage(2,data,bytes);EXPECT_EQ(page.rgba.size(),bytes);EXPECT_EQ(data.queries,1);
    }
    EXPECT_THROW(builder.buildPage(3,data),std::out_of_range);EXPECT_EQ(data.queries,1);
    const auto page=builder.buildPage(0,data);EXPECT_EQ(page.rgba.size(),bytes);EXPECT_EQ(data.queries,2);
}

TEST(MapBuilder, ExcessivePageCountIsRejectedWithoutPixelReads) {
    InfoReader info;PaletteReader palette;cgl::MapLayout layout{9,1,{}};
    for(uint16_t id=123;id<132;++id) {info.add(id,2046,4094);layout.cells.push_back({id,0,0});}
    EXPECT_THROW(cgl::MapBuilder(std::move(layout),info,palette),std::runtime_error);
}

TEST(MapBuilder, DecodeFailuresLeavePreparationReusableAndPreviouslyReturnedPixelsAlive) {
    InfoReader info;PaletteReader palette;DataReader data;info.add(123);
    cgl::MapBuilder builder({1,1,{{123,0,0}}},info,palette);
    const auto good=builder.buildPage(0,data);
    for(const auto mode:{DataReader::Mode::Fail,DataReader::Mode::Null,DataReader::Mode::WrongWidth,
            DataReader::Mode::WrongHeight,DataReader::Mode::Throw}) {
        data.mode=mode;EXPECT_THROW(builder.buildPage(0,data),std::runtime_error);
        EXPECT_EQ(builder.pageCount(),1);EXPECT_EQ(pixel(good,1,1)[0],33);
    }
    data.mode=DataReader::Mode::Normal;EXPECT_EQ(builder.buildPage(0,data).rgba,good.rgba);
}

TEST(MapBuilder, MetadataTransferConsumesPreparationWithoutCopyingLayoutStorage) {
    static_assert(!std::is_copy_constructible_v<cgl::MapBuilder>);
    InfoReader info;PaletteReader palette;DataReader data;info.add(123);
    cgl::MapLayout layout{1,1,{{123,0,17}}};const auto* cells=layout.cells.data();
    cgl::MapBuilder builder(std::move(layout),info,palette);
    auto moved=std::move(builder);EXPECT_EQ(moved.data().layout.cells.data(),cells);
    auto result=std::move(moved).takeData();EXPECT_EQ(result.layout.cells.data(),cells);
    EXPECT_EQ(result.layout.cells[0].flags,17);EXPECT_EQ(result.images.size(),1);
    EXPECT_EQ(moved.pageCount(),0);EXPECT_EQ(moved.data().rgbaBytes,0);
    EXPECT_THROW(moved.buildPage(0,data),std::out_of_range);
}

TEST(MapBuilder, EmptyArtworkKeepsLayoutAndNeedsNoAtlasPixels) {
    InfoReader info;PaletteReader palette;DataReader data;
    cgl::MapBuilder builder({2,1,{{0,2,19},{2,200,20}}},info,palette);
    EXPECT_EQ(builder.pageCount(),0);EXPECT_EQ(builder.data().rgbaBytes,0);
    EXPECT_TRUE(builder.data().images.empty());EXPECT_EQ(builder.data().layout.cells[1].flags,20);
    EXPECT_EQ(info.queries,0);EXPECT_THROW(builder.buildPage(0,data),std::out_of_range);EXPECT_EQ(data.queries,0);
}
