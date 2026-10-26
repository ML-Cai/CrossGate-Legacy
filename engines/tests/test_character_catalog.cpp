#include <gtest/gtest.h>
#include "assets/character_catalog.h"
#include <future>
namespace {
constexpr auto Classic=cgl::CrossGateVersion::CG_VERSION_Classic;
class Info final:public cgl::IAnimeInfoReader {
public:
    explicit Info(cgl::CrossGateVersion v):IAnimeInfoReader({nullptr,v}) {}
    std::vector<cgl::AnimeResourceSerialNum> serials;size_t queries=0;bool fail=false;
    cgl::Results load() override {return cgl::Results::Success;}
    void close() noexcept override {}
    cgl::Results queryAvailableSerialNums(std::vector<cgl::AnimeResourceSerialNum>* out) noexcept override {
        *out=serials;return fail?cgl::Results::Fail:cgl::Results::Success;
    }
    cgl::Results query(const cgl::AnimeResourceSerialNum& key,cgl::AnimeResourceInfo* out) override {
        ++queries;out->serialNum=key;return cgl::Results::Success;
    }
};
class Data final:public cgl::IAnimeDataReader {
public:
    explicit Data(cgl::CrossGateVersion v):IAnimeDataReader({nullptr,v}) {}
    std::unordered_map<uint32_t,cgl::AnimeResourceData> entries;size_t queries=0;bool fail=false;
    cgl::Results load() override {return cgl::Results::Success;}
    void close() noexcept override {}
    void setCacheBudget(size_t) noexcept override {}
    void trimCache() noexcept override {}
    cgl::ResourceCacheStatistics cacheStatistics() const noexcept override {return {};}
    void resetCacheStatistics() noexcept override {}
    cgl::Results query(const cgl::AnimeResourceInfo& info,cgl::AnimeResourceData* out) override {
        ++queries;
        auto found=entries.find(info.serialNum.value);
        if(fail || found==entries.end()) return cgl::Results::Fail;
        *out=found->second;return cgl::Results::Success;
    }
};
class Graphics final:public cgl::IGraphicsInfoReader {
public:
    explicit Graphics(cgl::CrossGateVersion v):IGraphicsInfoReader({nullptr,v}) {}
    cgl::Results load() override {return cgl::Results::Success;}
    void close() noexcept override {}
    size_t infoCount() const noexcept override {return 1;}
    bool mightContain(const cgl::GraphicsResourceSerialNum&) const noexcept override {return true;}
    cgl::Results queryAvailableSerialNums(std::vector<int32_t>*) noexcept override {return cgl::Results::Fail;}
    cgl::Results query(const cgl::GraphicsResourceSerialNum& key,cgl::GraphicsResourceInfo* out) const noexcept override {
        *out={};out->width=key.value==99?4096:2;out->height=out->width;
        return key.value==0?cgl::Results::Fail:cgl::Results::Success;
    }
};
class CatalogTest:public testing::Test {
protected:
    cgl::component::AssetsReaderState readers;
    Info* info;Data* data;
    void SetUp() override {install(Classic);info=static_cast<Info*>(readers.pAnimeInfoReader[Classic].get());data=static_cast<Data*>(readers.pAnimeDataReader[Classic].get());}
    void install(cgl::CrossGateVersion v) {
        readers.pAnimeInfoReader[v]=std::make_unique<Info>(v);
        readers.pAnimeDataReader[v]=std::make_unique<Data>(v);
        readers.pGfxInfoReader[v]=std::make_unique<Graphics>(v);
    }
    void add(uint32_t serial,cgl::CrossGateVersion v=Classic) {
        static_cast<Info*>(readers.pAnimeInfoReader[v].get())->serials.push_back({v,serial});
        cgl::AnimeResourceData value{};
        for(unsigned d=0;d<8;++d) for(unsigned m=0;m<2;++m)
            value.motionMap[{cgl::DirectionTypes(d),cgl::MotionTypes(m)}]={v,cgl::DirectionTypes(d),cgl::MotionTypes(m),100,{1}};
        static_cast<Data*>(readers.pAnimeDataReader[v].get())->entries[serial]=std::move(value);
    }
    static void finish(cgl::CharacterCatalogScan& scan,size_t batch=16) {
        for(int i=0;i<10000;++i) if(scan.advance(batch)) return;
        FAIL()<<"Scan did not terminate";
    }
};
}
TEST_F(CatalogTest, BatchCountsRejectedCandidatesAndLimitsResults) {
    for(uint32_t i=0;i<64;++i) add(i);
    for(uint32_t i=0;i<32;++i) data->entries.erase(i);
    cgl::CharacterCatalogScan scan(readers,5);
    EXPECT_FALSE(scan.advance());EXPECT_EQ(info->queries,0); // Enumeration only.
    EXPECT_FALSE(scan.advance());EXPECT_EQ(info->queries,16);EXPECT_EQ(scan.size(),0);
    EXPECT_FALSE(scan.advance());EXPECT_EQ(info->queries,32);EXPECT_EQ(scan.size(),0);
    EXPECT_TRUE(scan.advance());EXPECT_EQ(info->queries,37);
    auto result=scan.take();ASSERT_EQ(result.size(),5);EXPECT_EQ(result.front().serial,32);EXPECT_EQ(result.back().serial,36);
    EXPECT_TRUE(scan.advance());EXPECT_EQ(info->queries,37);
}
TEST_F(CatalogTest, PreservesVersionSerialOrderAndMatchesBlockingPath) {
    add(19);add(3);add(11);
    auto next=cgl::CrossGateVersion(unsigned(Classic)+1);install(next);add(2,next);add(1,next);
    cgl::CharacterCatalogScan scan(readers,8);finish(scan,1);auto result=scan.take();
    ASSERT_EQ(result.size(),5);EXPECT_EQ(result[0].serial,3);EXPECT_EQ(result[1].serial,11);EXPECT_EQ(result[2].serial,19);
    EXPECT_EQ(result[3].serial,1);EXPECT_EQ(result[3].version,next);EXPECT_EQ(result[4].serial,2);
    for(const auto& key:result) {EXPECT_EQ(key.direction,key.serial%8);EXPECT_EQ(key.motion,0);}
    EXPECT_EQ(result,cgl::ReadCharacterCatalog(readers,8));
}
TEST_F(CatalogTest, PreservesMotionFrameCanvasAndPayloadFilters) {
    for(uint32_t i=0;i<7;++i) add(i);
    data->entries[0].motionMap.erase({cgl::DirectionTypes(7),cgl::MotionTypes(1)});
    data->entries[1].motionMap.at({cgl::DirectionTypes(0),cgl::MotionTypes(1)}).duration=0;
    data->entries[2].motionMap.at({cgl::DirectionTypes(2),cgl::MotionTypes(0)}).motionGraphicsSerialNums.clear();
    data->entries[3].motionMap.at({cgl::DirectionTypes(3),cgl::MotionTypes(0)}).motionGraphicsSerialNums.resize(257,1);
    data->entries[4].motionMap.at({cgl::DirectionTypes(4),cgl::MotionTypes(0)}).motionGraphicsSerialNums={0};
    data->entries[5].motionMap.at({cgl::DirectionTypes(5),cgl::MotionTypes(0)}).motionGraphicsSerialNums={99};
    cgl::CharacterCatalogScan scan(readers);finish(scan);auto result=scan.take();
    ASSERT_EQ(result.size(),1);EXPECT_EQ(result[0].serial,6);
}
TEST_F(CatalogTest, BusyReaderReturnsWithoutWorkAndCanRetry) {
    add(1);cgl::CharacterCatalogScan scan(readers);
    std::promise<void> locked,release;auto released=release.get_future();
    auto holder=std::async(std::launch::async,[&]{std::lock_guard lock(*readers.queryMutex);locked.set_value();released.wait();});
    locked.get_future().wait();
    EXPECT_FALSE(scan.advance());EXPECT_EQ(info->queries,0);EXPECT_EQ(scan.size(),0);
    release.set_value();holder.get();finish(scan);EXPECT_EQ(scan.take().size(),1);
}
TEST_F(CatalogTest, IncompleteScanCanBeDestroyedAndRestarted) {
    for(uint32_t i=0;i<64;++i) add(i);
    {cgl::CharacterCatalogScan scan(readers);EXPECT_FALSE(scan.advance());EXPECT_FALSE(scan.advance());EXPECT_EQ(scan.size(),16);}
    EXPECT_EQ(cgl::ReadCharacterCatalog(readers,2).size(),2);EXPECT_EQ(info->queries,18);
}
TEST_F(CatalogTest, MissingFailedAndEmptyReadersTerminateWithError) {
    cgl::CharacterCatalogScan missing(readers);
    EXPECT_THROW(finish(missing),std::runtime_error);
    add(1);info->fail=true;cgl::CharacterCatalogScan failed(readers);
    EXPECT_THROW(finish(failed),std::runtime_error);
    info->fail=false;data->fail=true;cgl::CharacterCatalogScan corrupt(readers);
    EXPECT_THROW(finish(corrupt),std::runtime_error);
}
TEST_F(CatalogTest, RejectsInvalidLimitsBatchAndEarlyPublication) {
    EXPECT_THROW(cgl::CharacterCatalogScan(readers,0),std::invalid_argument);
    EXPECT_THROW(cgl::CharacterCatalogScan(readers,4097),std::invalid_argument);
    cgl::CharacterCatalogScan scan(readers);
    EXPECT_THROW(scan.advance(0),std::invalid_argument);
    EXPECT_THROW(scan.take(),std::logic_error);
}
