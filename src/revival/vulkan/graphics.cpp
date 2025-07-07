#include <revival/vulkan/graphics.h>
#include <revival/vulkan/utils.h>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <stb_image.h>

#include <revival/renderer.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"


#ifndef NDEBUG
    bool isDebug = true;
#else
    bool isDebug = false;
#endif

VkBool32 VKAPI_PTR debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    const char *prefix = "UNDEFINED";
    if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        prefix = "GENERAL";
    else if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        prefix = "VALIDATION";
    else if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        prefix = "PERFORMANCE";

    printf("[%s] %s\n", prefix, pCallbackData->pMessage);

    return VK_FALSE; // always return false
}

void VulkanGraphics::init(GLFWwindow *window)
{
    pWindow = window;

    // vulkan initialization
    VK_CHECK(volkInitialize());
    createInstance();
    volkLoadInstance(instance);

    createDebugMessenger(instance);

    createSurface(instance, window);

    createPhyiscalDevice(instance, surface, queueFamilyIndex);
    createDevice(instance, surface, physicalDevice, queueFamilyIndex);
    volkLoadDevice(device);

    createAllocator(instance, device, physicalDevice, 0);

    // create swapchain
    createSwapchain(device, physicalDevice, surface, queueFamilyIndex, window, swapchainExtent);

    // command pool
    commandPool = createCommandPool(device, queueFamilyIndex);
    commandBuffers = createCommandBuffers(device, commandPool);

    createSyncPrimitives();

    createImage(renderImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, multisampleCount);
    createImage(depthImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, multisampleCount);

    initImGui();
}

void VulkanGraphics::shutdown()
{
    vkDeviceWaitIdle(device);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, imGuiDesctiptorPool, nullptr);

    destroyImage(renderImage);
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

    vmaDestroyAllocator(allocator);

    vkDestroyDevice(device, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);

    if (isDebug)
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    vkDestroyInstance(instance, nullptr);
}

void VulkanGraphics::createInstance()
{
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = 0;
    appInfo.pEngineName = "Engine";
    appInfo.engineVersion = 0;
    appInfo.apiVersion = API_VERSION;

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

        if (!found)
            Logger::println(LOG_ERROR, "Instance extension is not supported.");
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

        if (!found)
            Logger::println(LOG_ERROR, "Instance layer is not supported.\n");
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

    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &instance));
}

void VulkanGraphics::createSurface(VkInstance instance, GLFWwindow *window)
{
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));
}

void VulkanGraphics::createDebugMessenger(VkInstance instance)
{
    #ifndef NDEBUG
        return;
    #endif
    VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerInfo.pfnUserCallback = debugCallback;
    messengerInfo.pUserData = nullptr;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &messengerInfo, nullptr, &debugMessenger));
}

void VulkanGraphics::createAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocatorCreateFlags flags)
{
    VmaVulkanFunctions vmaFunctions = {};
    vmaFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    vmaFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vmaFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vmaFunctions.vkAllocateMemory = vkAllocateMemory;
    vmaFunctions.vkFreeMemory = vkFreeMemory;
    vmaFunctions.vkMapMemory = vkMapMemory;
    vmaFunctions.vkUnmapMemory = vkUnmapMemory;
    vmaFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vmaFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vmaFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vmaFunctions.vkBindImageMemory = vkBindImageMemory;
    vmaFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vmaFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vmaFunctions.vkCreateBuffer = vkCreateBuffer;
    vmaFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vmaFunctions.vkCreateImage = vkCreateImage;
    vmaFunctions.vkDestroyImage = vkDestroyImage;
    vmaFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
    vmaFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
    vmaFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
    vmaFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;
    vmaFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;
    vmaFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;

    VmaAllocatorCreateInfo createInfo = {};
    createInfo.instance = instance;
    createInfo.device = device;
    createInfo.physicalDevice = physicalDevice;
    createInfo.pVulkanFunctions = &vmaFunctions;
    createInfo.vulkanApiVersion = API_VERSION;
    createInfo.flags = flags;

    VK_CHECK(vmaCreateAllocator(&createInfo, &allocator));
}

void VulkanGraphics::createPhyiscalDevice(VkInstance instance, VkSurfaceKHR surface, uint32_t &queueFamilyIndex)
{
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
        if (features12.runtimeDescriptorArray && features12.shaderSampledImageArrayNonUniformIndexing && features12.descriptorBindingStorageBufferUpdateAfterBind) {
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
}

void VulkanGraphics::createDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice physical, uint32_t queueFamilyIndex)
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
            Logger::println(LOG_ERROR, "Device extension is not supported.\n");
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
    features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;

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

    VK_CHECK(vkCreateDevice(physical, &deviceInfo, nullptr, &device));

    // graphics/present queue
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    assert(queue);
}

