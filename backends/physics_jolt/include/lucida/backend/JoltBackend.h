#pragma once
// Factory for the Jolt physics backend. Swapping in Bullet is a change to one
// line in the app, because nothing above this header names Jolt.

#include "lucida/physics/PhysicsBackend.h"

#include <memory>

namespace lucida {

std::unique_ptr<IPhysicsBackend> CreateJoltBackend();

} // namespace lucida
