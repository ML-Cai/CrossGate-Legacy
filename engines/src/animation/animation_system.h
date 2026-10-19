#pragma once
namespace cgl {
class ECSCore;
// Deterministic time input; no dependency on the renderer or wall clock.
void UpdateAnimations(ECSCore& world, double deltaSeconds);
}
