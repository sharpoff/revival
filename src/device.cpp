#include "device.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <stb_image.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

VkBool32 VKAPI_PTR debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    const char *prefix = "";
    if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        prefix = "GENERAL";
    }
    else if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        prefix = "VALIDATION";
    }
    else if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        prefix = "PERFORMANCE";
    }

    printf("[%s] %s\n", prefix, pCallbackData->pMessage);

    return VK_FALSE; // always return false
}

void VulkanDevice::create(GLFWwindow *window)
{
    pWindow = window;

    // vulkan initialization
    VK_CHECK(volkInitialize());
    instance = createInstance();
    volkLoadInstance(instance);

#ifndef NDEBUG
    debugMessenger = createDebugMessenger(instance);
#endif

    surface = createSurface(instance, window);

    physicalDevice = createPhyiscalDevice(instance, surface, queueFamilyIndex);
    device = createDevice(instance, surface, physicalDevice, queueFamilyIndex);
    volkLoadDevice(device);

    // graphics/present queue
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    assert(queue);

    // create swapchain
    swapchain = createSwapchain(device, physicalDevice, surface, queueFamilyIndex, window, swapchainExtent);

    // get swapchain images
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    swapchainImageViews = createSwapchainImageViews(device, swapchainImages);

    createImage(depthImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depthImageView = createImageView(depthImage.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);


    // command pool
    commandPool = createCommandPool(device, queueFamilyIndex);
    commandBuffers = createCommandBuffers(device, commandPool);

    // synchronization primitives
    for (unsigned int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        acquireSemaphores[i] = createSemaphore(device);
        submitSemaphores[i] = createSemaphore(device);
        finishRenderFences[i] = createFence(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    initializeImGui();
}

void VulkanDevice::destroy()
{
    vkDeviceWaitIdle(device);

    // dear imgui
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, imGuiDesctiptorPool, nullptr);

    vkDestroyImageView(device, depthImageView, nullptr);

    destroyImage(depthImage);

    for (unsigned int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, acquireSemaphores[i], nullptr);
        vkDestroySemaphore(device, submitSemaphores[i], nullptr);
        vkDestroyFence(device, finishRenderFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);

    for (auto &imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    vkDestroyDevice(device, nullptr);

#ifndef NDEBUG
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

VkInstance VulkanDevice::createInstance()
{
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = 0;
    appInfo.pEngineName = "Engine";
    appInfo.engineVersion = 0;
    appInfo.apiVersion = API_VERSION;

    bool isDebug = false;
#ifndef NDEBUG
    isDebug = true;
#endif

    // Vulkan extensions
    uint32_t supportedExtensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr));
    std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data()));

    uint32_t count;
    const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char *> extensions(requiredExtensions, requiredExtensions + count);
    if (isDebug)
        extensions.push_back("VK_EXT_debug_utils");

    // check extensions support
    for (auto requiredExtension : extensions) {
        bool found = false;
        for (VkExtensionProperties supportedExtension : supportedExtensions) {
            if (strcmp(requiredExtension, supportedExtension.extensionName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            printf("Instance extension is not supported.\n");
        }
    }

    // Vulkan layers
    uint32_t supportedLayerCount = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr));
    std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data()));

    std::vector<const char *> layers = {};
    if (isDebug)
        layers.push_back("VK_LAYER_KHRONOS_validation");

    // check layers support
    for (auto requiredLayer : layers) {
        bool found = false;
        for (VkLayerProperties supportedLayer : supportedLayers) {
            if (strcmp(requiredLayer, supportedLayer.layerName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            printf("Instance layer is not supported.\n");
        }
    }

    VkInstanceCreateInfo instanceCI = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCI.pApplicationInfo = &appInfo;
    instanceCI.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instanceCI.ppEnabledLayerNames = layers.data();
    instanceCI.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceCI.ppEnabledExtensionNames = extensions.data();
    if (isDebug) {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messengerInfo.pfnUserCallback = debugCallback;
        messengerInfo.pUserData = nullptr;

        instanceCI.pNext = &messengerInfo;
    }

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &instance));

    return instance;
}

VkSurfaceKHR VulkanDevice::createSurface(VkInstance instance, GLFWwindow *window)
{
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));

    return surface;
}

VkDebugUtilsMessengerEXT VulkanDevice::createDebugMessenger(VkInstance instance)
{
    VkDebugUtilsMessengerEXT messenger;

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerInfo.pfnUserCallback = debugCallback;
    messengerInfo.pUserData = nullptr;

    vkCreateDebugUtilsMessengerEXT(instance, &messengerInfo, nullptr, &messenger);

    return messenger;
}

