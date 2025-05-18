#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>
#include <revival/types.h>

#include <revival/game_object.h>

class VulkanGraphics;

class ShadowPass
{
public:
    void init(VulkanGraphics &graphics, std::vector<Texture> &textures, Buffer &vertexBuffer);
    void shutdown(VkDevice device);

    void beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer indexBuffer);
    void endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd);

    void render(VkCommandBuffer cmd, Scene &scene);
    void render(VkCommandBuffer cmd, GameObject &gameObject);

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
