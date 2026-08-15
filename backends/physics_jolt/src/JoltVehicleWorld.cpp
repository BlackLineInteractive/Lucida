// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
// =========================================================== PhysicsWorld.cpp
// Jolt Physics vehicle simulation for the 1969 Ford Mustang Boss 302.
//
// Mustang Boss 302 real-world specs used for tuning:
//   Mass              : 1 470 kg
//   Wheelbase         : 2.82 m
//   Track front/rear  : 1.47 / 1.49 m
//   CoM height        : 0.45 m  (estimated, stock suspension)
//   Engine peak torque: 420 N·m @ 3 400 rpm
//   Engine peak power : 217 kW  @ 5 200 rpm
//   Redline           : 6 000 rpm
//   Drivetrain        : RWD, 4-speed manual (simulated as auto)
//   Rear axle ratio   : 3.50 (Boss 302 close-ratio)
//   Tyre size         : P215/65R15 → radius 0.330 m
//   Suspension front  : MacPherson strut, natural freq ~1.6 Hz
//   Suspension rear   : Live axle (Hotchkiss), natural freq ~1.2 Hz

#include "JoltVehicleWorld.h"

// ── Jolt core ──────────────────────────────────────────────────────────────
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
// ── Vehicle ────────────────────────────────────────────────────────────────
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>

JPH_SUPPRESS_WARNINGS

// ── Object layers (must match BroadPhase layer map) ─────────────────────────
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING     = 1;
    static constexpr JPH::uint        NUM_LAYERS = 2;
}

// ── Simple object–layer pair filter ─────────────────────────────────────────
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer l1, JPH::ObjectLayer l2) const override {
        switch (l1) {
            case Layers::NON_MOVING: return l2 == Layers::MOVING;
            case Layers::MOVING:     return true;
            default: return false;
        }
    }
};

// ── Broad phase layers ───────────────────────────────────────────────────────
namespace BPLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint            NUM_LAYERS = 2;
}

class BPLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        m_object_to_bp[Layers::NON_MOVING] = BPLayers::NON_MOVING;
        m_object_to_bp[Layers::MOVING]     = BPLayers::MOVING;
    }
    JPH::uint GetNumBroadPhaseLayers() const override { return BPLayers::NUM_LAYERS; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override {
        return m_object_to_bp[l];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer l) const override {
        return l == BPLayers::NON_MOVING ? "NON_MOVING" : "MOVING";
    }
#endif
private:
    JPH::BroadPhaseLayer m_object_to_bp[Layers::NUM_LAYERS];
};

class OBPLayerPairFilter : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer l, JPH::BroadPhaseLayer bp) const override {
        switch (l) {
            case Layers::NON_MOVING: return bp == BPLayers::MOVING;
            case Layers::MOVING:     return true;
            default: return false;
        }
    }
};

// ── PhysicsWorld::Impl ───────────────────────────────────────────────────────
struct PhysicsWorld::Impl {
    // Jolt infrastructure
    JPH::TempAllocatorImpl*    temp_allocator   = nullptr;
    JPH::JobSystemThreadPool*  job_system       = nullptr;
    BPLayerInterfaceImpl*      bp_layer_iface   = nullptr;
    OBPLayerPairFilter*        ob_bp_filter     = nullptr;
    ObjectLayerPairFilterImpl* obj_layer_filter = nullptr;
    JPH::PhysicsSystem*        physics_system   = nullptr;

    // Bodies
    JPH::BodyID car_body_id;
    JPH::BodyID ground_id;

    // Vehicle
    JPH::Ref<JPH::VehicleConstraint> vehicle;
    JPH::WheeledVehicleController*   controller = nullptr; // owned by vehicle

    // Spawn
    JPH::Vec3 spawn_pos = JPH::Vec3(0.0f, 0.7f, 0.0f);
    JPH::Quat spawn_rot = JPH::Quat::sIdentity();

