#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>
#include <revival/types.h>

class VulkanGraphics;

class ScenePass
{
public:
    void init(VulkanGraphics &graphics, std::vector<Texture> &textures, Buffer &vertexBuffer, Buffer &uboBuffer, Buffer &materialsBuffer, Buffer &lightsBuffer);
    void shutdown(VkDevice device);

    void render(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer indexBuffer, Scene *scenes, uint32_t scenesCount);
private:
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    struct PushConstant
    {
        alignas(16) mat4 model;
        int materialIndex;
    };
};
