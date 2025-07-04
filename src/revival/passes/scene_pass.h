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

    void beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer &indexBuffer);
    void endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd);

    void render(VkCommandBuffer cmd, Mesh &mesh);

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
