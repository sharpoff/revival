#pragma once

#include <revival/transform.h>
#include <revival/types.h>
#include <string>
#include <revival/physics/rigid_body.h>

struct GameObject
{
    std::string name = "Game Object";
    Transform transform = Transform();

    Scene *scene;
    RigidBody rigidBody;
};
