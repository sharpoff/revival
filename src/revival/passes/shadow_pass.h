#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>
#include <revival/types.h>

#include <revival/game_object.h>

class VulkanGraphics;

class ShadowPass
{
public:
    void init(VulkanGraphics &graphics, std::vector<Texture> &textures, std::vector<Light> &lights, Buffer &vertexBuffer);
    void shutdown(VkDevice device);

    void beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer indexBuffer, uint32_t shadowMapIndex);
    void endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, uint32_t shadowMapIndex);

    void render(VkCommandBuffer cmd, Scene &scene, mat4 lightMVP);
    void render(VkCommandBuffer cmd, GameObject &gameObject, mat4 lightMVP);

    Image &getShadowMapByLightIndex(uint32_t index) { return shadowMaps[index]; };
private:
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    const uint32_t shadowMapSize = 2048;
    const float depthBiasConstant = 1.25f;
    const float depthBiasSlope = 1.75f;

    std::vector<Image> shadowMaps;
};
