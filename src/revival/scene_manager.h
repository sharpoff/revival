#pragma once

#include <filesystem>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <revival/types.h>

class SceneManager
{
public:
    Scene &loadScene(std::string name, std::filesystem::path path);
    Scene &loadModel(std::filesystem::path path);

    Scene &getSceneByName(std::string name) { return sceneMap[name]; };
    Light &getLightByIndex(uint32_t idx) { return lights[idx]; };
    Material &getMaterialByIndex(uint32_t idx) { return materials[idx]; };
    std::vector<Vertex> &getVertices() { return vertices; };
    std::vector<uint32_t> &getIndices() { return indices; };
    std::vector<std::filesystem::path> &getTexturePaths() { return texturePaths; };
    std::vector<Material> &getMaterials() { return materials; };
    std::vector<Light> &getLights() { return lights; };
    std::vector<Scene> &getScenes() { return scenes; };
private:
    void processNode(Scene &scene, const aiScene *aScene, const aiNode *aNode, std::filesystem::path directory, uint32_t materialOffset);
    Mesh processMesh(const aiScene *aiScene, const aiMesh *aiMesh, std::filesystem::path directory, uint32_t materialOffset);
    mat4 toGlm(const aiMatrix4x4 &m);

    std::vector<std::filesystem::path> texturePaths;

    std::vector<Material> materials;
    std::vector<Light> lights;

    std::vector<Scene> scenes;
    std::unordered_map<std::string, Scene> sceneMap;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

};
