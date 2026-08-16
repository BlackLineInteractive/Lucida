// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/backend/JoltBackend.h"

#include "JoltVehicleWorld.h"
#include "lucida/core/diag/Log.h"

namespace lucida {
namespace {

// Adapter over the existing Jolt vehicle world. The world currently owns one
// car and a ground plane; CreateVehicle hands out a handle to it rather than
// pretending to support an arbitrary fleet.
class JoltBackend final : public IPhysicsBackend {
public:
    bool Init() override {
        m_world.Init();
        if (!m_world.is_initialised) {
            LUCIDA_ERROR(Physics, "Jolt world failed to start");
            return false;
        }
        LUCIDA_INFO(Physics, "Jolt up");
        return true;
    }

    void Shutdown() override {
        if (m_world.is_initialised) m_world.Shutdown();
    }

    void Step(f32 fixed_dt) override {
        if (!m_world.is_initialised) return;
        m_world.Step(fixed_dt, m_input);
    }

    // --- rigid bodies ------------------------------------------------------
    BodyHandle CreateBody(const BodyDesc& desc) override {
        return m_world.CreateBody(desc);
    }
    void DestroyBody(BodyHandle body) override {
        m_world.DestroyBody(body);
    }
    Transform GetBodyTransform(BodyHandle body) const override {
        return m_world.GetBodyTransform(body);
    }
    void SetBodyTransform(BodyHandle body, const Transform& transform) override {
        m_world.SetBodyTransform(body, transform);
    }

    // --- vehicle ------------------------------------------------------------
    VehicleHandle CreateVehicle(const VehicleDesc&) override {
        if (!m_world.is_initialised) return VehicleHandle{};
        if (m_vehicle_taken) {
            LUCIDA_WARN(Physics, "the Jolt world holds one vehicle; returning the existing one");
        }
        m_vehicle_taken = true;
        return VehicleHandle{0, 1};
    }

    void SetVehicleInput(VehicleHandle vehicle, const VehicleInput& input) override {
        if (!vehicle.IsValid()) return;
        m_input.throttle  = input.throttle;
        m_input.brake     = input.brake;
        m_input.steer     = input.steer;
        m_input.handbrake = input.handbrake;
        m_input.reset     = false;
    }

    VehicleState GetVehicleState(VehicleHandle vehicle) const override {
        VehicleState state;
        if (!vehicle.IsValid()) return state;

        state.position = m_world.car_pos;
        state.rotation = m_world.car_rot;
        for (usize i = 0; i < state.wheels.size(); ++i) {
            state.wheels[i].position    = m_world.wheels[i].world_pos;
            state.wheels[i].rotation    = m_world.wheels[i].world_rot;
            state.wheels[i].steer_angle = m_world.wheels[i].steer_angle_rad;
            state.wheels[i].spin_angle  = m_world.wheels[i].spin_angle_rad;
        }
        state.speed_kmh  = m_world.speed_kmh;
        state.engine_rpm = m_world.engine_rpm;
        state.gear       = m_world.current_gear;
        return state;
    }

    void ResetVehicle(VehicleHandle vehicle) override {
        if (vehicle.IsValid()) m_world.Reset();
    }

    const char* Name() const override { return "Jolt"; }

private:
    PhysicsWorld m_world;
    CarInput     m_input;
    bool         m_vehicle_taken = false;
};

} // namespace

std::unique_ptr<IPhysicsBackend> CreateJoltBackend() {
    return std::make_unique<JoltBackend>();
}

} // namespace lucida
