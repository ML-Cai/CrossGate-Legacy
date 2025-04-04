// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include <filesystem>
#include "cgl/settings/settings.h"
#include "cgl/settings/settings_loader.h"

using cgl::SettingsLoader;

// -----------------------------------------------------------------------------
// cgl::SettingsLoader::Impl
// -----------------------------------------------------------------------------
class cgl::SettingsLoader::Impl {
 public:
    Impl();

    ~Impl();

    cgl::Results load() noexcept;

    const cgl::Settings* settings() const noexcept;

 private:
    bool loaded_;
    cgl::Settings setting_;
};

// -----------------------------------------------------------------------------
SettingsLoader::Impl::Impl()
    : loaded_(false) {}

// -----------------------------------------------------------------------------
SettingsLoader::Impl::~Impl() {
}

// -----------------------------------------------------------------------------
cgl::Results SettingsLoader::Impl::load() noexcept {
    // load config via direct assign the value in this moment.
    setting_.crossGateDataRootDir = "C:\\Users\\Dana\\Downloads\\cg_data\\";

    loaded_ = true;
    return cgl::Results::Success;
}

// -----------------------------------------------------------------------------
const cgl::Settings* SettingsLoader::Impl::settings() const noexcept {
    if (loaded_ == false) {
        return nullptr;
    }
    return &setting_;
}

// -----------------------------------------------------------------------------
// cgl::SettingsLoader's Impl
// -----------------------------------------------------------------------------
SettingsLoader::SettingsLoader() {
    pImpl_ = std::make_unique<cgl::SettingsLoader::Impl>();
}

// -----------------------------------------------------------------------------
SettingsLoader::~SettingsLoader() {
}

// -----------------------------------------------------------------------------
cgl::Results SettingsLoader::load() noexcept {
    return pImpl_->load();
}

// -----------------------------------------------------------------------------
const cgl::Settings* SettingsLoader::settings() const noexcept {
    return pImpl_->settings();
}
