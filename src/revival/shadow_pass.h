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

    void render(VkCommandBuffer cmd, Scene &scene, mat4 lightMVP);
    void render(VkCommandBuffer cmd, GameObject &gameObject, mat4 lightMVP);

    Image &getShadowMap() { return shadowMap; };
    unsigned int getShadowMapIndex() { return shadowMapIndex; };
private:
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    const uint32_t shadowMapSize = 2048;
    const float depthBiasConstant = 1.25f;
    const float depthBiasSlope = 1.75f;

    Image shadowMap;
    unsigned int shadowMapIndex;
};
