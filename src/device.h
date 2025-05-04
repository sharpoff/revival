#pragma once

#include <vector>
#include "common.h"
#include <GLFW/glfw3.h>
#include <array>
#include "resources.h"
#include <functional>

class VulkanDevice
{
public:
    void create(GLFWwindow *window);
    void destroy();

    void draw(std::function<void()> drawFunction);

    // resource creation
    void createBuffer(Buffer &buffer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags);
    void createImage(Image &image, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryFlags);
    VkImageView createImageView(VkImage image, VkImageViewType type, VkFormat format, VkImageAspectFlags aspect);

    void destroyBuffer(Buffer &buffer);
    void destroyImage(Image &image);

    void uploadBuffer(Buffer &buffer, void *data, VkDeviceSize size);

    VkSampler createSampler(VkFilter minFilter, VkFilter magFilter);

    void createTexture(Texture &texture, const char *file, VkFormat format);

    void requestResize();

    // getters
    VkDevice getDevice() { return device; };
    VkCommandBuffer getCommandBuffer() { return commandBuffers[currentFrame]; };
    VkExtent2D getSwapchainExtent() { return swapchainExtent; };
private:
    VkInstance createInstance();
    VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow *window);
    VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance);
    VkPhysicalDevice createPhyiscalDevice(VkInstance instance, VkSurfaceKHR surface, uint32_t &queueFamilyIndex);
    VkDevice createDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice physical, uint32_t queueFamilyIndex);

    VkSwapchainKHR createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex, GLFWwindow *window, VkExtent2D &swapchainExtent);
    std::vector<VkImageView> createSwapchainImageViews(VkDevice device, std::vector<VkImage> &swapchainImages);

    VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex);
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> createCommandBuffers(VkDevice device, VkCommandPool commandPool);

    void recreateSwapchain();

    void initializeImGui();

private:
    GLFWwindow *pWindow;

    VkInstance instance;
    VkSurfaceKHR surface;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debugMessenger;
#endif

    VkPhysicalDevice physicalDevice;
    VkDevice device;

    uint32_t queueFamilyIndex;
    VkQueue queue;

    VkSwapchainKHR swapchain;
    VkExtent2D swapchainExtent;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    Image depthImage;
    VkImageView depthImageView;

    VkCommandPool commandPool;
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;

    std::array<VkSemaphore, FRAMES_IN_FLIGHT> acquireSemaphores;
    std::array<VkSemaphore, FRAMES_IN_FLIGHT> submitSemaphores;
    std::array<VkFence, FRAMES_IN_FLIGHT> finishRenderFences;

    uint32_t imageCount = 0;
    uint32_t currentFrame = 0;

    bool isResizing = false;

    // dear imgui
    VkDescriptorPool imGuiDesctiptorPool;
};
