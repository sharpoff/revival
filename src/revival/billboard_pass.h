#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>
#include <revival/types.h>

class VulkanGraphics;

class BillboardPass
{
public:
    void init(VulkanGraphics &graphics);
    void shutdown(VkDevice device);

    void beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd);
    void endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd);

    void render(VkCommandBuffer cmd);
private:
    void updateBuffers();

    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    struct UBO
    {
        alignas(16) mat4 viewProj;
        alignas(16) vec3 cameraUp;
        alignas(16) vec3 cameraRight;
        alignas(16) vec3 center;
        alignas(8) vec2 size;
    };
};
