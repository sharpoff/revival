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

    void beginFrame(VkCommandBuffer cmd);
    void beginFrame(VkCommandBuffer cmd, Image &depthImage);
    void beginFrame(VkCommandBuffer cmd, std::vector<std::pair<VkRenderingAttachmentInfo, Image>> &attachments, VkExtent2D extent);

    // TODO: add variation of endFrame with arbitrary number of barriers passed to it
    void endFrame(VkCommandBuffer cmd, bool insertBarrier = true);

    // resource creation
    void createBuffer(Buffer &buffer, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO);
    void createImage(Image &image, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageViewType type, VkImageAspectFlags aspect, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode samplerMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, bool cubemap = false);

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
private:
    VkInstance createInstance();
    VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow *window);
    VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance);
    VmaAllocator createAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocatorCreateFlags flags);

    VkPhysicalDevice createPhyiscalDevice(VkInstance instance, VkSurfaceKHR surface, uint32_t &queueFamilyIndex);
    VkDevice createDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice physical, uint32_t queueFamilyIndex);

    VkSwapchainKHR createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex, GLFWwindow *window, VkExtent2D &swapchainExtent);
    std::vector<VkImageView> createSwapchainImageViews(VkDevice device, std::vector<VkImage> &swapchainImages);

    VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex);
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> createCommandBuffers(VkDevice device, VkCommandPool commandPool);

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

    Image depthImage;

    bool resizeRequested = false;

    VkDescriptorPool imGuiDesctiptorPool;
};
