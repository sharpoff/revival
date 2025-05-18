#include <revival/game_manager.h>

namespace GameManager
{
    std::vector<GameObject> gameObjects;
    std::unordered_map<std::string, GameObject*> gameObjectsMap;

    void createGameObject(Physics &physics, std::string name, Scene *scene, Transform transform, vec3 halfExtent, bool isStatic)
    {
        GameObject &gameObject = gameObjects.emplace_back();
        gameObject.name = name;
        gameObject.scene = scene;
        gameObject.transform = transform;
        physics.createBox(&gameObject.rigidBody, transform, halfExtent, isStatic);

        gameObjectsMap[name] = &gameObject;
    }

    GameObject *getGameObjectByName(std::string name)
    {
        return gameObjectsMap[name];
    }

    std::vector<GameObject> &getGameObjects()
    {
        return gameObjects;
    }
} // namespace GameManager
