#pragma once

#include <volk.h>
#include <vector>
#include "math/math.h"

struct PipelineDescription
{
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;

    VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
    VkFrontFace cullFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    bool depthTestEnabled = false;

    // shaders
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkShaderModule tessellationControl = VK_NULL_HANDLE;
    VkShaderModule tessellationEval = VK_NULL_HANDLE;
};

struct Vertex
{
    vec3 pos;
    vec2 uv;
    vec3 normal;
};

struct Camera
{
    vec3 position;
    quat rotation;

    float moveSpeed;
    float rotationSpeed;
};

struct Mesh
{
    mat4 matrix;

    int indexOffset;
    int indexCount;

    int materialIndex = -1;
};

// should match the shader
struct Material
{
    int albedoTexture = -1;
    int specularTexture = -1;
    int normalTexture = -1;
    int emissiveTexture = -1;

    alignas(16) vec4 albedoFactor = vec4(1.0);
    float specularFactor = 0.0;
    alignas(16) vec3 emissiveFactor = vec3(0.0);
};

// should match the shader
struct Light
{
    alignas(16) vec3 position;
    alignas(16) vec3 color;
};
