#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>
#include <revival/types.h>
#include <revival/camera.h>

class VulkanGraphics;

class BillboardPass
{
public:
    void init(VulkanGraphics &graphics, std::vector<Texture> &textures);
    void shutdown(VulkanGraphics &graphics, VkDevice device);

    void beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, Camera &camera);
    void endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd);

    void render(VkCommandBuffer cmd, VkDevice device, vec3 center, vec2 size, int textureIndex);
private:
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
    };
    Buffer uboBuffer;

    struct PushConstant
    {
        alignas(16) vec3 center;
        alignas(8) vec2 size;
        alignas(4) int textureIndex;
    };

    Buffer vertexBuffer;
};
