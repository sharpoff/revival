#pragma once

#include <volk.h>

struct Buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    void *mapped;
};

struct Image
{
    VkImage image;
    VkDeviceMemory memory;
};

struct Texture
{
    Image image;
    VkImageView imageView;
    VkSampler sampler;
};
