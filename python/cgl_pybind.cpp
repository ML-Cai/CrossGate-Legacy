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
#include "cgl/resources/palette_reader.h"
#include "cgl/utils/enum_string_helper.h"

namespace py = pybind11;

// -----------------------------------------------------------------------------
namespace {

template<typename T>
inline void remove(std::vector<T>* v, T target) {
    auto it = std::find(v->begin(), v->end(), target);
    if (it != v->end()) {
        v->erase(it);
    }
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
inline void* acquire_ptr(py::object obj) {
    if (obj.is_none()) return nullptr;

    if (py::isinstance<py::capsule>(obj) == false) {
        throw std::invalid_argument("Expected py::capsule for arguments");
    }
    return obj.cast<py::capsule>().get_pointer();
}


struct PaletteData256Holder {
    cgl::PaletteData256 data;
};

class Session {
public:
    Session();

    size_t acquire_graphics_data_count(
        cgl::CrossGateVersion version);

    std::shared_ptr<cgl::GraphicsResourceData> acquire_graphics_data(
        cgl::CrossGateVersion version,
        int32_t               idx);

    std::shared_ptr<PaletteData256Holder> acquire_env_palette(
        cgl::EnvironmentPaletteTypes envPalette);

    std::vector<uint8_t> apply_palette(
        void* pGfxDataHandle,
        void* pPlaetteHandle);

    std::vector<int32_t> acquire_available_serial_nums(
        cgl::CrossGateVersion version);

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

    cgl::IGraphicsResourceFileInfoReader* acquireInfoReader(
        cgl::CrossGateVersion version);

    cgl::IGraphicsResourceFileDataReader* acquireDataReader(
        cgl::CrossGateVersion version);

    cgl::IPaletteReader* acquirePaletteReader(
        cgl::EnvironmentPaletteTypes envPalette);
};

}   // namespace

Session::Session() {
    cgl::SettingsLoader loader;
    settingLoader_.load();
    pSettings_ = settingLoader_.settings();
}

cgl::IGraphicsResourceFileInfoReader* Session::acquireInfoReader(
    cgl::CrossGateVersion version
) {
    auto& reader = infoReaderMap_[version];
    if (!reader) {
        reader = cgl::IGraphicsResourceFileInfoReader::create({
            .pSettings = pSettings_,
            .version   = version
        });
        if (!reader) {
            throw std::invalid_argument("Failed to create the reader");
        }
        if (reader->load() != cgl::Results::Success) {
            throw std::invalid_argument("Failed to load the reader");
        }
    }
    return reader.get();
}

cgl::IGraphicsResourceFileDataReader* Session::acquireDataReader(
    cgl::CrossGateVersion version
) {
    auto& reader = dataReaderMap_[version];
    if (!reader) {
        reader = cgl::IGraphicsResourceFileDataReader::create({
            .pSettings = pSettings_,
            .version   = version
        });
        if (!reader) {
            throw std::invalid_argument("Failed to create the reader");
        }
        if (reader->load() != cgl::Results::Success) {
            throw std::invalid_argument("Failed to load the reader");
        }
    }
    return reader.get();
}

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

size_t Session::acquire_graphics_data_count(
    cgl::CrossGateVersion version
) {
    return acquireInfoReader(version)->infoCount();
}

std::shared_ptr<cgl::GraphicsResourceData> Session::acquire_graphics_data(
    cgl::CrossGateVersion version,
    int32_t               idx
) {
    auto infoReader = acquireInfoReader(version);
    auto dataReader = acquireDataReader(version);

    cgl::GraphicsResourceSerialNum resIdx {
        .type    = cgl::GraphicsResourceSerialNumTypes::GraphicsSerialNum,
        .version = cgl::CrossGateVersion::CG_VERSION_Classic,
        .value   = idx
    };
    cgl::GraphicsResourceInfo info;
    if (infoReader->query(resIdx, &info) != cgl::Results::Success) {
        printf("Failed to query info %s:%d\n", cgl::GetString(version), idx);
        return nullptr;
    }

    auto data = std::make_shared<cgl::GraphicsResourceData>();
    if (dataReader->query(info, data.get()) != cgl::Results::Success) {
        printf("Failed to query data %s:%d\n", cgl::GetString(version), idx);
        return nullptr;
    }

    return data;
}

std::shared_ptr<PaletteData256Holder> Session::acquire_env_palette(
    cgl::EnvironmentPaletteTypes envPalette
) {
    auto infoReader = acquirePaletteReader(envPalette);

    auto data = std::make_shared<PaletteData256Holder>();
    if (infoReader->read(envPalette, &data->data) != cgl::Results::Success) {
        printf("Failed to query palette %s\n", cgl::GetString(envPalette));
        return nullptr;
    }
    return data;
}

std::vector<int32_t> Session::acquire_available_serial_nums(
    cgl::CrossGateVersion version
) {
    auto reader = acquireInfoReader(version);

    std::vector<int32_t> ret;
    if (reader->queryAvailableSerialNums(&ret) != cgl::Results::Success) {
        throw std::invalid_argument("Failed to query available Serial Numbers");
    }
    return ret;
}

std::vector<uint8_t> Session::apply_palette(
    void* pGfxDataHandle,
    void* pPletteHandle
) {
    auto pGfxResData = static_cast<cgl::GraphicsResourceData *>(pGfxDataHandle);
    auto pPaletteHolder = static_cast<PaletteData256Holder *>(pPletteHandle);

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
            "acquire_graphics_data",
            [](Session& self,
               cgl::CrossGateVersion version, size_t idx) -> py::object {
                auto ptr = self.acquire_graphics_data(version, idx);
                if (ptr) {
                    return py::cast(ptr);
                }
                return py::none();
            })

        .def(
            "acquire_env_palette",
            [](Session& self,
               cgl::EnvironmentPaletteTypes envPalette) -> py::object {
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
            [](Session& self,
               std::shared_ptr<cgl::GraphicsResourceData> gfx_data,
               std::shared_ptr<PaletteData256Holder> palette_data) -> py::object {
                auto img = self.apply_palette(gfx_data.get(), palette_data.get());
                auto* buffer = new std::vector<uint8_t>(std::move(img));
                uint8_t* data_ptr = buffer->data();

                py::capsule free_when_done(buffer, [](void* f) {
                    delete static_cast<std::vector<uint8_t>*>(f);
                });

                return py::array_t<uint8_t>(
                    { gfx_data->height, gfx_data->width, 4 },
                    { gfx_data->width * 4, 4, 1 },
                    data_ptr,
                    free_when_done
                );
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
    // Define cgl::CrossGateVersion
    // -------------------------------------------------------------------------
    py::class_<cgl::GraphicsResourceData, std::shared_ptr<cgl::GraphicsResourceData>>(m, "GraphicsResourceData")
        .def_readonly("version", &cgl::GraphicsResourceData::version)
        .def_readonly("width", &cgl::GraphicsResourceData::width)
        .def_readonly("height", &cgl::GraphicsResourceData::height);

    py::class_<PaletteData256Holder, std::shared_ptr<PaletteData256Holder>>(m, "PaletteData256Holder")
        .def(py::init<>());
}
