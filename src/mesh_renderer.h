#pragma once
#include "device.h"
#include "scene.h"

class MeshRenderer
{
public:
    MeshRenderer(std::vector<Material> &materials, std::vector<Light> &lights, Camera &camera);

    void create(VulkanDevice &vkDevice, std::vector<Texture> &textures);
    void destroy(VulkanDevice &vkDevice);
    void draw(Scene &scene, VulkanDevice &vkDevice, Buffer &vertexBuffer, Buffer &indexBuffer);
private:
    void updateDynamicBuffers(VulkanDevice &vkDevice, Camera &camera);

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    struct UniformBuffer
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };

    struct MeshPush
    {
        alignas(16) mat4 model;
        int materialIndex;
    };

    UniformBuffer ubo;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    // references from engine
    std::vector<Light> &lights;
    std::vector<Material> &materials;
    Camera &camera;
};
