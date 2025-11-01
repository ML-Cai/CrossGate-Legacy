// =============================================================================
//   The MIT License (MIT)
//
//   Copyright (c) 2024-2025 MengLun,Cai
//
//   All rights reserved.
// =============================================================================

#pragma once

#include <string>
#include <cassert>
#include "cgl/common/states.h"
#include "engine/error_system.h"

namespace cgl {

#define CGL_TRANSIT_STATE(pStatePtr, targetState) \
    do { \
        (pStatePtr)->state = targetState; \
    } while (0)


#define CGL_IS_STATE_ERROR(pStatePtr) \
    ((pStatePtr == nullptr) || (pStatePtr)->state == cgl::StateTypes::ERROR)


#define CGL_RAISE_IF_STATE_ERROR(pCheckStatePtr, pRaiseToStatePtr) \
    do {                                                           \
        if ((pCheckStatePtr)->state == cgl::StateTypes::ERROR) {   \
            cgl::RaiseError(pRaiseToStatePtr,                      \
                            (pCheckStatePtr)->lastError);          \
            return;                                                \
        }                                                          \
    } while (0)



// -----------------------------------------------------------------------------
template <typename STATE>
struct StateTransitionHint {
    STATE* pState;
    cgl::StateTypes to;
};

// -----------------------------------------------------------------------------
template <typename SYSTEM, typename STATE, typename STATE_2, typename STATE_3>
struct UpdateSystemParams {
    SYSTEM* pSystem;
    cgl::ECSCore* pEcs;

    STATE* pCheckState;

    StateTransitionHint<STATE_2> onSuccess;

    STATE_3* pOnErrorState;
};

// -------------------------------------------------------------
template <typename SYSTEM_T, typename ECS_T, typename S1, typename S2, typename S3>
UpdateSystemParams(
    SYSTEM_T*,
    ECS_T*,
    S1*,
    cgl::StateTransitionHint<S2>,
    S3*
) -> UpdateSystemParams<SYSTEM_T, S1, S2, S3>;

// -------------------------------------------------------------
template <typename SYSTEM, typename STATE, typename STATE_2, typename STATE_3>
inline bool UpdateSystemAndTransition(
    const cgl::UpdateSystemParams<SYSTEM, STATE, STATE_2, STATE_3>& p
) {
    p.pSystem->update(p.pEcs);

    if (p.pCheckState->state == cgl::StateTypes::ERROR) {
        cgl::RaiseError(p.pOnErrorState,
                        "System transition error, reason: ",
                        p.pCheckState->lastError);
        return false;
    }

    p.onSuccess.pState->state = p.onSuccess.to;

    return true;
}

#define CGL_UPDATE_SYS_AND_TRANSIT_STATE(...) \
    (cgl::UpdateSystemAndTransition(cgl::UpdateSystemParams __VA_ARGS__))

} // namespace cgl
