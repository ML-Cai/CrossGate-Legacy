// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#include "cgl/engine/engine.h"

// -----------------------------------------------------------------------------
int main(int, char *argv[]) {
    auto engine = cgl::IEngine::create();

    if (engine != nullptr) {
        engine->run();
    }

    return 0;
}
