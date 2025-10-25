// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <memory>
#include <array>
#include "cgl/trace/logger.h"
#include "cgl/common/formatters.h"
#include "engine/ecs.h"
#include "engine/error_system.h"
#include "engine/engine_components.h"
#include "assets/assets_components.h"
#include "assets/assets_system.h"

using cgl::component::EngineState;
using cgl::component::AssetsReaderState;

// -----------------------------------------------------------------------------
namespace {

#define CGL_X(name) cgl::CrossGateVersion::name,
static std::vector<cgl::CrossGateVersion> CGL_VERSIONS {
        CGL_VERSION_TYPES_VALID_LIST
    };
#undef CGL_X

// -----------------------------------------------------------------------------
template<typename ReaderT>
std::future<typename ReaderT::Ptr>
LaunchInitReaderAsyncTask(
    cgl::Settings*        pSettings,
    cgl::CrossGateVersion version
) {
    return std::async(
        std::launch::async,
        [pSettings, version]() -> ReaderT::Ptr {
            auto p = ReaderT::create({
                .pSettings = pSettings,
                .version   = version,
            });
            if (!p || p->load() != cgl::Results::Success) {
                return nullptr;
            }
            return p;
        }
    );
}

// -----------------------------------------------------------------------------
template<typename T>
bool IsFutureReady(
    std::future<T>& future,
    bool* anyFail
) {
    const auto timeout = std::chrono::seconds(0);
    if (future.valid() == false) {
        *anyFail = true;
        LOGE("future is invalid");
        return false;
    }
    if (future.wait_for(timeout) != std::future_status::ready) {
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
template<typename T>
bool IsFuturesAllReady(
    std::vector<std::future<T>>& futures,
    bool* anyFail
) {
    const auto timeout = std::chrono::seconds(0);
    for (auto& future : futures) {
        if (IsFutureReady(future, anyFail) == false) {
            return false;
        }
    }
    return true;
}

}   // namespace

// -----------------------------------------------------------------------------
cgl::AssetsReaderInitSystem::AssetsReaderInitSystem() {
    updater_ = &AssetsReaderInitSystem::initStage;
}

// -----------------------------------------------------------------------------
void cgl::AssetsReaderInitSystem::initStage(cgl::ECSCore* pECS) {
    LOGD("Launch tasks to init assets readers");

    auto pEngineState   = pECS->getSingleton<EngineState>();
    pAssetsReaderState_ = pECS->getSingleton<AssetsReaderState>();
    assert(pEngineState != nullptr);
    assert(pAssetsReaderState_ != nullptr);


    // check state
    if ((pAssetsReaderState_->state != cgl::StateTypes::UNKNOWN) &&
        (pAssetsReaderState_->state != cgl::StateTypes::INITIALIZING)) {
        cgl::RaiseError(pAssetsReaderState_, "The `AssetsReaderInitSystem` "
            "updates data and systems only when the `AssetsReaderState` is in "
            "the `UNKNOWN` or `INITIALIZING` state. Please verify the flow.");
        return;
    }

    futures_IAnimeInfoReader_.resize(CGL_VERSIONS.size());
    futures_IAnimeDataReader_.resize(CGL_VERSIONS.size());
    futures_IGraphicsDataReader_.resize(CGL_VERSIONS.size());
    futures_IGraphicsInfoReader_.resize(CGL_VERSIONS.size());

    // create task to init reader in background
    auto pSettings = pEngineState->settings.get();

    for (const auto version : CGL_VERSIONS) {
        LOGD("Launch tasks to init readers for: " << version);
        const auto idx = static_cast<size_t>(version);
        futures_IAnimeInfoReader_[idx] = LaunchInitReaderAsyncTask<
            cgl::IAnimeInfoReader>(pSettings, version);

        futures_IAnimeDataReader_[idx] = LaunchInitReaderAsyncTask<
            cgl::IAnimeDataReader>(pSettings, version);

        futures_IGraphicsInfoReader_[idx] = LaunchInitReaderAsyncTask<
            cgl::IGraphicsInfoReader>(pSettings, version);

        futures_IGraphicsDataReader_[idx] = LaunchInitReaderAsyncTask<
            cgl::IGraphicsDataReader>(pSettings, version);
    }

    futures_IPaletteReader_ = std::async(
        std::launch::async,
        [pSettings]() -> cgl::IPaletteReader::Ptr {
            auto p = cgl::IPaletteReader::create({
                .pSettings = pSettings,
            });
            if (p == nullptr)
                return nullptr;
            return p;
        }
    );

    // swtich to next stage
    updater_ = &AssetsReaderInitSystem::checkStage;
}

// -----------------------------------------------------------------------------
void cgl::AssetsReaderInitSystem::checkStage(cgl::ECSCore* pECS) {
    bool anyFail = false;

    // check all futures all ready?
    if ((IsFuturesAllReady(futures_IAnimeInfoReader_, &anyFail) == false) ||
        (IsFuturesAllReady(futures_IAnimeDataReader_, &anyFail) == false) ||
        (IsFuturesAllReady(futures_IGraphicsInfoReader_, &anyFail) == false) ||
        (IsFuturesAllReady(futures_IGraphicsDataReader_, &anyFail) == false) ||
        (IsFutureReady(futures_IPaletteReader_, &anyFail) == false)) {
        if (anyFail) {
            cgl::RaiseError(pAssetsReaderState_,
                "One of future in reader init task is invalid.");
            return;
        }
        return;
    }

    // move all readers to the state after all ready.
    LOGI("All assets reader initialization tasks ready");

    for (const auto version : CGL_VERSIONS) {
        const auto idx = static_cast<size_t>(version);
        pAssetsReaderState_->pAnimeInfoReader[version]
            = futures_IAnimeInfoReader_[idx].get();

        pAssetsReaderState_->pAnimeDataReader[version]
            = futures_IAnimeDataReader_[idx].get();

        pAssetsReaderState_->pGfxDataReader[version]
            = futures_IGraphicsDataReader_[idx].get();

        pAssetsReaderState_->pGfxInfoReader[version]
            = futures_IGraphicsInfoReader_[idx].get();
    }
    pAssetsReaderState_->pPaletteReader = futures_IPaletteReader_.get();

    // swtich to next stage
    updater_ = &AssetsReaderInitSystem::nopStage;
}

// -----------------------------------------------------------------------------
void cgl::AssetsReaderInitSystem::nopStage(cgl::ECSCore* pECS) {
    // NOP
}

// -----------------------------------------------------------------------------
void cgl::AssetsReaderInitSystem::update(cgl::ECSCore* pECS) {
    (this->*updater_)(pECS);
}
