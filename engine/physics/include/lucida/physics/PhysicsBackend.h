// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Physics backend interface (GEA 1.6.12).
//
// Jolt and Bullet are both good and both wrong to depend on directly: their
// types would leak into gameplay and pin the engine to one library forever.
// The engine sees this header; the app picks the implementation.

#include "lucida/core/container/Handle.h"
#include "lucida/core/math/Math.h"

#include <array>
#include <memory>

namespace lucida {

LUCIDA_DECLARE_HANDLE(BodyHandle);
LUCIDA_DECLARE_HANDLE(VehicleHandle);

enum class BodyType : u8 { Static, Dynamic, Kinematic };
enum class ShapeType : u8 { Box, Sphere, Capsule, Cylinder, Plane, Mesh };

struct BodyDesc {
    BodyType  type  = BodyType::Dynamic;
    ShapeType shape = ShapeType::Box;
    Vec3 half_extent{0.5f};   // box: half sizes; sphere/capsule: x is the radius
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    f32  mass        = 1.0f;
    f32  friction    = 0.5f;
    f32  restitution = 0.0f;
    bool is_sensor   = false;  // Trigger volume: detects overlaps without physical collision
    uint32_t user_data = 0;   // Entity ID or application context
};

struct RaycastHit {
    bool        has_hit = false;
    uint32_t    user_data = 0;
    BodyHandle  body;
    Vec3        point{0.0f};
    Vec3        normal{0.0f, 1.0f, 0.0f};
    f32         distance = 0.0f;
};

struct ContactPoint {
    Vec3 position{0.0f};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    f32  distance = 0.0f;
    f32  impulse  = 0.0f;
};

struct CollisionEvent {
    enum class Type : u8 { Begin, End, TriggerEnter, TriggerExit };
    Type        type = Type::Begin;
    BodyHandle  body_a;
    BodyHandle  body_b;
    uint32_t    user_data_a = 0;
    uint32_t    user_data_b = 0;
    ContactPoint contact;
};

// Driver intent, not forces. What a throttle of 0.7 means is the backend's job.
struct VehicleInput {
    f32  throttle  = 0.0f;   // 0..1
    f32  brake     = 0.0f;   // 0..1
    f32  steer     = 0.0f;   // -1 left .. +1 right
    bool handbrake = false;
};

struct WheelState {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    f32  steer_angle = 0.0f;
    f32  spin_angle  = 0.0f;
};

struct VehicleDesc {
    Vec3 position{0.0f, 1.0f, 0.0f};
    Vec3 half_extent{0.9f, 0.6f, 2.2f};
    f32  mass = 1500.0f;
    f32  wheel_radius = 0.35f;
    f32  wheel_width  = 0.25f;
    f32  max_engine_torque = 500.0f;
    f32  max_steer_angle   = 30.0f * kDegToRad;
};

struct VehicleState {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    std::array<WheelState, 4> wheels{};   // FL, FR, RL, RR
    f32 speed_kmh  = 0.0f;
    f32 engine_rpm = 0.0f;
    i32 gear       = 0;
};

class IPhysicsBackend {
public:
    virtual ~IPhysicsBackend() = default;

    virtual bool Init() = 0;
    virtual void Shutdown() = 0;

    // Fixed step, driven by the game loop. Never pass a variable dt here.
    virtual void Step(f32 fixed_dt) = 0;

    virtual BodyHandle CreateBody(const BodyDesc& desc) = 0;
    virtual void       DestroyBody(BodyHandle body) = 0;
    virtual Transform  GetBodyTransform(BodyHandle body) const = 0;
    virtual void       SetBodyTransform(BodyHandle body, const Transform& transform) = 0;

    virtual void       AddImpulse(BodyHandle body, const Vec3& impulse) = 0;
    virtual void       AddForce(BodyHandle body, const Vec3& force) = 0;
    virtual void       SetLinearVelocity(BodyHandle body, const Vec3& velocity) = 0;
    virtual Vec3       GetLinearVelocity(BodyHandle body) const = 0;

    virtual bool       CastRay(const Vec3& origin, const Vec3& direction, f32 max_distance, RaycastHit& out_hit) const = 0;
    virtual void       PopCollisionEvents(std::vector<CollisionEvent>& out_events) = 0;

    virtual VehicleHandle CreateVehicle(const VehicleDesc& desc) = 0;
    virtual void          SetVehicleInput(VehicleHandle vehicle, const VehicleInput& input) = 0;
    virtual VehicleState  GetVehicleState(VehicleHandle vehicle) const = 0;
    virtual void          ResetVehicle(VehicleHandle vehicle) = 0;

    virtual const char* Name() const = 0;
};

} // namespace lucida
