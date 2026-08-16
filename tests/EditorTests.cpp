// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editor behaviour that is easy to break and impossible to notice: picking maths
// and the undo stack. No window, no GPU - these are questions about entities.
//
// Run with `ctest --test-dir build` or by executing the binary directly; it
// returns the number of failed checks.

#include "lucida/framework/Commands.h"
#include "lucida/framework/Picking.h"
#include "lucida/render/Components.h"
#include <cstdio>
using namespace lucida;

int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

int main() {
    Registry reg;

    // A unit box sitting 5 m in front of a camera at the origin looking down -Z.
    const Entity box = reg.Create("box");
    reg.Get<LocalTransform>(box)->position = Vec3(0, 0, -5);
    reg.Add<LocalBounds>(box, LocalBounds{Vec3(-1.0f), Vec3(1.0f)});
    UpdateWorldTransforms(reg);

    CameraState cam;
    cam.position = Vec3(0, 0, 0);
    cam.yaw = -kHalfPi;   // -Z
    cam.pitch = 0.0f;

    const f32 aspect = 16.0f / 9.0f;

    PickResult centre = PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0, 0)));
    check(centre.Hit() && centre.entity == box, "click in the centre selects the box");
    check(centre.distance > 3.9f && centre.distance < 4.1f, "reported distance is the box face (4 m)");

    PickResult edge = PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0.95f, 0.0f)));
    check(!edge.Hit(), "click far to the side selects nothing");

    reg.Get<Visibility>(box)->visible = false;
    check(!PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0, 0))).Hit(),
          "a hidden entity cannot be picked");
    reg.Get<Visibility>(box)->visible = true;

    // Scaling the entity must scale what you can click on.
    reg.Get<LocalTransform>(box)->scale = 0.05f;
    UpdateWorldTransforms(reg);
    check(!PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0.2f, 0))).Hit(),
          "shrinking the entity shrinks its pick volume");
    reg.Get<LocalTransform>(box)->scale = 1.0f;
    UpdateWorldTransforms(reg);

    // --- command stack
    CommandStack stack;
    LocalTransform before = *reg.Get<LocalTransform>(box);
    LocalTransform after = before;
    after.position = Vec3(3, 0, -5);

    stack.Execute(std::make_unique<TransformCommand>(reg, box, before, after, "Move"));
    check(reg.Get<LocalTransform>(box)->position.x == 3.0f, "execute applies the change");
    check(stack.CanUndo() && !stack.CanRedo(), "stack has one undo, no redo");

    stack.Undo();
    check(reg.Get<LocalTransform>(box)->position.x == 0.0f, "undo restores the old value");
    check(stack.CanRedo(), "undo makes redo available");

    stack.Redo();
    check(reg.Get<LocalTransform>(box)->position.x == 3.0f, "redo reapplies it");

    // A new edit must discard the redo branch.
    stack.Undo();
    LocalTransform other = before;
    other.position = Vec3(0, 7, -5);
    stack.Execute(std::make_unique<TransformCommand>(reg, box, before, other, "Move up"));
    check(!stack.CanRedo(), "a new edit discards the redo branch");

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : "SOME CHECKS FAILED");
    return failures;
}
