#pragma once

#include <filesystem>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <revival/types.h>

namespace SceneManager
{
    Scene &loadScene(std::string name, std::filesystem::path path);
    Scene &loadModel(std::filesystem::path path);

    void processNode(Scene &scene, const aiScene *aScene, const aiNode *aNode, std::filesystem::path directory, uint32_t materialOffset);
    Mesh processMesh(const aiScene *aiScene, const aiMesh *aiMesh, std::filesystem::path directory, uint32_t materialOffset);
    mat4 toGlm(const aiMatrix4x4 &m);

    Scene &getSceneByName(std::string name);
    Light &getLightByIndex(uint32_t idx);
    Material &getMaterialByIndex(uint32_t idx);
    std::vector<Vertex> &getVertices();
    std::vector<uint32_t> &getIndices();
    std::vector<std::filesystem::path> &getTexturePaths();
    std::vector<Material> &getMaterials();
    std::vector<Light> &getLights();
    std::vector<Scene> &getScenes();
} // namespace SceneManager
