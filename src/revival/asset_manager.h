#pragma once

#include <filesystem>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <revival/types.h>

class VulkanGraphics;

class AssetManager
{
public:
    Scene &loadScene(std::string name, std::filesystem::path path);
    Scene &loadScene2(std::filesystem::path path);

    uint32_t getTextureIndexByFilename(std::string filename);
    Texture &getTextureByIndex(uint32_t index);

    Scene &getSceneByName(std::string name) { return sceneMap[name]; };
    Light &getLightByIndex(uint32_t index) { return lights[index]; };
    Material &getMaterialByIndex(uint32_t index) { return materials[index]; };

    std::vector<Vertex> &getVertices() { return vertices; };
    std::vector<uint32_t> &getIndices() { return indices; };
    std::vector<std::filesystem::path> &getTexturePaths() { return texturePaths; };
    std::vector<Material> &getMaterials() { return materials; };
    std::vector<Light> &getLights() { return lights; };
    std::vector<Scene> &getScenes() { return scenes; };
    std::vector<Texture> &getTextures() { return textures; };

    uint32_t addTexture(VulkanGraphics &graphics, std::string filename);
    uint32_t addMaterial(Material material);
    uint32_t addLight(Light light);
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

    std::vector<Texture> textures;
    std::unordered_map<std::string, uint32_t> textureMap; // index into textures vector
};
