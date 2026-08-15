#include "lucida/framework/SceneLibrary.h"

namespace lucida::scenes {
namespace {

constexpr f32 kFloorY = -1.0f;

// Materials shared by the primitive scenes, added in a fixed order so the
// indices below stay readable.
struct CommonMaterials {
    i32 floor, chrome, glass, red, emitter, water;
};

CommonMaterials AddCommonMaterials(RenderScene& scene) {
    CommonMaterials m{};
    m.floor   = scene.AddMaterial(Material(CHECKERBOARD, {0.8f, 0.8f, 0.8f}, {0, 0, 0},
                                           0.8, 0.0, 1.0, {0.2f, 0.2f, 0.2f}));
    m.chrome  = scene.AddMaterial(Material(METAL, {0.9f, 0.9f, 0.95f}, {0, 0, 0}, 0.05, 1.0));
    m.glass   = scene.AddMaterial(Material(GLASS, {0.98f, 0.99f, 1.0f}, {0, 0, 0}, 0.0, 0.0, 1.5));
    m.red     = scene.AddMaterial(Material(DIFFUSE, {0.8f, 0.15f, 0.1f}, {0, 0, 0}, 0.9, 0.0));
    m.emitter = scene.AddMaterial(Material(EMISSIVE, {0, 0, 0}, {0.3f, 0.5f, 2.0f}, 1.0, 0.0));
    m.water   = scene.AddMaterial(Material(WATER, {0.0f, 0.3f, 0.4f}, {0, 0, 0}, 0.0, 0.0, 1.33));
    return m;
}

void AddPrimitives(RenderScene& scene, const CommonMaterials& m) {
    scene.AddSphere({-2.0f, 0.0f, -5.0f}, 1.0f, m.chrome);
    scene.AddSphere({ 0.0f, 0.2f, -4.5f}, 1.2f, m.glass);
    scene.AddSphere({ 1.5f, 0.5f, -3.5f}, 0.3f, m.emitter);
    scene.AddCube({1.5f, -0.5f, -6.0f}, {0.5f, 0.5f, 0.5f}, m.red);

    scene.AddLight({-5.0f, 8.0f, -2.0f}, 50.0f, {1.0f, 0.95f, 0.9f}, 2.0f);
    // Second light sits inside the emissive sphere, so the glow casts light too.
    scene.AddLight({1.5f, 0.5f, -3.5f}, 15.0f, {0.3f, 0.5f, 1.0f}, 0.2f);
}

CameraState EyeLevelSpawn(const Vec3& position, f32 pitch = 0.0f) {
    CameraState camera;
    camera.position = position;
    camera.yaw      = -kHalfPi;   // looking down -Z
    camera.pitch    = pitch;
    return camera;
}

} // namespace

RenderScene BasicPrimitives() {
    RenderScene scene;
    scene.name  = "basic primitives";
    scene.model = ShadingModel::Whitted;

    const CommonMaterials m = AddCommonMaterials(scene);
    scene.AddPlane({0, 1, 0}, kFloorY, m.floor);
    AddPrimitives(scene, m);

    scene.environment.fog_enabled = false;
    scene.spawn = EyeLevelSpawn({0.0f, 0.0f, 2.0f});
    return scene;
}

RenderScene WaterAndFog() {
    RenderScene scene;
    scene.name  = "water and fog";
    scene.model = ShadingModel::WhittedGI;

    const CommonMaterials m = AddCommonMaterials(scene);
    scene.AddPlane({0, 1, 0}, kFloorY, m.floor);
    // Water sits just above the floor: the checker reads through it, which is
    // what makes the refraction legible.
    scene.AddPlane({0, 1, 0}, -0.85f, m.water);
    AddPrimitives(scene, m);

    scene.environment.fog_enabled = true;
    scene.spawn = EyeLevelSpawn({0.0f, 0.0f, 2.0f});
    return scene;
}

RenderScene MaterialLab() {
    RenderScene scene;
    scene.name  = "material lab";
    scene.model = ShadingModel::WhittedGI;

    struct Stand { MaterialType type; Vec3 albedo; double rough, metal, ri; i32 proc; };
    static constexpr Stand kStands[] = {
        { METAL,   {0.95f, 0.96f, 0.98f}, 0.02, 1.0, 1.50, PROC_NONE },       // polished chrome
        { METAL,   {0.95f, 0.96f, 0.98f}, 0.25, 1.0, 1.50, PROC_BRUSHED },    // brushed steel
        { METAL,   {1.00f, 0.77f, 0.34f}, 0.10, 1.0, 1.50, PROC_NONE },       // gold
        { METAL,   {0.95f, 0.64f, 0.54f}, 0.20, 1.0, 1.50, PROC_PATINA },     // copper, oxidising
        { METAL,   {0.56f, 0.57f, 0.58f}, 0.30, 1.0, 1.50, PROC_RUST },       // iron turning to rust
        { METAL,   {0.94f, 0.78f, 0.38f}, 0.50, 1.0, 1.50, PROC_ROUGH_RAMP }, // roughness sweep
        { METAL,   {0.75f, 0.62f, 0.18f}, 0.30, 1.0, 1.50, PROC_HEX },        // hex-cell inlay
        { GLASS,   {0.98f, 0.99f, 1.00f}, 0.00, 0.0, 1.52, PROC_NONE },       // clear glass
        { GLASS,   {0.85f, 0.93f, 0.98f}, 0.35, 0.0, 1.33, PROC_NONE },       // frosted, low IOR
        { WATER,   {0.00f, 0.30f, 0.40f}, 0.00, 0.0, 1.33, PROC_NONE },       // water
        { DIFFUSE, {0.86f, 0.85f, 0.82f}, 0.10, 0.0, 1.50, PROC_MARBLE },     // polished marble
        { DIFFUSE, {0.45f, 0.26f, 0.12f}, 0.45, 0.0, 1.50, PROC_WOOD },       // wood
        { DIFFUSE, {0.18f, 0.45f, 0.55f}, 0.30, 0.0, 1.50, PROC_TILES },      // glazed tiles
        { DIFFUSE, {0.52f, 0.51f, 0.49f}, 0.85, 0.0, 1.50, PROC_CONCRETE },   // concrete
        { DIFFUSE, {0.80f, 0.12f, 0.10f}, 0.12, 0.0, 1.50, PROC_NONE },       // smooth plastic
        { DIFFUSE, {0.05f, 0.05f, 0.06f}, 0.95, 0.0, 1.50, PROC_NONE },       // matte rubber
        { EMISSIVE,{0.00f, 0.00f, 0.00f}, 1.00, 0.0, 1.50, PROC_NONE },       // emitter
        { CHECKERBOARD, {0.9f, 0.9f, 0.9f}, 0.35, 0.0, 1.50, PROC_NONE },     // reference checker
    };
    constexpr i32 kCount = i32(sizeof(kStands) / sizeof(kStands[0]));

    const i32 floor_mat  = scene.AddMaterial(Material(CHECKERBOARD, {0.8f, 0.8f, 0.8f}, {0, 0, 0},
                                                      0.8, 0.0, 1.0, {0.2f, 0.2f, 0.2f}));
    const i32 plinth_mat = scene.AddMaterial(Material(DIFFUSE, {0.30f, 0.30f, 0.32f}, {0, 0, 0},
                                                      0.7, 0.0));
    scene.AddPlane({0, 1, 0}, kFloorY, floor_mat);

    constexpr f32 kSpacing = 2.4f, kRadius = 0.75f, kPlinthHalfHeight = 0.55f;
    const f32 x0 = -0.5f * (kCount - 1) * kSpacing;

    for (i32 i = 0; i < kCount; ++i) {
        const Stand& stand = kStands[i];
        const Vec3 emission = (stand.type == EMISSIVE) ? Vec3(1.6f, 1.35f, 0.9f) : Vec3(0.0f);
        const i32 mat = scene.AddMaterial(
            Material(stand.type, stand.albedo, emission, stand.rough, stand.metal, stand.ri),
            stand.proc);

        const f32 x = x0 + f32(i) * kSpacing;
        const f32 top = kFloorY + 2.0f * kPlinthHalfHeight;
        scene.AddCube({x, kFloorY + kPlinthHalfHeight, -6.0f},
                      {0.85f, kPlinthHalfHeight, 0.85f}, plinth_mat);
        scene.AddSphere({x, top + kRadius, -6.0f}, kRadius, mat);
    }

    scene.AddLight({-6.0f, 9.0f, 1.0f}, 90.0f, {1.00f, 0.96f, 0.90f}, 1.5f);
    scene.AddLight({ 7.0f, 5.0f, 1.0f}, 45.0f, {0.65f, 0.78f, 1.00f}, 2.5f);

    // Far enough back that the whole row fits a 60-degree frame.
    scene.spawn = EyeLevelSpawn({0.0f, 2.2f, 15.0f}, -0.05f);
    return scene;
}

RenderScene Build(BuiltIn which) {
    switch (which) {
    case BuiltIn::BasicPrimitives: return BasicPrimitives();
    case BuiltIn::MaterialLab:     return MaterialLab();
    case BuiltIn::WaterAndFog:
    default:                       return WaterAndFog();
    }
}

const char* Name(BuiltIn which) {
    switch (which) {
    case BuiltIn::BasicPrimitives: return "Basic primitives";
    case BuiltIn::MaterialLab:     return "Material lab";
    case BuiltIn::WaterAndFog:     return "Water and fog";
    default:                       return "?";
    }
}

} // namespace lucida::scenes