VkPhysicalDevice VulkanDevice::createPhyiscalDevice(VkInstance instance, VkSurfaceKHR surface, uint32_t &queueFamilyIndex)
{
    VkPhysicalDevice physicalDevice;

    // Physical device
    uint32_t physicalDeviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()));

    bool deviceFound = false;
    for (uint32_t i = 0; i < physicalDevices.size(); i++) {
        // properties
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);

        // getting queue families
        uint32_t queueFamilyPropsCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropsCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropsCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropsCount, queueFamilyProps.data());

        // check if supports graphics and present queue
        bool haveQueues = false;
        for (uint32_t j = 0; j < queueFamilyPropsCount; j++) {
            VkBool32 presentSupported;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], j, surface, &presentSupported);
            if (((queueFamilyProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT) && (presentSupported == VK_TRUE)) {
                haveQueues = true;
                queueFamilyIndex = j;
            }
        }

        // get features
        VkPhysicalDeviceVulkan12Features features12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &features12;
        vkGetPhysicalDeviceFeatures2(physicalDevices[i], &features);

        // check features vk 1.0
        bool features10Supported = false;
        if (features.features.fillModeNonSolid && features.features.shaderSampledImageArrayDynamicIndexing) {
            features10Supported = true;
        }

        // check features vk 1.2
        bool features12Supported = false;
        if (features12.runtimeDescriptorArray && features12.shaderSampledImageArrayNonUniformIndexing) {
            features12Supported = true;
        }

        // check device type
        if (haveQueues && features10Supported && features12Supported) {
            physicalDevice = physicalDevices[i];
            deviceFound = true;
            break;
        }
    }
    assert(deviceFound);

    return physicalDevice;
}

VkDevice VulkanDevice::createDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice physical, uint32_t queueFamilyIndex)
{
    // get device extensions
    uint32_t supportedDeviceExtensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physical, nullptr, &supportedDeviceExtensionCount, nullptr));
    std::vector<VkExtensionProperties> supportedDeviceExtensions(supportedDeviceExtensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physical, nullptr, &supportedDeviceExtensionCount, supportedDeviceExtensions.data()));

    std::vector<const char*> deviceExtensions = {"VK_KHR_swapchain"};

    // check support
    for (auto requiredExtension : deviceExtensions) {
        bool found = false;
        for (VkExtensionProperties supportedExtension : supportedDeviceExtensions) {
            if (strcmp(requiredExtension, supportedExtension.extensionName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            printf("Device extension is not supported.\n");
            exit(EXIT_FAILURE);
        }
    }

    // vk 1.0 features
    VkPhysicalDeviceFeatures features10 = {};
    features10.fillModeNonSolid = VK_TRUE;
    features10.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

    // vk 1.2 features
    VkPhysicalDeviceVulkan12Features features12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    // dynamic rendering features
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    dynamicRenderingFeatures.pNext = &features12;

    // Queue family info
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    // Logical device
    VkDeviceCreateInfo deviceInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceInfo.pNext = &dynamicRenderingFeatures;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceInfo.pEnabledFeatures = &features10;

    VkDevice device;
    VK_CHECK(vkCreateDevice(physical, &deviceInfo, nullptr, &device));

    return device;
}

VkSwapchainKHR VulkanDevice::createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex, GLFWwindow *window, VkExtent2D &swapchainExtent)
{
    // Get surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    swapchainExtent.width = std::clamp(uint32_t(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    swapchainExtent.height = std::clamp(uint32_t(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    VkSwapchainCreateInfoKHR swapchainCI{};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = capabilities.minImageCount;
    swapchainCI.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;  // TODO: change hardcoded format
    swapchainCI.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCI.imageExtent = swapchainExtent;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCI.queueFamilyIndexCount = 1;
    swapchainCI.pQueueFamilyIndices = &queueFamilyIndex;
    swapchainCI.preTransform = capabilities.currentTransform;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCI.presentMode = VK_PRESENT_MODE_FIFO_KHR; // NOTE: this only works with vsync

    VkSwapchainKHR swapchain;
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));
    return swapchain;
}

std::vector<VkImageView> VulkanDevice::createSwapchainImageViews(VkDevice device, std::vector<VkImage> &swapchainImages)
{
    // create swapchain image views from images
    std::vector<VkImageView> swapchainImageViews(swapchainImages.size());
    for (uint32_t i = 0; i < swapchainImageViews.size(); i++) {
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]));
    }

    return swapchainImageViews;
}

VkCommandPool VulkanDevice::createCommandPool(VkDevice device, uint32_t queueFamilyIndex)
{
    VkCommandPool commandPool;
    VkCommandPoolCreateInfo commandPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndex;

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

    return commandPool;
}