void VulkanGraphics::createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex, GLFWwindow *window, VkExtent2D &swapchainExtent)
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

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));

    // get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    // create swapchain image views from images
    swapchainImageViews.resize(swapchainImages.size());
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
}

VkCommandPool VulkanGraphics::createCommandPool(VkDevice device, uint32_t queueFamilyIndex)
{
    VkCommandPool commandPool;
    VkCommandPoolCreateInfo commandPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndex;

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

    return commandPool;
}

std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> VulkanGraphics::createCommandBuffers(VkDevice device, VkCommandPool commandPool)
{
    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers;
    VkCommandBufferAllocateInfo bufferAllocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    bufferAllocInfo.commandPool = commandPool;
    bufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferAllocInfo.commandBufferCount = commandBuffers.size();

    VK_CHECK(vkAllocateCommandBuffers(device, &bufferAllocInfo, commandBuffers.data()));

    return commandBuffers;
}

void VulkanGraphics::createSyncPrimitives()
{
    for (unsigned int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        acquireSemaphores[i] = createSemaphore(device);
        submitSemaphores[i] = createSemaphore(device);
        finishRenderFences[i] = createFence(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }
}

void VulkanGraphics::recreateSwapchain()
{
    resizeRequested = false;
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

    destroyImage(renderImage);
    destroyImage(depthImage);

    for (unsigned int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, acquireSemaphores[i], nullptr);
        vkDestroySemaphore(device, submitSemaphores[i], nullptr);
        vkDestroyFence(device, finishRenderFences[i], nullptr);
    }
    createSyncPrimitives();

    createSwapchain(device, physicalDevice, surface, queueFamilyIndex, pWindow, swapchainExtent);

    createImage(renderImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, multisampleCount);
    createImage(depthImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, multisampleCount);
};

VkCommandBuffer VulkanGraphics::beginCommandBuffer()
{
    VK_CHECK(vkWaitForFences(device, 1, &finishRenderFences[currentFrame], VK_TRUE, ~0ull));
    VK_CHECK(vkResetFences(device, 1, &finishRenderFences[currentFrame]));

    VkResult result = vkAcquireNextImageKHR(device, swapchain, ~0ull, acquireSemaphores[currentFrame], nullptr, &imageIndex);
    if (resizeRequested || result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        beginCommandBuffer(); // is this right?
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        Logger::println(LOG_ERROR, "Failed to acquire swapchain image.");
        exit(EXIT_FAILURE);
    }

    VkCommandBuffer cmd = commandBuffers[currentFrame];
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Command buffer begin
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return cmd;
}

void VulkanGraphics::endCommandBuffer(VkCommandBuffer cmd)
{
    // Command buffer end
    VK_CHECK(vkEndCommandBuffer(cmd));
}

void VulkanGraphics::submitCommandBuffer(VkCommandBuffer cmd)
{
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

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (resizeRequested || result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }

    currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;
}

void VulkanGraphics::createBuffer(Buffer &buffer, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage)
{
    if (size <= 0) {
        Logger::println(LOG_WARNING, "Cannot create buffer - size is 0.");
        return;
    }

    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memUsage;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocInfo.priority = 1.0;

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, &buffer.info));

    if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo deviceAddressInfo = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
        deviceAddressInfo.buffer = buffer.buffer;
        buffer.address = vkGetBufferDeviceAddress(device, &deviceAddressInfo);
    }

    buffer.size = size;
}

void VulkanGraphics::destroyBuffer(Buffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

void VulkanGraphics::createImage(Image &image, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkSampleCountFlagBits samples, VkImageViewType type, VkFilter filter, VkSamplerAddressMode samplerMode)
{
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = samples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (type == VK_IMAGE_VIEW_TYPE_CUBE) {
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.arrayLayers = 6;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VK_CHECK(vmaCreateImage(allocator, &imageInfo, &allocInfo, &image.handle, &image.allocation, &image.info));

    VkImageViewCreateInfo imageViewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewInfo.image = image.handle;
    imageViewInfo.viewType = type;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.format = format;
    imageViewInfo.subresourceRange = {aspect, 0, 1, 0, 1};

    if (type == VK_IMAGE_VIEW_TYPE_CUBE)
        imageViewInfo.subresourceRange.layerCount = 6;

    VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &image.view));

    image.sampler = createSampler(filter, filter, samplerMode);
}

void VulkanGraphics::destroyImage(Image &image)
{
    vmaDestroyImage(allocator, image.handle, image.allocation);
    vkDestroyImageView(device, image.view, nullptr);
    vkDestroySampler(device, image.sampler, nullptr);
}

void VulkanGraphics::destroyTexture(Texture &texture)
{
    destroyImage(texture.image);
}

