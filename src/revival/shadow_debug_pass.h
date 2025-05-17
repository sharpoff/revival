
#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>

class VulkanGraphics;

class ShadowDebugPass
{
public:
    void init(VulkanGraphics &graphics, Buffer &vertexBuffer, Image &shadowMap);
    void shutdown(VkDevice device);

    void render(VulkanGraphics &graphics, VkCommandBuffer cmd);
private:
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;
};
