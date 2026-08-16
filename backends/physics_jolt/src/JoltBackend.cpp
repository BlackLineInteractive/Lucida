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

    void AddImpulse(BodyHandle body, const Vec3& impulse) override {
        m_world.AddImpulse(body, impulse);
    }
    void AddForce(BodyHandle body, const Vec3& force) override {
        m_world.AddForce(body, force);
    }
    void SetLinearVelocity(BodyHandle body, const Vec3& velocity) override {
        m_world.SetLinearVelocity(body, velocity);
    }
    Vec3 GetLinearVelocity(BodyHandle body) const override {
        return m_world.GetLinearVelocity(body);
    }

    bool CastRay(const Vec3& origin, const Vec3& direction, f32 max_distance, RaycastHit& out_hit) const override {
        return m_world.CastRay(origin, direction, max_distance, out_hit);
    }
    void PopCollisionEvents(std::vector<CollisionEvent>& out_events) override {
        m_world.PopCollisionEvents(out_events);
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

    // --- character controller -----------------------------------------------
    // First-iteration implementation: kinematic capsule body + downward raycast
    // for grounded detection. Full JPH::CharacterVirtual integration follows once
    // the PhysicsWorld exposes the JPH::PhysicsSystem reference (M12).
    CharacterHandle CreateCharacter(const CharacterDesc& desc) override {
        if (!m_world.is_initialised) return CharacterHandle{};
        // Find free slot
        for (u32 i = 0; i < kMaxCharacters; ++i) {
            if (!m_chars[i].alive) {
                BodyDesc bd{};
                bd.type         = BodyType::Kinematic;
                bd.shape        = ShapeType::Capsule;
                bd.half_extent  = Vec3(desc.capsule_radius,
                                       desc.capsule_height * 0.5f,
                                       desc.capsule_radius);
                bd.position     = desc.position;
                bd.mass         = desc.mass;
                bd.user_data    = i;

                m_chars[i].body          = m_world.CreateBody(bd);
                m_chars[i].alive         = true;
                m_chars[i].step_height   = desc.step_height;
                m_chars[i].max_slope_deg = desc.max_slope_deg;
                m_chars[i].capsule_half  = desc.capsule_height * 0.5f;
                m_chars[i].is_grounded   = false;
                return CharacterHandle{i, 1};
            }
        }
        LUCIDA_WARN(Physics, "CreateCharacter: all %u slots full", kMaxCharacters);
        return CharacterHandle{};
    }

    void DestroyCharacter(CharacterHandle ch) override {
        if (!ch.IsValid() || ch.index >= kMaxCharacters) return;
        auto& slot = m_chars[ch.index];
        if (!slot.alive) return;
        m_world.DestroyBody(slot.body);
        slot = CharSlot{};
    }

    void MoveCharacter(CharacterHandle ch, const Vec3& velocity, f32 dt) override {
        if (!ch.IsValid() || ch.index >= kMaxCharacters) return;
        auto& slot = m_chars[ch.index];
        if (!slot.alive) return;

        // Translate kinematic body by velocity * dt
        Transform t = m_world.GetBodyTransform(slot.body);
        t.position  += velocity * dt;
        m_world.SetBodyTransform(slot.body, t);

        // Grounded: short raycast downward from capsule centre
        RaycastHit hit{};
        Vec3 origin = t.position;
        bool grounded = m_world.CastRay(origin, Vec3(0.0f, -1.0f, 0.0f),
                                        slot.capsule_half + slot.step_height + 0.05f,
                                        hit);
        slot.is_grounded = grounded && hit.has_hit;
    }

    Vec3 GetCharacterPosition(CharacterHandle ch) const override {
        if (!ch.IsValid() || ch.index >= kMaxCharacters) return Vec3(0.0f);
        const auto& slot = m_chars[ch.index];
        if (!slot.alive) return Vec3(0.0f);
        return m_world.GetBodyTransform(slot.body).position;
    }

    bool IsCharacterGrounded(CharacterHandle ch) const override {
        if (!ch.IsValid() || ch.index >= kMaxCharacters) return false;
        return m_chars[ch.index].is_grounded;
    }

    const char* Name() const override { return "Jolt"; }

private:
    static constexpr u32 kMaxCharacters = 32;

    struct CharSlot {
        BodyHandle body{};
        f32  step_height   = 0.35f;
        f32  max_slope_deg = 45.0f;
        f32  capsule_half  = 0.90f;
        bool is_grounded   = false;
        bool alive         = false;
    };

    PhysicsWorld m_world;
    CarInput     m_input;
    bool         m_vehicle_taken = false;
    std::array<CharSlot, kMaxCharacters> m_chars{};
};

} // namespace

std::unique_ptr<IPhysicsBackend> CreateJoltBackend() {
    return std::make_unique<JoltBackend>();
}

} // namespace lucida
