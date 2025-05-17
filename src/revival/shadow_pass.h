#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>

class VulkanGraphics;

class ShadowPass
{
public:
    void init(VulkanGraphics &graphics, std::vector<Texture> &textures, Buffer &vertexBuffer);
    void shutdown(VkDevice device);

    void render(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer indexBuffer);

    Image &getShadowMap() { return shadowMap; };
    unsigned int getShadowMapIndex() { return shadowMapIndex; };
private:
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    const uint32_t shadowMapSize = 2048;

    Image shadowMap;
    unsigned int shadowMapIndex;
};
