#pragma once

#include <revival/vulkan/common.h>

namespace vkutils
{
    // primitive
    VkSemaphore createSemaphore(VkDevice device);
    VkFence createFence(VkDevice device, VkFenceCreateFlags flags);

    // barrier
    void insertImageBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange);

    // descriptor
    VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding> &bindings);
    VkDescriptorSet createDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);
    VkDescriptorPool createDescriptorPool(VkDevice device, std::vector<VkDescriptorPoolSize> &poolSizes);
    void writeBuffer(VkDevice device, VkDescriptorSet set, uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkBuffer &buffer, VkDeviceSize size);
    void writeImage(VkDevice device, VkDescriptorSet set, uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkImageView imageView, VkSampler sampler);
    void writeImages(VkDevice device, VkDescriptorSet set, uint32_t binding, VkDescriptorType type, std::vector<VkDescriptorImageInfo> infos);

    // pipeline
    VkPipelineLayout createPipelineLayout(VkDevice device, VkPushConstantRange pushConstant);
    VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout);
    VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout, VkPushConstantRange pushConstant);
    VkShaderModule loadShaderModule(VkDevice device, const char *path);

    void setDebugName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char *name);
    void beginDebugLabel(VkCommandBuffer cmd, const char *name, vec4 color = {1.0, 1.0, 1.0, 1.0});
    void endDebugLabel(VkCommandBuffer cmd);

    void setViewport(VkCommandBuffer cmd, float x, float y, float width, float height);
    void setScissor(VkCommandBuffer cmd, VkExtent2D extent);
} // namespace vkutils
