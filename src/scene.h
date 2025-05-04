#pragma once

#include "cgltf.h"
#include "types.h"
#include <filesystem>

struct Scene
{
    mat4 modelMatrix = mat4(1.0);

    std::vector<Mesh> meshes;
    std::vector<Light> lights;
};

bool loadScene(Scene *scene, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices, std::vector<std::filesystem::path> &texturePaths, std::vector<Material> &materials, std::vector<Light> &lights, std::filesystem::path file);
void loadNode(Scene *scene, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices, std::vector<Light> &lights, cgltf_data *data, cgltf_node *node, uint32_t materialOffset);
void loadMesh(Scene *scene, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices, mat4 nodeMatrix, cgltf_data *data, cgltf_mesh *gltfMesh, uint32_t materialOffset);
