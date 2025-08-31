// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <vector>
#include <cstdint>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include "cgl/core/version.h"
#include "cgl/settings/settings_loader.h"
#include "cgl/settings/settings.h"
#include "cgl/resources/resource_types.h"
#include "cgl/resources/graphics_resource_file_info_reader.h"
#include "cgl/resources/graphics_resource_file_data_reader.h"
#include "cgl/resources/anime_resource_info_reader.h"
#include "cgl/resources/anime_resource_data_reader.h"
#include "cgl/resources/palette_reader.h"
#include "cgl/character/direction_types.h"
#include "cgl/character/motion_types.h"
#include "cgl/utils/enum_string_helper.h"
#include "utils/logger.h"
#include "holders.h"

namespace py = pybind11;


namespace {

// -----------------------------------------------------------------------------
template<typename T>
inline void remove(std::vector<T>* v, T target) {
    auto it = std::find(v->begin(), v->end(), target);
    if (it != v->end()) {
        v->erase(it);
    }
}

// -----------------------------------------------------------------------------
template <typename T>
std::string type_name() {
#if defined(__clang__) || defined(__GNUC__)
    return abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, nullptr);
#elif defined(_MSC_VER)
    return typeid(T).name();
#else
    return typeid(T).name();
#endif
}

// -----------------------------------------------------------------------------
std::vector<cgl::EnvironmentPaletteTypes> GetTypes_EnvironmentPaletteTypes() {
    std::vector<cgl::EnvironmentPaletteTypes> result;
#define CGL_X(name) result.push_back(cgl::EnvironmentPaletteTypes::name);
    EnvironmentPaletteTypes_ENUM_LIST
#undef CGL_X
    remove(&result, cgl::EnvironmentPaletteTypes::Unknown);

    return result;
}

// -----------------------------------------------------------------------------
std::vector<cgl::CrossGateVersion> GetTypes_CrossGateVersion() {
    std::vector<cgl::CrossGateVersion> result;
#define CGL_X(name) result.push_back(cgl::CrossGateVersion::name);
    CrossGateVersion_ENUM_LIST
#undef CGL_X
    remove(&result, cgl::CrossGateVersion::CG_VERSION_UNKNOWN);
    remove(&result, cgl::CrossGateVersion::Count);

    return result;
}

// -----------------------------------------------------------------------------
class Session {
public:
    Session();

    size_t acquire_graphics_data_count(
        cgl::CrossGateVersion version);

    cgl::py::PaletteData256Holder::Ptr acquire_env_palette(
        cgl::EnvironmentPaletteTypes envPalette);

    std::vector<uint8_t> apply_palette(
        void* pGfxDataHandle,
        void* pPlaetteHandle);

    std::vector<int32_t> acquire_available_serial_nums(
        cgl::CrossGateVersion version);

    cgl::py::GraphicsResourceBundleHolder::Ptr
    acquire_graphics_resource(
        cgl::CrossGateVersion version,
        uint32_t              serial_num);

    cgl::py::AnimeResourceDataHolder::Ptr
    acquire_anime_resource(
        cgl::CrossGateVersion version,
        uint32_t              anime_serial_num);

private:
    int width_, height_;
    std::vector<uint8_t> rgbData_;

    const cgl::Settings* pSettings_;
    cgl::SettingsLoader settingLoader_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IGraphicsResourceFileInfoReader::Ptr> infoReaderMap_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IGraphicsResourceFileDataReader::Ptr> dataReaderMap_;

    cgl::IPaletteReader::Ptr paletteReader_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IAnimeResourceInfoReader::Ptr> animeInfoReaderMap_;

    std::unordered_map<
        cgl::CrossGateVersion,
        cgl::IAnimeResourceDataReader::Ptr> animeDataReaderMap_;

    cgl::IPaletteReader* acquirePaletteReader(
        cgl::EnvironmentPaletteTypes envPalette);

    template <typename ReaderInterface, typename MapType>
    ReaderInterface* acquireReader(
        MapType& readerMap,
        const cgl::CrossGateVersion version);
};

}   // namespace


// -----------------------------------------------------------------------------
Session::Session() {
    cgl::SettingsLoader loader;
    settingLoader_.load();
    pSettings_ = settingLoader_.settings();
}

