#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>
#include <revival/types.h>
#include <revival/camera.h>

class VulkanGraphics;

class SkyboxPass
{
public:
    void init(VulkanGraphics &graphics, Texture &skybox);
    void shutdown(VulkanGraphics &graphics, VkDevice device);

    void render(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer &vertexBuffer, VkBuffer &indexBuffer, Camera &camera, Scene &cubeScene);
private:
    void updateUniformBuffers(Camera &camera);

    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    struct UBO
    {
        mat4 projection;
        mat4 view;
    };
    Buffer uboBuffer;
};
