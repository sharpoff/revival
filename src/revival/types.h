#pragma once

#include <vector>
#include <revival/math/math.h>
#include <revival/vulkan/resources.h>

struct Vertex
{
    alignas(16) vec3 pos;
    alignas(16) vec2 uv;
    alignas(16) vec3 normal;
};

// should match the shader
struct Material
{
    int albedoId = -1;
    int specularId = -1;
    int normalId = -1;
    int emissiveId = -1;

    alignas(16) vec4 albedoFactor = vec4(1.0);
    float specularFactor = 0.0;
    alignas(16) vec3 emissiveFactor = vec3(0.0);
};

// should match the shader
struct Light
{
    alignas(16) mat4 mvp;
    alignas(16) vec3 position;
    alignas(16) vec3 color;
    uint shadowMapIndex;
};

struct Mesh
{
    mat4 matrix = mat4(1.0f);

    int indexOffset;
    int indexCount;

    int materialIndex = -1;
};

struct Scene
{
    std::vector<Mesh> meshes;
};

struct Billboard
{
    vec3 position = vec3(0.0f);
    int textureIndex = -1;
    vec2 size = vec2(1.0f);
};

using Attachment = std::pair<VkRenderingAttachmentInfo, Image>;