    // Accumulate wheel spin
    float wheel_spin[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// ── helpers ──────────────────────────────────────────────────────────────────
static glm::vec3 ToGLM(const JPH::Vec3& v)  { return {v.GetX(), v.GetY(), v.GetZ()}; }
static glm::quat ToGLM(const JPH::Quat& q)  { return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()}; }

// ─────────────────────────────────────────────────── PhysicsWorld::Init ──────
void PhysicsWorld::Init() {
    if (is_initialised) return;

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    auto* impl = new Impl();
    m_impl = impl;

    impl->temp_allocator   = new JPH::TempAllocatorImpl(32 * 1024 * 1024); // 32 MB
    impl->job_system       = new JPH::JobSystemThreadPool(
                                 JPH::cMaxPhysicsJobs,
                                 JPH::cMaxPhysicsBarriers, 2);
    impl->bp_layer_iface   = new BPLayerInterfaceImpl();
    impl->ob_bp_filter     = new OBPLayerPairFilter();
    impl->obj_layer_filter = new ObjectLayerPairFilterImpl();

    impl->physics_system   = new JPH::PhysicsSystem();
    impl->physics_system->Init(
        1024,     // max bodies
        0,        // num body mutexes (0 = auto)
        1024,     // max body pairs
        1024,     // max contact constraints
        *impl->bp_layer_iface,
        *impl->ob_bp_filter,
        *impl->obj_layer_filter);

    impl->physics_system->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    auto& body_iface = impl->physics_system->GetBodyInterface();

    // ── Ground plane ────────────────────────────────────────────────────────
    // Half-space at y=0 — an enormous flat box works fine for a flat world.
    JPH::BoxShapeSettings ground_shape(JPH::Vec3(500.0f, 0.5f, 500.0f));
    ground_shape.SetEmbedded();
    JPH::BodyCreationSettings gs(
        ground_shape.Create().Get(),
        JPH::RVec3(0.0f, -0.5f, 0.0f),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);
    gs.mFriction = 0.85f;
    impl->ground_id = body_iface.CreateAndAddBody(gs, JPH::EActivation::DontActivate);

    // ── Car body ─────────────────────────────────────────────────────────────
    // Mustang chassis bounding box: 1.77 m × 0.45 m × 4.20 m
    // We use a box that approximates the body, with CoM offset downward
    // to lower the centre of gravity (0.45 m above ground).
    float car_w  = 0.88f;   // half-width  (~1.77 m total)
    float car_h  = 0.22f;   // half-height (~0.45 m, low CG muscle car)
    float car_l  = 2.10f;   // half-length (~4.20 m total)

    JPH::BoxShapeSettings car_shape_s(JPH::Vec3(car_w, car_h, car_l));
    car_shape_s.SetEmbedded();
    auto car_shape_result = car_shape_s.Create();

    // Offset CoM so it sits at 0.45 m above ground when the body
    // reference point is at 0.68 m (axle-height).
    JPH::OffsetCenterOfMassShapeSettings com_shape(
        JPH::Vec3(0.0f, -0.10f, 0.0f),   // pull CoM down
        car_shape_result.Get());
    com_shape.SetEmbedded();

    JPH::BodyCreationSettings car_bs(
        com_shape.Create().Get(),
        JPH::RVec3(impl->spawn_pos),
        impl->spawn_rot,
        JPH::EMotionType::Dynamic,
        Layers::MOVING);
    car_bs.mOverrideMassProperties     = JPH::EOverrideMassProperties::CalculateInertia;
    car_bs.mMassPropertiesOverride.mMass = 1470.0f;  // kg
    car_bs.mFriction                   = 0.5f;
    car_bs.mLinearDamping              = 0.12f;
    car_bs.mAngularDamping             = 0.8f;

    // Create then add separately: VehicleConstraint needs the Body reference,
    // and CreateAndAddBody only hands back an ID.
    JPH::Body *car_body = body_iface.CreateBody(car_bs);
    impl->car_body_id = car_body->GetID();
    body_iface.AddBody(impl->car_body_id, JPH::EActivation::Activate);

    // ── VehicleConstraint ───────────────────────────────────────────────────
    JPH::VehicleConstraintSettings vehicle_settings;

    // Wheel positions relative to car body centre (Y = axle height offset)
    // The body box centre is at 0.68 m, axles at 0.33 m → offset -0.35 m
    // XZ: front axle at +1.35 m, rear at -1.47 m (wheelbase 2.82 m)
    // Track: ±0.735 m front, ±0.745 m rear
    const float y_axle  = -0.35f;  // below box centre
    const float z_front = +1.35f;
    const float z_rear  = -1.47f;
    const float x_fl    = -0.735f;
    const float x_fr    = +0.735f;
    const float x_rl    = -0.745f;
    const float x_rr    = +0.745f;

    auto make_wheel = [&](float x, float y, float z, bool front) {
        JPH::WheelSettingsWV* w = new JPH::WheelSettingsWV();
        w->mPosition             = JPH::Vec3(x, y, z);
        w->mRadius               = 0.330f;    // P215/65R15
        w->mWidth                = 0.215f;
        w->mSuspensionMinLength  = 0.10f;
        w->mSuspensionMaxLength  = front ? 0.30f : 0.28f;
        w->mSuspensionSpring.mMode      = JPH::ESpringMode::FrequencyAndDamping;
        w->mSuspensionSpring.mFrequency = front ? 1.6f : 1.2f;  // Hz (stiffer front)
        w->mSuspensionSpring.mDamping   = 0.5f;
        // Longitudinal friction (Pacejka-inspired, Jolt uses a simple slip curve)
        w->mMaxSteerAngle        = front ? JPH::DegreesToRadians(32.0f) : 0.0f;
        // Lateral friction (cornering) — wide muscle car tyre, decent grip
        return w;
    };

    JPH::Ref<JPH::WheelSettingsWV> wFL(make_wheel(x_fl, y_axle, z_front, true));
    JPH::Ref<JPH::WheelSettingsWV> wFR(make_wheel(x_fr, y_axle, z_front, true));
    JPH::Ref<JPH::WheelSettingsWV> wRL(make_wheel(x_rl, y_axle, z_rear,  false));
    JPH::Ref<JPH::WheelSettingsWV> wRR(make_wheel(x_rr, y_axle, z_rear,  false));

    vehicle_settings.mWheels.clear();
    for (JPH::WheelSettings *w : { (JPH::WheelSettings *)wFL, (JPH::WheelSettings *)wFR,
                                   (JPH::WheelSettings *)wRL, (JPH::WheelSettings *)wRR })
        vehicle_settings.mWheels.push_back(w);

    // ── Anti-roll bars ──────────────────────────────────────────────────────
    JPH::VehicleAntiRollBar front_arb, rear_arb;
    front_arb.mLeftWheel  = 0; front_arb.mRightWheel = 1; front_arb.mStiffness = 1200.0f;
    rear_arb.mLeftWheel   = 2; rear_arb.mRightWheel  = 3; rear_arb.mStiffness  =  800.0f;
    vehicle_settings.mAntiRollBars = { front_arb, rear_arb };

    // ── WheeledVehicleController ────────────────────────────────────────────
    auto* ctrl = new JPH::WheeledVehicleControllerSettings();

    // Engine — 302 cu.in. (4.9 L) V8, 290 hp / 290 ft·lb
    ctrl->mEngine.mMaxTorque          = 420.0f;   // N·m peak
    ctrl->mEngine.mMinRPM             = 700.0f;
    ctrl->mEngine.mMaxRPM             = 6200.0f;
    // Normalised torque curve (0=idle, 1=redline) — typical V8 broad band
    ctrl->mEngine.mNormalizedTorque.Reserve(6);
    ctrl->mEngine.mNormalizedTorque.AddPoint(0.00f, 0.60f);  // idle torque
    ctrl->mEngine.mNormalizedTorque.AddPoint(0.20f, 0.85f);  // building
    ctrl->mEngine.mNormalizedTorque.AddPoint(0.45f, 1.00f);  // 3 400 rpm peak
    ctrl->mEngine.mNormalizedTorque.AddPoint(0.65f, 0.95f);  // tapering
    ctrl->mEngine.mNormalizedTorque.AddPoint(0.85f, 0.80f);  // near redline
    ctrl->mEngine.mNormalizedTorque.AddPoint(1.00f, 0.55f);  // redline

    // Transmission — Boss 302 Toploader 4-speed (simulate as auto with same ratios)
    ctrl->mTransmission.mMode = JPH::ETransmissionMode::Auto;
    ctrl->mTransmission.mGearRatios        = { 3.50f, 2.14f, 1.36f, 1.00f };
    ctrl->mTransmission.mReverseGearRatios = { -3.00f };
    ctrl->mTransmission.mSwitchTime        = 0.25f;   // seconds between shifts
    ctrl->mTransmission.mClutchStrength    = 6.0f;
    ctrl->mTransmission.mShiftUpRPM        = 5400.0f;
    ctrl->mTransmission.mShiftDownRPM      = 2200.0f;

    // Differential — RWD, Traction-Lok limited slip (Boss 302 standard)
    JPH::VehicleDifferentialSettings diff;
    diff.mLeftWheel          = 2;     // RL
    diff.mRightWheel         = 3;     // RR
    diff.mDifferentialRatio  = 3.50f; // Boss 302 rear axle ratio
    diff.mLeftRightSplit      = 0.5f; // 50:50 base, LSD
    diff.mLimitedSlipRatio    = 1.4f; // LSD locks at 1.4× speed difference
    diff.mEngineTorqueRatio   = 1.0f; // 100% engine torque to rear
    ctrl->mDifferentials = { diff };

    // Tyre friction model — Pacejka-like lateral slip
    // Front wheels: slightly less grip (understeer tendency = more stable)
    // Rear wheels: slightly more grip (but RWD will oversteer under hard throttle)
    for (auto* ws : { wFL.GetPtr(), wFR.GetPtr() }) {
        ws->mLateralFriction.Reserve(3);
        ws->mLateralFriction.AddPoint(0.0f, 0.0f);
        ws->mLateralFriction.AddPoint(3.0f, 0.78f);   // peak lateral at 3 deg slip
        ws->mLateralFriction.AddPoint(20.0f, 0.65f);
        ws->mLongitudinalFriction.Reserve(3);
        ws->mLongitudinalFriction.AddPoint(0.0f, 0.0f);
        ws->mLongitudinalFriction.AddPoint(0.08f, 0.82f);
        ws->mLongitudinalFriction.AddPoint(0.40f, 0.70f);
    }
    for (auto* ws : { wRL.GetPtr(), wRR.GetPtr() }) {
        ws->mLateralFriction.Reserve(3);
        ws->mLateralFriction.AddPoint(0.0f, 0.0f);
        ws->mLateralFriction.AddPoint(3.0f, 0.85f);
        ws->mLateralFriction.AddPoint(20.0f, 0.72f);
        ws->mLongitudinalFriction.Reserve(3);
        ws->mLongitudinalFriction.AddPoint(0.0f, 0.0f);
        ws->mLongitudinalFriction.AddPoint(0.10f, 0.90f);
        ws->mLongitudinalFriction.AddPoint(0.50f, 0.75f);
    }

    vehicle_settings.mController = ctrl;

    // ── Create & add constraint ──────────────────────────────────────────────
    impl->vehicle = new JPH::VehicleConstraint(*car_body, vehicle_settings);

    // Collision tester — cast a cylinder down from each wheel
    JPH::Ref<JPH::VehicleCollisionTesterCastCylinder> tester =
        new JPH::VehicleCollisionTesterCastCylinder(Layers::MOVING, 0.05f);
    impl->vehicle->SetVehicleCollisionTester(tester);

    impl->physics_system->AddConstraint(impl->vehicle);
    impl->physics_system->AddStepListener(impl->vehicle);

    // Cache controller pointer (owned by impl->vehicle internally)
    impl->controller = static_cast<JPH::WheeledVehicleController*>(
        impl->vehicle->GetController());

    // Initialise outputs
    Reset();

    is_initialised = true;
    std::cout << "[Physics] Jolt v"
              << JPH_VERSION_MAJOR << "." << JPH_VERSION_MINOR
              << " initialised — Mustang Boss 302 ready.\n";
}

// ──────────────────────────────────────────── PhysicsWorld::Step ─────────────
void PhysicsWorld::Step(float dt, const CarInput& input) {
    if (!is_initialised || !m_impl) return;
    auto* impl = m_impl;

    if (input.reset) { Reset(); return; }

    // Clamp dt to keep simulation stable during lag spikes
    dt = std::min(dt, 1.0f / 20.0f);

    auto& body_iface = impl->physics_system->GetBodyInterface();

    // ── Feed inputs to vehicle controller ───────────────────────────────────
    // Jolt forward direction: +Z (the car was placed facing +Z at spawn)
    // Right: +X.  The Mustang GLB is oriented along -Z, so we negate fwd.
    auto* ctrl = impl->controller;

    float fwd   = input.throttle - input.brake;  // net: +=accelerate, -=brake
    float right = -input.steer;                  // Jolt uses right-handed steer

    ctrl->SetDriverInput(fwd, right,
                         input.brake,
                         input.handbrake ? 1.0f : 0.0f);

    // ── Physics step ────────────────────────────────────────────────────────
    const int kCollisionSteps = 1;
    impl->physics_system->Update(dt, kCollisionSteps,
                                 impl->temp_allocator,
                                 impl->job_system);

    // ── Read car body state ──────────────────────────────────────────────────
    JPH::RVec3 jpos = body_iface.GetCenterOfMassPosition(impl->car_body_id);
    JPH::Quat  jrot = body_iface.GetRotation(impl->car_body_id);
    JPH::Vec3  jvel = body_iface.GetLinearVelocity(impl->car_body_id);

    car_pos   = ToGLM(JPH::Vec3(jpos));
    car_rot   = ToGLM(jrot);
    speed_kmh = JPH::Vec3(jvel).Length() * 3.6f;

    // ── Read engine state ────────────────────────────────────────────────────
    engine_rpm   = ctrl->GetEngine().GetCurrentRPM();
    current_gear = (int)ctrl->GetTransmission().GetCurrentGear();

    // ── Read wheel states ────────────────────────────────────────────────────
    glm::mat4 car_mat = glm::mat4_cast(car_rot);
    car_mat[3] = glm::vec4(car_pos, 1.0f);

    for (int i = 0; i < 4; ++i) {
        const JPH::Wheel* w = impl->vehicle->GetWheel(i);
        const JPH::WheelSettings* ws = w->GetSettings();

        // Wheel centre in world space:
        // Jolt gives us the contact point; add radius back up
        JPH::RMat44 wt = impl->vehicle->GetWheelWorldTransform(
            i, JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY());
        JPH::Vec3 contact = JPH::Vec3(wt.GetTranslation());
        JPH::Quat wrot    = wt.GetQuaternion();

        wheels[i].world_pos = ToGLM(JPH::Vec3(contact));
        wheels[i].world_rot = ToGLM(wrot);
        wheels[i].steer_angle_rad = w->GetSteerAngle();
        impl->wheel_spin[i] += w->GetAngularVelocity() * dt;
        wheels[i].spin_angle_rad  = impl->wheel_spin[i];
    }
}

// ─────────────────────────────────────────── PhysicsWorld::Reset ─────────────
void PhysicsWorld::Reset() {
    if (!m_impl) return;
    auto& body_iface = m_impl->physics_system->GetBodyInterface();
    body_iface.SetPositionAndRotation(
        m_impl->car_body_id,
        JPH::RVec3(m_impl->spawn_pos),
        m_impl->spawn_rot,
        JPH::EActivation::Activate);
    body_iface.SetLinearVelocity(m_impl->car_body_id,  JPH::Vec3::sZero());
    body_iface.SetAngularVelocity(m_impl->car_body_id, JPH::Vec3::sZero());
    for (float& s : m_impl->wheel_spin) s = 0.0f;
    car_pos = glm::vec3(m_impl->spawn_pos.GetX(),
                        m_impl->spawn_pos.GetY(),
                        m_impl->spawn_pos.GetZ());
    car_rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    speed_kmh  = 0.0f;
    engine_rpm = 800.0f;
    current_gear = 0;
}

// ─────────────────────────────────────────── PhysicsWorld::Shutdown ──────────
void PhysicsWorld::Shutdown() {
    if (!is_initialised || !m_impl) return;
    auto* impl = m_impl;

    impl->physics_system->RemoveConstraint(impl->vehicle);
    impl->vehicle = nullptr;

    auto& body_iface = impl->physics_system->GetBodyInterface();
    body_iface.RemoveBody(impl->car_body_id);
    body_iface.DestroyBody(impl->car_body_id);
    body_iface.RemoveBody(impl->ground_id);
    body_iface.DestroyBody(impl->ground_id);

    delete impl->physics_system;
    delete impl->job_system;
    delete impl->temp_allocator;
    delete impl->bp_layer_iface;
    delete impl->ob_bp_filter;
    delete impl->obj_layer_filter;
    delete impl;
    m_impl = nullptr;

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    is_initialised = false;
}