std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> VulkanDevice::createCommandBuffers(VkDevice device, VkCommandPool commandPool)
{
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;
    VkCommandBufferAllocateInfo bufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    bufferAllocInfo.commandPool = commandPool;
    bufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferAllocInfo.commandBufferCount = commandBuffers.size();

    VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocInfo, commandBuffers.data()));

    return commandBuffers;
}

void VulkanDevice::recreateSwapchain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(pWindow, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(pWindow, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    for (auto &imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    destroyImage(depthImage);
    vkDestroyImageView(device, depthImageView, nullptr);

    for (unsigned int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, acquireSemaphores[i], nullptr);
        vkDestroySemaphore(device, submitSemaphores[i], nullptr);
        vkDestroyFence(device, finishRenderFences[i], nullptr);
        acquireSemaphores[i] = createSemaphore(device);
        submitSemaphores[i] = createSemaphore(device);
        finishRenderFences[i] = createFence(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    swapchain = createSwapchain(device, physicalDevice, surface, queueFamilyIndex, pWindow, swapchainExtent);

    // get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    swapchainImageViews = createSwapchainImageViews(device, swapchainImages);

    createImage(depthImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    depthImageView = createImageView(depthImage.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
};

void VulkanDevice::draw(std::function<void()> drawFunction)
{
    VK_CHECK(vkWaitForFences(device, 1, &finishRenderFences[currentFrame], VK_TRUE, ~0ull));
    VK_CHECK(vkResetFences(device, 1, &finishRenderFences[currentFrame]));

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, ~0ull, acquireSemaphores[currentFrame], nullptr, &imageIndex);
    if (isResizing || result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        isResizing = false;
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        printf("Failed to acquire swapchain image.\n");
        exit(EXIT_FAILURE);
    }
    VkCommandBuffer &cmd = commandBuffers[currentFrame];

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Command buffer begin
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    insertImageBarrier(cmd, swapchainImages[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    insertImageBarrier(cmd, depthImage.image, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});

    VkRenderingAttachmentInfo colorAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachmentInfo.clearValue = {{{0.0, 0.0, 0.0, 1.0}}};
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.imageView = swapchainImageViews[imageIndex];
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttachmentInfo.clearValue.depthStencil = {0.0, 0};
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.imageView = depthImageView;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;
    renderingInfo.renderArea.extent = swapchainExtent;
    renderingInfo.renderArea.offset = {0};
    renderingInfo.layerCount = 1;

    // Rendering begin
    vkCmdBeginRendering(cmd, &renderingInfo);

    // Setup dynamic state
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = swapchainExtent.width;
    viewport.height = swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Actually draw frame
    drawFunction();

    // Rendering end
    vkCmdEndRendering(cmd);

    // transition image for presentation
    insertImageBarrier(cmd, swapchainImages[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // Command buffer end
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit
    VkSubmitInfo submit = {};
    VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &acquireSemaphores[currentFrame];
    submit.pWaitDstStageMask = stages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &submitSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, finishRenderFences[currentFrame]));

    // Present
    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &submitSemaphores[currentFrame];

    result = vkQueuePresentKHR(queue, &presentInfo);
    if (isResizing || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
        isResizing = false;
    }

    currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;
}

// https://registry.khronos.org/vulkan/specs/latest/man/html/VkPhysicalDeviceMemoryProperties.html
int32_t findMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties* pMemoryProperties, uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties) {
    for (uint32_t memoryIndex = 0; memoryIndex < pMemoryProperties->memoryTypeCount; ++memoryIndex)
        if ((memoryTypeBitsRequirement & (1 << memoryIndex)) && (pMemoryProperties->memoryTypes[memoryIndex].propertyFlags & requiredProperties) == requiredProperties)
            return static_cast<int32_t>(memoryIndex);

    // failed to find memory type
    return -1;
}

void VulkanDevice::createBuffer(Buffer &buffer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
{
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer.buffer));

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &memoryRequirements);

    int32_t memoryTypeIndex = findMemoryTypeIndex(&memoryProperties, memoryRequirements.memoryTypeBits, memoryFlags);
    assert(memoryTypeIndex != -1);

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    allocInfo.allocationSize = memoryRequirements.size;

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &buffer.memory));
    VK_CHECK(vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0));

    if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        VK_CHECK(vkMapMemory(device, buffer.memory, 0, size, 0, &buffer.mapped));
    
    buffer.size = size;
}

void VulkanDevice::destroyBuffer(Buffer &buffer)
{
    vkDestroyBuffer(device, buffer.buffer, nullptr);
    vkFreeMemory(device, buffer.memory, nullptr);
}