// -----------------------------------------------------------------------------
template <typename ReaderInterface, typename MapType>
ReaderInterface* Session::acquireReader(
    MapType& readerMap,
    const cgl::CrossGateVersion version
) {
    auto& reader = readerMap[version];
    if (!reader) {
        LOGD("Create `{}` for version `{}`", type_name<ReaderInterface>(),
             cgl::GetString(version));

        reader = ReaderInterface::create({
            .pSettings = pSettings_,
            .version   = version
        });

        if (!reader) {
            std::stringstream ss;
            ss << "Failed to create " << type_name<ReaderInterface>()
               << " for version " << cgl::GetString(version);
            throw std::invalid_argument(ss.str());
        }
        if (reader->load() != cgl::Results::Success) {
            std::stringstream ss;
            ss << "Failed to load " << type_name<ReaderInterface>()
               << " for version " << cgl::GetString(version);
            throw std::invalid_argument(ss.str());
        }
    }
    return reader.get();
}

// -----------------------------------------------------------------------------
cgl::IPaletteReader* Session::acquirePaletteReader(
    cgl::EnvironmentPaletteTypes envPalette
) {
    if (!paletteReader_) {
        paletteReader_ = cgl::IPaletteReader::create({
            .pSettings = pSettings_,
        });
        if (!paletteReader_) {
            throw std::invalid_argument("Failed to create the reader");
        }
    }
    return paletteReader_.get();
}

// -----------------------------------------------------------------------------
size_t Session::acquire_graphics_data_count(
    cgl::CrossGateVersion version
) {
    auto reader = acquireReader<cgl::IGraphicsResourceFileInfoReader>(
                    infoReaderMap_, version);
    return reader->infoCount();
}

// -----------------------------------------------------------------------------
cgl::py::PaletteData256Holder::Ptr Session::acquire_env_palette(
    cgl::EnvironmentPaletteTypes envPalette
) {
    auto infoReader = acquirePaletteReader(envPalette);

    auto data = std::make_shared<cgl::py::PaletteData256Holder>();
    if (infoReader->read(envPalette, &data->data) != cgl::Results::Success) {
        LOGE("Failed to query palette {}", cgl::GetString(envPalette));
        return nullptr;
    }
    return data;
}

// -----------------------------------------------------------------------------
std::vector<int32_t> Session::acquire_available_serial_nums(
    cgl::CrossGateVersion version
) {
    auto reader = acquireReader<cgl::IGraphicsResourceFileInfoReader>(
                    infoReaderMap_, version);

    std::vector<int32_t> ret;
    if (reader->queryAvailableSerialNums(&ret) != cgl::Results::Success) {
        throw std::invalid_argument("Failed to query available Serial Numbers");
    }
    return ret;
}

// -----------------------------------------------------------------------------
std::vector<uint8_t> Session::apply_palette(
    void* pGfxDataHandle,
    void* pPletteHandle
) {
    auto pHolder = static_cast<cgl::py::GraphicsResourceBundleHolder *>(pGfxDataHandle);
    auto pPaletteHolder = static_cast<cgl::py::PaletteData256Holder *>(pPletteHandle);
    auto pGfxResData = &pHolder->data;

    std::vector<uint8_t> img;
    size_t count = pGfxResData->width * pGfxResData->height;
    img.resize(count * 4);
    uint8_t* pData = pGfxResData->pData.get();
    uint8_t* pImg = img.data();
    uint8_t* pEnd = img.data() + count * 4;

    // const cgl::PaletteData256* pPalette = &pPaletteHolder->data;
    auto pPalette = reinterpret_cast<uint8_t*>(pPaletteHolder->data.data());
    if (pGfxResData->pPaletteData != nullptr) {
        pPalette = pGfxResData->pPaletteData;
    }

    while (pImg < pEnd) {
        uint32_t paletteDataIdx = *(pData);
        uint8_t* pBGR = &pPalette[paletteDataIdx * 3];
        pImg[0] = pBGR[0];
        pImg[1] = pBGR[1];
        pImg[2] = pBGR[2];
        pImg[3] = (paletteDataIdx == 0)? 0 : 255;
        pImg+=4;
        pData++;
    }

    return img;
}

