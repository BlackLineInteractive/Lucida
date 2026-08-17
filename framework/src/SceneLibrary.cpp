// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/SceneLibrary.h"
#include "lucida/physics/Components.h"

namespace lucida::scenes {
namespace {

constexpr f32 kFloorY = -1.0f;

struct CommonMaterials {
    i32 floor, chrome, glass, red, emitter, water;
};

CommonMaterials AddCommonMaterials(SceneAssets& assets) {
    CommonMaterials m{};
    m.floor   = assets.AddMaterial(Material(CHECKERBOARD, {0.8f, 0.8f, 0.8f}, {0, 0, 0},
                                            0.8, 0.0, 1.0, {0.2f, 0.2f, 0.2f}),
                                   PROC_NONE, "floor");
    m.chrome  = assets.AddMaterial(Material(METAL, {0.9f, 0.9f, 0.95f}, {0, 0, 0}, 0.05, 1.0),
                                   PROC_NONE, "chrome");
    m.glass   = assets.AddMaterial(Material(GLASS, {0.98f, 0.99f, 1.0f}, {0, 0, 0}, 0.0, 0.0, 1.5),
                                   PROC_NONE, "glass");
    m.red     = assets.AddMaterial(Material(DIFFUSE, {0.8f, 0.15f, 0.1f}, {0, 0, 0}, 0.9, 0.0),
                                   PROC_NONE, "red_plastic");
    m.emitter = assets.AddMaterial(Material(EMISSIVE, {0, 0, 0}, {0.3f, 0.5f, 2.0f}, 1.0, 0.0),
                                   PROC_NONE, "blue_emitter");
    m.water   = assets.AddMaterial(Material(WATER, {0.0f, 0.3f, 0.4f}, {0, 0, 0}, 0.0, 0.0, 1.33),
                                   PROC_NONE, "water");
    return m;
}

void AddPrimitives(Registry& registry, const CommonMaterials& m) {
    CreatePrimitive(registry, PrimitiveType::Sphere, {-2.0f, 0.0f, -5.0f}, m.chrome, "Chrome ball");

    const Entity glass = CreatePrimitive(registry, PrimitiveType::Sphere,
                                         {0.0f, 0.2f, -4.5f}, m.glass, "Glass ball");
    registry.Get<LocalTransform>(glass)->scale = Vec3(1.2f);

    const Entity emitter = CreatePrimitive(registry, PrimitiveType::Sphere,
                                           {1.5f, 0.5f, -3.5f}, m.emitter, "Emitter");
    registry.Get<LocalTransform>(emitter)->scale = Vec3(0.3f);

    CreatePrimitive(registry, PrimitiveType::Box, {1.5f, -0.5f, -6.0f}, m.red, "Red box");

    CreateLight(registry, {-5.0f, 8.0f, -2.0f}, {1.0f, 0.95f, 0.9f}, 50.0f, 2.0f, "Key light");
    // Sits inside the emissive sphere so the glow casts light as well as showing.
    CreateLight(registry, {1.5f, 0.5f, -3.5f}, {0.3f, 0.5f, 1.0f}, 15.0f, 0.2f, "Emitter glow");
}

Entity AddFloor(Registry& registry, i32 material, f32 height = kFloorY) {
    const Entity floor = CreatePrimitive(registry, PrimitiveType::Plane,
                                         {0.0f, height, 0.0f}, material, "Ground");
    return floor;
}

CameraState EyeLevelSpawn(const Vec3& position, f32 pitch = 0.0f) {
    CameraState camera;
    camera.position = position;
    camera.yaw      = -kHalfPi;
    camera.pitch    = pitch;
    return camera;
}

} // namespace

SceneAssets Empty(Registry& registry) {
    SceneAssets assets;
    assets.name  = "empty";
    assets.model = ShadingModel::WhittedGI;

    // No geometry at all. The grid and the sky gradient are drawn by the
    // renderer, not by objects, so an empty scene still looks like somewhere to
    // stand rather than a void.
    assets.environment.fog_enabled = false;
    assets.environment.grid_enabled = true;
    assets.spawn = EyeLevelSpawn({0.0f, 1.6f, 6.0f}, -0.12f);

    // One default material, so the first primitive the user creates has
    // something to wear.
    assets.AddMaterial(Material(DIFFUSE, {0.75f, 0.75f, 0.78f}, {0, 0, 0}, 0.45, 0.0),
                       PROC_NONE, "default");
    (void)registry;
    return assets;
}

SceneAssets BasicPrimitives(Registry& registry) {
    SceneAssets assets;
    assets.name  = "basic primitives";
    assets.model = ShadingModel::Whitted;

    const CommonMaterials m = AddCommonMaterials(assets);
    AddFloor(registry, m.floor);
    AddPrimitives(registry, m);

    assets.environment.fog_enabled = false;
    assets.spawn = EyeLevelSpawn({0.0f, 0.0f, 2.0f});
    return assets;
}

SceneAssets WaterAndFog(Registry& registry) {
    SceneAssets assets;
    assets.name  = "water and fog";
    assets.model = ShadingModel::WhittedGI;

    const CommonMaterials m = AddCommonMaterials(assets);
    AddFloor(registry, m.floor);
    // Water sits just above the floor: the checker reads through it, which is
    // what makes the refraction legible.
    AddFloor(registry, m.water, -0.85f);
    AddPrimitives(registry, m);

    assets.environment.fog_enabled = true;
    assets.spawn = EyeLevelSpawn({0.0f, 0.0f, 2.0f});
    return assets;
}

SceneAssets MaterialLab(Registry& registry) {
    SceneAssets assets;
    assets.name  = "material lab";
    assets.model = ShadingModel::WhittedGI;

    struct Stand { MaterialType type; Vec3 albedo; double rough, metal, ri; i32 proc;
                   const char* name; };
    static constexpr Stand kStands[] = {
        { METAL,   {0.95f, 0.96f, 0.98f}, 0.02, 1.0, 1.50, PROC_NONE,       "polished_chrome" },
        { METAL,   {0.95f, 0.96f, 0.98f}, 0.25, 1.0, 1.50, PROC_BRUSHED,    "brushed_steel" },
        { METAL,   {1.00f, 0.77f, 0.34f}, 0.10, 1.0, 1.50, PROC_NONE,       "gold" },
        { METAL,   {0.95f, 0.64f, 0.54f}, 0.20, 1.0, 1.50, PROC_PATINA,     "copper_patina" },
        { METAL,   {0.56f, 0.57f, 0.58f}, 0.30, 1.0, 1.50, PROC_RUST,       "rusted_iron" },
        { METAL,   {0.94f, 0.78f, 0.38f}, 0.50, 1.0, 1.50, PROC_ROUGH_RAMP, "roughness_sweep" },
        { METAL,   {0.75f, 0.62f, 0.18f}, 0.30, 1.0, 1.50, PROC_HEX,        "hex_inlay" },
        { GLASS,   {0.98f, 0.99f, 1.00f}, 0.00, 0.0, 1.52, PROC_NONE,       "clear_glass" },
        { GLASS,   {0.85f, 0.93f, 0.98f}, 0.35, 0.0, 1.33, PROC_NONE,       "frosted_glass" },
        { WATER,   {0.00f, 0.30f, 0.40f}, 0.00, 0.0, 1.33, PROC_NONE,       "water" },
        { DIFFUSE, {0.86f, 0.85f, 0.82f}, 0.10, 0.0, 1.50, PROC_MARBLE,     "marble" },
        { DIFFUSE, {0.45f, 0.26f, 0.12f}, 0.45, 0.0, 1.50, PROC_WOOD,       "wood" },
        { DIFFUSE, {0.18f, 0.45f, 0.55f}, 0.30, 0.0, 1.50, PROC_TILES,      "glazed_tiles" },
        { DIFFUSE, {0.52f, 0.51f, 0.49f}, 0.85, 0.0, 1.50, PROC_CONCRETE,   "concrete" },
        { DIFFUSE, {0.80f, 0.12f, 0.10f}, 0.12, 0.0, 1.50, PROC_NONE,       "red_plastic" },
        { DIFFUSE, {0.05f, 0.05f, 0.06f}, 0.95, 0.0, 1.50, PROC_NONE,       "matte_rubber" },
        { EMISSIVE,{0.00f, 0.00f, 0.00f}, 1.00, 0.0, 1.50, PROC_NONE,       "emitter" },
        { CHECKERBOARD, {0.9f, 0.9f, 0.9f}, 0.35, 0.0, 1.50, PROC_NONE,     "reference_checker" },
    };
    constexpr i32 kCount = i32(sizeof(kStands) / sizeof(kStands[0]));

    const i32 floor_mat  = assets.AddMaterial(Material(CHECKERBOARD, {0.8f, 0.8f, 0.8f},
                                                       {0, 0, 0}, 0.8, 0.0, 1.0,
                                                       {0.2f, 0.2f, 0.2f}), PROC_NONE, "floor");
    const i32 plinth_mat = assets.AddMaterial(Material(DIFFUSE, {0.30f, 0.30f, 0.32f}, {0, 0, 0},
                                                       0.7, 0.0), PROC_NONE, "plinth");
    AddFloor(registry, floor_mat);

    constexpr f32 kSpacing = 2.4f, kRadius = 0.75f, kPlinthHalfHeight = 0.55f;
    const f32 x0 = -0.5f * (kCount - 1) * kSpacing;

    for (i32 i = 0; i < kCount; ++i) {
        const Stand& stand = kStands[i];
        const Vec3 emission = (stand.type == EMISSIVE) ? Vec3(1.6f, 1.35f, 0.9f) : Vec3(0.0f);
        const i32 mat = assets.AddMaterial(
            Material(stand.type, stand.albedo, emission, stand.rough, stand.metal, stand.ri),
            stand.proc, stand.name);

        const f32 x = x0 + f32(i) * kSpacing;
        const f32 top = kFloorY + 2.0f * kPlinthHalfHeight;

        const Entity plinth = CreatePrimitive(registry, PrimitiveType::Box,
                                              {x, kFloorY + kPlinthHalfHeight, -6.0f},
                                              plinth_mat, "Plinth");
        registry.Get<PrimitiveShape>(plinth)->size = Vec3(0.85f, kPlinthHalfHeight, 0.85f);
        registry.Add<LocalBounds>(plinth, LocalBounds{Vec3(-0.85f, -kPlinthHalfHeight, -0.85f),
                                                      Vec3( 0.85f,  kPlinthHalfHeight,  0.85f)});

        const Entity ball = CreatePrimitive(registry, PrimitiveType::Sphere,
                                            {x, top + kRadius, -6.0f}, mat, stand.name);
        registry.Get<LocalTransform>(ball)->scale = Vec3(kRadius);
    }

    CreateLight(registry, {-6.0f, 9.0f, 1.0f}, {1.00f, 0.96f, 0.90f}, 90.0f, 1.5f, "Key light");
    CreateLight(registry, { 7.0f, 5.0f, 1.0f}, {0.65f, 0.78f, 1.00f}, 45.0f, 2.5f, "Fill light");

    assets.spawn = EyeLevelSpawn({0.0f, 2.2f, 15.0f}, -0.05f);
    return assets;
}

SceneAssets RadianceCascades3D(Registry& registry) {
    SceneAssets assets;
    assets.name  = "radiance cascades 3d";
    assets.model = ShadingModel::WhittedGI;

    // Cornell box materials matching private/Shaders/radiance_cascades_3d
    const i32 white_wall  = assets.AddMaterial(Material(DIFFUSE, {0.90f, 0.90f, 0.90f}, {0, 0, 0}, 0.65, 0.0), PROC_NONE, "cornell_white");
    const i32 red_wall    = assets.AddMaterial(Material(DIFFUSE, {0.90f, 0.12f, 0.10f}, {0, 0, 0}, 0.70, 0.0), PROC_NONE, "cornell_red");
    const i32 green_wall  = assets.AddMaterial(Material(DIFFUSE, {0.08f, 0.92f, 0.12f}, {0, 0, 0}, 0.70, 0.0), PROC_NONE, "cornell_green");
    const i32 floor_mat   = assets.AddMaterial(Material(DIFFUSE, {0.88f, 0.88f, 0.88f}, {0, 0, 0}, 0.60, 0.0), PROC_NONE, "cornell_floor");
    const i32 ceiling_mat = assets.AddMaterial(Material(DIFFUSE, {0.92f, 0.92f, 0.92f}, {0, 0, 0}, 0.65, 0.0), PROC_NONE, "cornell_ceiling");
    const i32 mirror_mat  = assets.AddMaterial(Material(METAL,   {0.98f, 0.98f, 0.98f}, {0, 0, 0}, 0.01, 1.0), PROC_NONE, "mirror_chrome");
    const i32 glass_mat   = assets.AddMaterial(Material(GLASS,   {0.99f, 0.99f, 1.00f}, {0, 0, 0}, 0.00, 0.0, 1.52), PROC_NONE, "optical_glass");
    const i32 sun_emitter = assets.AddMaterial(Material(EMISSIVE, {0, 0, 0}, {2.8f, 2.4f, 1.7f}, 1.0, 0.0), PROC_NONE, "sun_aperture");

    // Room boundaries (Floor, Ceiling, Left Red Wall, Right Green Wall, Back White Wall)
    AddFloor(registry, floor_mat, -1.0f);

    // Ceiling
    Entity ceiling = CreatePrimitive(registry, PrimitiveType::Box, {0.0f, 3.2f, -3.0f}, ceiling_mat, "Ceiling");
    registry.Get<PrimitiveShape>(ceiling)->size = Vec3(3.2f, 0.1f, 3.2f);
    registry.Add<LocalBounds>(ceiling, LocalBounds{Vec3(-3.2f, -0.1f, -3.2f), Vec3(3.2f, 0.1f, 3.2f)});

    // Left Red Wall
    Entity left_wall = CreatePrimitive(registry, PrimitiveType::Box, {-3.0f, 1.1f, -3.0f}, red_wall, "Left Red Wall");
    registry.Get<PrimitiveShape>(left_wall)->size = Vec3(0.1f, 2.1f, 3.2f);
    registry.Add<LocalBounds>(left_wall, LocalBounds{Vec3(-0.1f, -2.1f, -3.2f), Vec3(0.1f, 2.1f, 3.2f)});

    // Right Green Wall
    Entity right_wall = CreatePrimitive(registry, PrimitiveType::Box, {3.0f, 1.1f, -3.0f}, green_wall, "Right Green Wall");
    registry.Get<PrimitiveShape>(right_wall)->size = Vec3(0.1f, 2.1f, 3.2f);
    registry.Add<LocalBounds>(right_wall, LocalBounds{Vec3(-0.1f, -2.1f, -3.2f), Vec3(0.1f, 2.1f, 3.2f)});

    // Back White Wall
    Entity back_wall = CreatePrimitive(registry, PrimitiveType::Box, {0.0f, 1.1f, -6.0f}, white_wall, "Back White Wall");
    registry.Get<PrimitiveShape>(back_wall)->size = Vec3(3.2f, 2.1f, 0.1f);
    registry.Add<LocalBounds>(back_wall, LocalBounds{Vec3(-3.2f, -2.1f, -0.1f), Vec3(3.2f, 2.1f, 0.1f)});

    // Archway Columns
    Entity pillar_left = CreatePrimitive(registry, PrimitiveType::Cylinder, {-1.2f, 0.2f, -3.5f}, white_wall, "Archway Pillar Left");
    registry.Get<PrimitiveShape>(pillar_left)->size = Vec3(0.35f, 1.2f, 0.35f);
    Entity pillar_right = CreatePrimitive(registry, PrimitiveType::Cylinder, {1.2f, 0.2f, -3.5f}, white_wall, "Archway Pillar Right");
    registry.Get<PrimitiveShape>(pillar_right)->size = Vec3(0.35f, 1.2f, 0.35f);

    // Inner Mirror Chrome Sphere
    Entity mirror_sphere = CreatePrimitive(registry, PrimitiveType::Sphere, {-1.4f, -0.3f, -4.5f}, mirror_mat, "Mirror Sphere");
    registry.Get<LocalTransform>(mirror_sphere)->scale = Vec3(0.7f);

    // Inner Mirror Chrome Box (Rotated)
    Entity mirror_box = CreatePrimitive(registry, PrimitiveType::Box, {1.4f, -0.35f, -4.5f}, mirror_mat, "Mirror Box");
    registry.Get<PrimitiveShape>(mirror_box)->size = Vec3(0.65f, 0.65f, 0.65f);
    registry.Get<LocalTransform>(mirror_box)->rotation = glm::angleAxis(glm::radians(28.0f), Vec3(0, 1, 0));
    registry.Add<LocalBounds>(mirror_box, LocalBounds{Vec3(-0.65f), Vec3(0.65f)});

    // Central Glass Sphere
    Entity glass_sphere = CreatePrimitive(registry, PrimitiveType::Sphere, {0.0f, -0.15f, -2.4f}, glass_mat, "Glass Sphere");
    registry.Get<LocalTransform>(glass_sphere)->scale = Vec3(0.85f);

    // Radiance Sun Aperture Light
    Entity sun_aperture = CreatePrimitive(registry, PrimitiveType::Sphere, {0.0f, 3.05f, -3.5f}, sun_emitter, "Sun Aperture");
    registry.Get<LocalTransform>(sun_aperture)->scale = Vec3(0.35f);

    CreateLight(registry, {0.0f, 2.9f, -3.5f}, {1.00f, 0.92f, 0.70f}, 75.0f, 1.2f, "Sunlight Radiance");
    CreateLight(registry, {-1.5f, 2.0f, -2.0f}, {0.70f, 0.82f, 1.00f}, 35.0f, 2.0f, "Sky Ambient Fill");

    assets.environment.fog_enabled = false;
    assets.environment.grid_enabled = false;
    assets.spawn = EyeLevelSpawn({0.0f, 1.1f, 3.2f}, -0.06f);
    return assets;
}

SceneAssets PhysicsPlayground(Registry& registry) {
    SceneAssets assets;
    assets.name  = "physics playground";
    assets.model = ShadingModel::WhittedGI;

    const CommonMaterials m = AddCommonMaterials(assets);
    AddFloor(registry, m.floor);

    const i32 domino_mat = assets.AddMaterial(Material(DIFFUSE, {0.15f, 0.45f, 0.85f}, {0, 0, 0}, 0.5, 0.0), PROC_NONE, "domino_blue");
    const i32 heavy_red  = assets.AddMaterial(Material(DIFFUSE, {0.85f, 0.15f, 0.15f}, {0, 0, 0}, 0.4, 0.0), PROC_NONE, "heavy_red");

    for (int i = 0; i < 8; ++i) {
        float z = -2.0f - float(i) * 0.9f;
        Entity domino = CreatePrimitive(registry, PrimitiveType::Box, {0.0f, -0.3f, z}, domino_mat, "Domino " + std::to_string(i + 1));
        registry.Get<PrimitiveShape>(domino)->size = Vec3(0.4f, 0.7f, 0.15f);
        registry.Add<LocalBounds>(domino, LocalBounds{Vec3(-0.4f, -0.7f, -0.15f), Vec3(0.4f, 0.7f, 0.15f)});
        RigidBody rb{};
        rb.type = BodyType::Dynamic;
        rb.shape = ShapeType::Box;
        rb.mass = 5.0f;
        rb.friction = 0.4f;
        rb.restitution = 0.2f;
        registry.Add<RigidBody>(domino, rb);
    }

    Entity ball = CreatePrimitive(registry, PrimitiveType::Sphere, {0.0f, 0.5f, -0.5f}, heavy_red, "Bowling Ball");
    registry.Get<LocalTransform>(ball)->scale = Vec3(0.6f);
    RigidBody ball_rb{};
    ball_rb.type = BodyType::Dynamic;
    ball_rb.shape = ShapeType::Sphere;
    ball_rb.mass = 25.0f;
    ball_rb.friction = 0.3f;
    ball_rb.restitution = 0.6f;
    registry.Add<RigidBody>(ball, ball_rb);

    CreateLight(registry, {-5.0f, 8.0f, 2.0f}, {1.0f, 0.98f, 0.92f}, 80.0f, 1.5f, "Key Light");
    CreateLight(registry, { 5.0f, 6.0f, 2.0f}, {0.6f, 0.75f, 1.00f}, 40.0f, 2.0f, "Fill Light");

    assets.environment.fog_enabled = false;
    assets.environment.grid_enabled = true;
    assets.spawn = EyeLevelSpawn({2.8f, 1.6f, 2.5f}, -0.2f);
    return assets;
}

SceneAssets Build(BuiltIn which, Registry& registry) {
    switch (which) {
    case BuiltIn::Empty:              return Empty(registry);
    case BuiltIn::RadianceCascades3D: return RadianceCascades3D(registry);
    case BuiltIn::BasicPrimitives:    return BasicPrimitives(registry);
    case BuiltIn::MaterialLab:        return MaterialLab(registry);
    case BuiltIn::PhysicsPlayground:  return PhysicsPlayground(registry);
    case BuiltIn::WaterAndFog:
    default:                          return WaterAndFog(registry);
    }
}

const char* Name(BuiltIn which) {
    switch (which) {
    case BuiltIn::Empty:              return "Empty Scene";
    case BuiltIn::RadianceCascades3D: return "Radiance Cascades 3D (GI)";
    case BuiltIn::BasicPrimitives:    return "Whitted RT Studio";
    case BuiltIn::MaterialLab:        return "Material Lab (PBR)";
    case BuiltIn::PhysicsPlayground:  return "Physics Sandbox";
    case BuiltIn::WaterAndFog:        return "Water & Volumetric Fog";
    default:                          return "?";
    }
}

} // namespace lucida::scenes