void VulkanGraphics::uploadBuffer(Buffer &buffer, void *data, VkDeviceSize size)
{
    if (size <= 0) {
        Logger::println(LOG_WARNING, "Cannot upload buffer - size is 0.");
        return;
    }

    Buffer staging;
    createBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(staging.info.pMappedData, data, size);
    VK_CHECK(vmaFlushAllocation(allocator, staging.allocation, 0, VK_WHOLE_SIZE));

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

    VkFence fence = createFence(device, 0);
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, ~0L));
    vkDestroyFence(device, fence, nullptr);

    destroyBuffer(staging);
}

VkSampler VulkanGraphics::createSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerAddressMode samplerMode)
{
    // TODO: add anisotropy feature
    VkSamplerCreateInfo createInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    createInfo.magFilter = minFilter;
    createInfo.minFilter = magFilter;
    createInfo.addressModeU = samplerMode;
    createInfo.addressModeV = samplerMode;
    createInfo.addressModeW = samplerMode;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.compareOp = VK_COMPARE_OP_NEVER;
    createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(device, &createInfo, nullptr, &sampler));

    return sampler;
}

void VulkanGraphics::createTexture(Texture &texture, const TextureInfo &info, VkFormat format)
{
    uint32_t size = info.width * info.height * info.channels;

    createImage(texture.image, info.width, info.height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    Buffer staging;
    createBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(staging.info.pMappedData, info.pixels, size);

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
    vkutils::insertImageBarrier(
        copyCmd,
        texture.image.handle,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // copy
    VkBufferImageCopy copyRegion = {};
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageExtent = {static_cast<uint32_t>(info.width), static_cast<uint32_t>(info.height), 1};

    vkCmdCopyBufferToImage(copyCmd, staging.buffer, texture.image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // transition image to fragment shader
    vkutils::insertImageBarrier(
        copyCmd,
        texture.image.handle,
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

    VkFence fence = createFence(device, 0);
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, ~0L));
    vkDestroyFence(device, fence, nullptr);

    destroyBuffer(staging);
}

void VulkanGraphics::createTextureCubemap(Texture &texture, std::filesystem::path dir, VkFormat format)
{
    std::array<std::string, 6> paths = {"right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg"};

    std::array<TextureInfo, 6> infos;
    for (uint32_t face = 0; face < 6; face++) {
        auto path = dir / paths[face];

        infos[face].load(path);
    }

    // XXX: image sizes should be the same.

    uint32_t size = infos[0].width * infos[0].height * STBI_rgb_alpha * 6;
    uint32_t layerSize = size / 6;

    createImage(texture.image, infos[0].width, infos[0].height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, multisampleCount, VK_IMAGE_VIEW_TYPE_CUBE, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    Buffer staging{};
    createBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    for (uint32_t face = 0; face < 6; face++) {
        memcpy(static_cast<unsigned char*>(staging.info.pMappedData) + (layerSize * face), infos[face].pixels, layerSize);
    }

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
    vkutils::insertImageBarrier(
            copyCmd,
            texture.image.handle,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6});

    // copy
    std::vector<VkBufferImageCopy> copyRegions;
    for (uint32_t face = 0; face < 6; face++) {
        VkBufferImageCopy copyRegion = {};
        copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, face, 1};
        copyRegion.imageExtent = {static_cast<uint32_t>(infos[face].width), static_cast<uint32_t>(infos[face].height), 1};
        copyRegion.bufferOffset = face * layerSize;
        copyRegions.push_back(copyRegion);
    }

    vkCmdCopyBufferToImage(copyCmd, staging.buffer, texture.image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copyRegions.size(), copyRegions.data());

    // transition image to fragment shader
    vkutils::insertImageBarrier(
            copyCmd,
            texture.image.handle,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6});

    VK_CHECK(vkEndCommandBuffer(copyCmd));

    // submit
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &copyCmd;

    VkFence fence = createFence(device, 0);
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, ~0L));
    vkDestroyFence(device, fence, nullptr);

    destroyBuffer(staging);
}

void VulkanGraphics::initImGui()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Create descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_IMGUI_TEXTURES},
    };
    imGuiDesctiptorPool = vkutils::createDescriptorPool(device, poolSizes);

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

void VulkanGraphics::requestResize()
{
    resizeRequested = true;
}

VkSemaphore VulkanGraphics::createSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkSemaphore semaphore;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));

    return semaphore;
}

VkFence VulkanGraphics::createFence(VkDevice device, VkFenceCreateFlags flags)
{
    VkFenceCreateInfo createInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    createInfo.flags = flags;

    VkFence fence;
    VK_CHECK(vkCreateFence(device, &createInfo, nullptr, &fence));

    return fence;
}