// -----------------------------------------------------------------------------
cgl::py::AnimeResourceDataHolder::Ptr Session::acquire_anime_resource(
    cgl::CrossGateVersion version,
    uint32_t              serial_num
) {
    auto infoReader = acquireReader<cgl::IAnimeResourceInfoReader>(
                        animeInfoReaderMap_, version);
    auto dataReader = acquireReader<cgl::IAnimeResourceDataReader>(
                        animeDataReaderMap_, version);

    if (!infoReader || !dataReader) {
        LOGE("Failed to acquire anime resource readers for version {}",
               cgl::GetString(version));
        return nullptr;
    }

    cgl::AnimeResourceInfo info;
    cgl::AnimeResourceSerialNum sn {
        .version = version,
        .value   = serial_num
    };

    // query info first
    if (infoReader->query(sn, &info) != cgl::Results::Success) {
        LOGE("Failed to query anime info via S/N [{}]", cgl::toStr(sn));
        return nullptr;
    }

    // query anime data to the holder
    auto holder = std::make_shared<cgl::py::AnimeResourceDataHolder>();
    if (dataReader->query(info, &holder->animeData) != cgl::Results::Success) {
        LOGE("Failed to query anime data via S/N [{}]", cgl::toStr(sn));
        return nullptr;
    }

    return holder;
}

// -----------------------------------------------------------------------------
cgl::py::GraphicsResourceBundleHolder::Ptr Session::acquire_graphics_resource(
    cgl::CrossGateVersion version,
    uint32_t              serial_num
) {
    auto infoReader = acquireReader<cgl::IGraphicsResourceFileInfoReader>(
                        infoReaderMap_, version);
    auto dataReader = acquireReader<cgl::IGraphicsResourceFileDataReader>(
                        dataReaderMap_, version);

    if (!infoReader || !dataReader) {
        LOGE("Failed to acquire graphics resource readers for version {}",
               cgl::GetString(version));
        return nullptr;
    }

    auto holder = std::make_shared<cgl::py::GraphicsResourceBundleHolder>();

    // query info first
    cgl::GraphicsResourceSerialNum sn {
        .version = version,
        .value = serial_num
    };

    if (infoReader->query(sn, &holder->info) != cgl::Results::Success) {
        LOGE("Failed to query graphics info via S/N [{}]", cgl::toStr(sn));
        return nullptr;
    }

    if (dataReader->query(holder->info, &holder->data) != cgl::Results::Success) {
        LOGE("Failed to query graphics data via S/N [{}]", cgl::toStr(sn));
        return nullptr;
    }

    return holder;
}

