#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <revival/math/math.h>

struct RigidBody
{
    JPH::BodyID bodyId;
    JPH::Ref<JPH::Shape> shape;
    bool isStatic = false;

    vec3 halfExtent = vec3(1.0f);
};
