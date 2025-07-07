#pragma once

#include <vector>
#include <array>
#include <volk.h>
#include "vk_mem_alloc.h"
#include <GLFW/glfw3.h>
#include <revival/vulkan/resources.h>
#include <revival/vulkan/common.h>
#include <filesystem>

const int MAX_IMGUI_TEXTURES = 1000;
const int FRAMES_IN_FLIGHT = 2;

class VulkanGraphics
{
public:
    void init(GLFWwindow *window);
    void shutdown();

    VkCommandBuffer beginCommandBuffer();
    void endCommandBuffer(VkCommandBuffer cmd);
    void submitCommandBuffer(VkCommandBuffer cmd);

    // resource creation
    void createBuffer(Buffer &buffer, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO);
    void createImage(Image &image, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

    void destroyBuffer(Buffer &buffer);
    void destroyImage(Image &image);
    void destroyTexture(Texture &texture);

    void uploadBuffer(Buffer &buffer, void *data, VkDeviceSize size);

    VkSampler createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode samplerMode);

    void createTexture(Texture &texture, const TextureInfo &info, VkFormat format);
    void createTextureCubemap(Texture &texture, std::filesystem::path dir, VkFormat format);

    void requestResize();

    // getters
    VkDevice getDevice() { return device; };
    VkExtent2D getSwapchainExtent() { return swapchainExtent; };
    VkImage &getSwapchainImage() { return swapchainImages[imageIndex]; };
    Image &getDepthImage() { return depthImage; };
    VkImageView &getSwapchainImageView() { return swapchainImageViews[imageIndex]; };
    Image &getRenderImage() { return renderImage; };
    VkSampleCountFlagBits getMultisampleCount() { return multisampleCount; };
private:
    void createInstance();
    void createSurface(VkInstance instance, GLFWwindow *window);
    void createDebugMessenger(VkInstance instance);
    void createAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocatorCreateFlags flags);

    void createPhyiscalDevice(VkInstance instance, VkSurfaceKHR surface, uint32_t &queueFamilyIndex);
    void createDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice physical, uint32_t queueFamilyIndex);

    void createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex, GLFWwindow *window, VkExtent2D &swapchainExtent);

    VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex);
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> createCommandBuffers(VkDevice device, VkCommandPool commandPool);

    void createSyncPrimitives();

    VkSemaphore createSemaphore(VkDevice device);
    VkFence createFence(VkDevice device, VkFenceCreateFlags flags);

    void recreateSwapchain();

    void initImGui();
private:
    GLFWwindow *pWindow;

    VkInstance instance;
    VkSurfaceKHR surface;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debugMessenger;
#endif
    VmaAllocator allocator;

    VkPhysicalDevice physicalDevice;
    VkDevice device;

    uint32_t queueFamilyIndex;
    VkQueue queue;

    VkSwapchainKHR swapchain;
    VkExtent2D swapchainExtent;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkCommandPool commandPool;
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;

    std::array<VkSemaphore, FRAMES_IN_FLIGHT> acquireSemaphores;
    std::array<VkSemaphore, FRAMES_IN_FLIGHT> submitSemaphores;
    std::array<VkFence, FRAMES_IN_FLIGHT> finishRenderFences;

    uint32_t imageIndex = 0;
    uint32_t currentFrame = 0;

    const VkSampleCountFlagBits multisampleCount = VK_SAMPLE_COUNT_16_BIT;

    Image renderImage;
    Image depthImage;

    bool resizeRequested = false;

    VkDescriptorPool imGuiDesctiptorPool;
};
