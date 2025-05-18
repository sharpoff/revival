#pragma once

#include <revival/game_object.h>
#include <revival/physics/physics.h>

namespace GameManager
{
    void createGameObject(Physics &physics, std::string name, Scene *scene, Transform transform, vec3 halfExtent, bool isStatic);
    GameObject *getGameObjectByName(std::string name);
    std::vector<GameObject> &getGameObjects();
} // namespace GameManager
