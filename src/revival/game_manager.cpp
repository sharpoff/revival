#include <revival/game_manager.h>

void GameManager::createGameObject(Physics &physics, std::string name, Scene *scene, Transform transform, vec3 halfExtent, bool isStatic)
{
    GameObject &gameObject = gameObjects.emplace_back();
    gameObject.name = name;
    gameObject.scene = scene;
    gameObject.transform = transform;
    physics.createBox(&gameObject.rigidBody, transform, halfExtent, isStatic);

    gameObjectsMap[name] = &gameObject;
}

GameObject *GameManager::getGameObjectByName(std::string name)
{
    return gameObjectsMap[name];
}

std::vector<GameObject> &GameManager::getGameObjects()
{
    return gameObjects;
}
