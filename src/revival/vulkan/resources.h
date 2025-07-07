#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <stb_image.h>
#include <string>
#include <iomanip>
#include <revival/logger.h>

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
    std::string path;
    unsigned char *pixels = nullptr;

    int width = 0;
    int height = 0;
    int channels = STBI_rgb_alpha;
    bool loaded = false;

    void load(std::string filename) {
        int width, height, channels;
        unsigned char *data = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (!data) {
            Logger::println(LOG_ERROR, "Failed to load texture data ", std::quoted(filename));
            return;
        }

        this->width = width;
        this->height = height;
        channels = STBI_rgb_alpha;
        pixels = data;
        path = filename;
        loaded = true;
    }

    TextureInfo() {}

    TextureInfo(std::string filename) {
        load(filename);
    }

    ~TextureInfo() {
        if (loaded)
            stbi_image_free(pixels);
    }
};

struct Texture
{
    Image image;
    TextureInfo info;
};
