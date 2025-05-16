#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <stb_image.h>

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
    VkImage handle;
    VkImageView view;
    VkSampler sampler;

    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct TextureInfo
{
    const char *path = "";
    unsigned char *pixels = nullptr;

    int width = 0;
    int height = 0;
    int channels = STBI_rgb_alpha;

    bool loaded;
    ~TextureInfo()
    {
        if (loaded)
            stbi_image_free(pixels);
    }
};

struct Texture
{
    Image image;
    TextureInfo info;
};
