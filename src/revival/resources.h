#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

struct Buffer
{
    VkBuffer buffer;
    VkDeviceSize size;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkDeviceAddress address;
};

struct Image
{
    VkImage image;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Texture
{
    Image image;
    VkImageView imageView;
    VkSampler sampler;

    int width;
    int height;

    VkDescriptorSet imguiSet = VK_NULL_HANDLE;
};