// -----------------------------------------------------------------------------
// Define pybind
// -----------------------------------------------------------------------------
PYBIND11_MODULE(cgl, m) {
    // -------------------------------------------------------------------------
    // Session
    // -------------------------------------------------------------------------
    py::class_<Session>(m, "Session")
        .def(py::init<>())

        .def(
            "acquire_graphics_data_count",
            &Session::acquire_graphics_data_count)

        .def(
            "acquire_graphics_resource",
            [](Session&              self,
               cgl::CrossGateVersion version,
               uint32_t              serial_num
            ) -> py::object {
                auto ptr = self.acquire_graphics_resource(version, serial_num);
                if (ptr) {
                    return py::cast(ptr);
                }
                return py::none();
            })

        .def(
            "acquire_env_palette",
            [](Session&                     self,
               cgl::EnvironmentPaletteTypes envPalette
            ) -> py::object {
                auto ptr = self.acquire_env_palette(envPalette);
                if (ptr) {
                    return py::cast(ptr);
                }
                return py::none();
            })

        .def(
            "acquire_available_serial_nums",
            [](Session& self,
               cgl::CrossGateVersion version) {
                return self.acquire_available_serial_nums(version);
            })

        .def(
            "apply_palette",
            [](Session&                                   self,
               cgl::py::GraphicsResourceBundleHolder::Ptr gfx_data,
               cgl::py::PaletteData256Holder::Ptr         palette_data
            ) -> py::object {
                auto img = self.apply_palette(gfx_data.get(), palette_data.get());
                auto* buffer = new std::vector<uint8_t>(std::move(img));
                uint8_t* data_ptr = buffer->data();

                py::capsule free_when_done(buffer, [](void* f) {
                    delete static_cast<std::vector<uint8_t>*>(f);
                });

                return py::array_t<uint8_t>(
                    { gfx_data->info.height, gfx_data->info.width, 4 },
                    { gfx_data->info.width * 4, 4, 1 },
                    data_ptr,
                    free_when_done
                );
            })

        .def(
            "acquire_anime_resource",
            [](Session&              self,
               cgl::CrossGateVersion version,
               size_t                serial_num
            ) -> py::object {
                auto ptr = self.acquire_anime_resource(version, serial_num);
                if (ptr) {
                    return py::cast(ptr);
                }
                return py::none();
            })
        ;

    // -------------------------------------------------------------------------
    // Define cgl::CrossGateVersion
    // -------------------------------------------------------------------------
    m.def("CrossGateVersionList", &GetTypes_CrossGateVersion);
    py::enum_<cgl::CrossGateVersion>(m, "CrossGateVersion", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::CrossGateVersion::name)
            CrossGateVersion_ENUM_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::EnvironmentPaletteTypes
    // -------------------------------------------------------------------------
    m.def("EnvironmentPaletteTypesList", &GetTypes_EnvironmentPaletteTypes);
    py::enum_<cgl::EnvironmentPaletteTypes>(m, "EnvironmentPaletteTypes", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::EnvironmentPaletteTypes::name)
            EnvironmentPaletteTypes_ENUM_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::DirectionTypes
    // -------------------------------------------------------------------------
    py::enum_<cgl::DirectionTypes>(m, "DirectionTypes", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::DirectionTypes::name)
            CGL_DIRECTION_TYPES_ENUM_FULL_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::MotionTypes
    // -------------------------------------------------------------------------
    py::enum_<cgl::MotionTypes>(m, "MotionTypes", py::arithmetic())
        #define CGL_X(name) .value(#name, cgl::MotionTypes::name)
            CGL_MOTION_TYPES_ENUM_FULL_LIST
        #undef CGL_X
        .export_values();

    // -------------------------------------------------------------------------
    // Define cgl::GraphicsResourceBundleHolder
    // -------------------------------------------------------------------------
    py::class_<cgl::py::GraphicsResourceBundleHolder,
               cgl::py::GraphicsResourceBundleHolder::Ptr>(m, "GraphicsResourceBundleHolder")
        .def("offset",
             &cgl::py::GraphicsResourceBundleHolder::offset,
             "Return offset as (offsetX, offsetY) in current graphics resource");
        // .def_readonly("version", &cgl::GraphicsResourceBundleHolder::version)
        // .def_readonly("width", &cgl::GraphicsResourceBundleHolder::width)
        // .def_readonly("height", &cgl::GraphicsResourceBundleHolder::height)
        ;

    // -------------------------------------------------------------------------
    // Define holders
    // -------------------------------------------------------------------------
    py::class_<cgl::py::PaletteData256Holder, cgl::py::PaletteData256Holder::Ptr>(m, "PaletteData256Holder")
        .def(py::init<>());

    py::class_<cgl::py::AnimeResourceDataHolder, cgl::py::AnimeResourceDataHolder::Ptr>(m, "AnimeResourceDataHolder")
        .def(py::init<>())
        .def("keys", &cgl::py::AnimeResourceDataHolder::keys)
        .def("acquire", &cgl::py::AnimeResourceDataHolder::acquire);

    py::class_<cgl::AnimeMotionDesc>(m, "AnimeMotionDesc")
        .def_readonly("version", &cgl::AnimeMotionDesc::version)
        .def_readonly("direction", &cgl::AnimeMotionDesc::direction)
        .def_readonly("motion", &cgl::AnimeMotionDesc::motion)
        .def_readonly("duration", &cgl::AnimeMotionDesc::duration)
        .def_readonly("motionGraphicsSerialNums", &cgl::AnimeMotionDesc::motionGraphicsSerialNums)
        .def("__repr__", [](const cgl::AnimeMotionDesc& desc) {
            return "<AnimeMotionDesc: " + std::to_string(static_cast<int>(desc.direction)) +
                   ", " + std::to_string(static_cast<int>(desc.motion)) + ">";
        });
}