void VulkanDevice::createImage(Image &image, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryFlags)
{
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.usage = usage;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;

    VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image.image));

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image.image, &memoryRequirements);

    int32_t memoryTypeIndex = findMemoryTypeIndex(&memoryProperties, memoryRequirements.memoryTypeBits, memoryFlags);
    assert(memoryTypeIndex != -1);

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    allocInfo.allocationSize = memoryRequirements.size;

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &image.memory));
    VK_CHECK(vkBindImageMemory(device, image.image, image.memory, 0));
}

VkImageView VulkanDevice::createImageView(VkImage image, VkImageViewType type, VkFormat format, VkImageAspectFlags aspect)
{
    VkImageViewCreateInfo imageViewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewInfo.image = image;
    imageViewInfo.viewType = type;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.format = format;
    imageViewInfo.subresourceRange = {aspect, 0, 1, 0, 1};

    VkImageView imageView;
    vkCreateImageView(device, &imageViewInfo, nullptr, &imageView);

    return imageView;
}

void VulkanDevice::destroyImage(Image &image)
{
    vkDestroyImage(device, image.image, nullptr);
    vkFreeMemory(device, image.memory, nullptr);
}

void VulkanDevice::uploadBuffer(Buffer &buffer, void *data, VkDeviceSize size)
{
    Buffer staging;
    createBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    memcpy(staging.mapped, data, size);

    // create temporary command buffer
    VkCommandBuffer copyCmd;
    VkCommandBufferAllocateInfo bufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    bufferAllocInfo.commandPool = commandPool;
    bufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferAllocInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocInfo, &copyCmd));

    VkCommandBufferBeginInfo cmdBegin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(copyCmd, &cmdBegin));

    // copy
    VkBufferCopy copyRegion = {0, 0, size};
    vkCmdCopyBuffer(copyCmd, staging.buffer, buffer.buffer, 1, &copyRegion);

    VK_CHECK(vkEndCommandBuffer(copyCmd));

    // submit
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &copyCmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, nullptr));

    VK_CHECK(vkQueueWaitIdle(queue));

    destroyBuffer(staging);
}

VkSampler VulkanDevice::createSampler(VkFilter minFilter, VkFilter magFilter)
{
    // TODO: add anisotropy feature
    VkSamplerCreateInfo createInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    createInfo.magFilter = minFilter;
    createInfo.minFilter = magFilter;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.compareOp = VK_COMPARE_OP_NEVER;
    createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(device, &createInfo, nullptr, &sampler));

    return sampler;
}

void VulkanDevice::createTexture(Texture &texture, const char *file, VkFormat format)
{
    int width, height, channels;
    unsigned char *data = stbi_load(file, &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        printf("Failed to laod texture '%s'\n", file);
        return;
    }

    uint32_t size = width * height * STBI_rgb_alpha;

    createImage(texture.image, width, height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer staging;
    createBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memcpy(staging.mapped, data, size);

    // create temporary command buffer
    VkCommandBuffer copyCmd;
    VkCommandBufferAllocateInfo bufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    bufferAllocInfo.commandPool = commandPool;
    bufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferAllocInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocInfo, &copyCmd));

    VkCommandBufferBeginInfo cmdBegin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(copyCmd, &cmdBegin));

    // transition image to transfer
    insertImageBarrier(
        copyCmd,
        texture.image.image,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // copy
    VkBufferImageCopy copyRegion = {};
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    vkCmdCopyBufferToImage(copyCmd, staging.buffer, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // transition image to fragment shader
    insertImageBarrier(
        copyCmd,
        texture.image.image,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    VK_CHECK(vkEndCommandBuffer(copyCmd));

    // submit
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &copyCmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, nullptr));
    VK_CHECK(vkQueueWaitIdle(queue));

    texture.sampler = createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR);
    texture.imageView = createImageView(texture.image.image, VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_COLOR_BIT);

    destroyBuffer(staging);
}

void VulkanDevice::requestResize()
{
    isResizing = true;
}

void VulkanDevice::initializeImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Create descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
    };
    imGuiDesctiptorPool = createDescriptorPool(device, poolSizes);

    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB; // TODO: change hardcoded format

    VkPipelineRenderingCreateInfoKHR pipelineRenderingCI = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    pipelineRenderingCI.colorAttachmentCount = 1;
    pipelineRenderingCI.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingCI.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(pWindow, true);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = API_VERSION;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamilyIndex;
    initInfo.Queue = queue;
    initInfo.DescriptorPool = imGuiDesctiptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.RenderPass = nullptr;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = pipelineRenderingCI;

    ImGui_ImplVulkan_Init(&initInfo);
}
