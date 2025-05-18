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

void VulkanGraphics::init(GLFWwindow *window)
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

    allocator = createAllocator(instance, device, physicalDevice, 0);

    // graphics/present queue
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    assert(queue);

    // create swapchain
    swapchain = createSwapchain(device, physicalDevice, surface, queueFamilyIndex, window, swapchainExtent);

    // get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    swapchainImageViews = createSwapchainImageViews(device, swapchainImages);

    // command pool
    commandPool = createCommandPool(device, queueFamilyIndex);
    commandBuffers = createCommandBuffers(device, commandPool);

    // synchronization primitives
    for (unsigned int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        acquireSemaphores[i] = vkutils::createSemaphore(device);
        submitSemaphores[i] = vkutils::createSemaphore(device);
        finishRenderFences[i] = vkutils::createFence(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    // depth image
    createImage(depthImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

    initImGui();
}

void VulkanGraphics::shutdown()
{
    vkDeviceWaitIdle(device);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, imGuiDesctiptorPool, nullptr);

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

#ifndef NDEBUG
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

VkInstance VulkanGraphics::createInstance()
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

VkSurfaceKHR VulkanGraphics::createSurface(VkInstance instance, GLFWwindow *window)
{
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));

    return surface;
}

VkDebugUtilsMessengerEXT VulkanGraphics::createDebugMessenger(VkInstance instance)
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

VmaAllocator VulkanGraphics::createAllocator(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, VmaAllocatorCreateFlags flags)
{
    VmaAllocator allocator;
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

    return allocator;
}

VkPhysicalDevice VulkanGraphics::createPhyiscalDevice(VkInstance instance, VkSurfaceKHR surface, uint32_t &queueFamilyIndex)
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

VkDevice VulkanGraphics::createDevice(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice physical, uint32_t queueFamilyIndex)
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

VkSwapchainKHR VulkanGraphics::createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t queueFamilyIndex, GLFWwindow *window, VkExtent2D &swapchainExtent)
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

std::vector<VkImageView> VulkanGraphics::createSwapchainImageViews(VkDevice device, std::vector<VkImage> &swapchainImages)
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

    destroyImage(depthImage);

    for (unsigned int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, acquireSemaphores[i], nullptr);
        vkDestroySemaphore(device, submitSemaphores[i], nullptr);
        vkDestroyFence(device, finishRenderFences[i], nullptr);
        acquireSemaphores[i] = vkutils::createSemaphore(device);
        submitSemaphores[i] = vkutils::createSemaphore(device);
        finishRenderFences[i] = vkutils::createFence(device, VK_FENCE_CREATE_SIGNALED_BIT);
    }

    swapchain = createSwapchain(device, physicalDevice, surface, queueFamilyIndex, pWindow, swapchainExtent);

    // get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    swapchainImageViews = createSwapchainImageViews(device, swapchainImages);

    createImage(depthImage, swapchainExtent.width, swapchainExtent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);
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
        printf("Failed to acquire swapchain image.\n");
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

void VulkanGraphics::beginFrame(VkCommandBuffer cmd)
{
    vkutils::insertImageBarrier(cmd, swapchainImages[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    VkRenderingAttachmentInfo colorAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachmentInfo.clearValue = {{{0.0, 0.0, 0.0, 1.0}}};
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.imageView = swapchainImageViews[imageIndex];
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.renderArea.extent = swapchainExtent;
    renderingInfo.renderArea.offset = {0};
    renderingInfo.layerCount = 1;

    // Rendering begin
    vkCmdBeginRendering(cmd, &renderingInfo);

    // Setup dynamic state
    vkutils::setViewport(cmd, 0.0f, 0.0f, swapchainExtent.width, swapchainExtent.height);
    vkutils::setScissor(cmd, swapchainExtent);
}

void VulkanGraphics::beginFrame(VkCommandBuffer cmd, Image &depthImage)
{
    vkutils::insertImageBarrier(cmd, swapchainImages[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    vkutils::insertImageBarrier(cmd, depthImage.handle, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});

    VkRenderingAttachmentInfo colorAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachmentInfo.clearValue = {{{0.0, 0.0, 0.0, 1.0}}};
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.imageView = swapchainImageViews[imageIndex];
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachmentInfo = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttachmentInfo.clearValue.depthStencil = {0.0, 0};
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.imageView = depthImage.view;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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
    vkutils::setViewport(cmd, 0.0f, 0.0f, swapchainExtent.width, swapchainExtent.height);
    vkutils::setScissor(cmd, swapchainExtent);
}

void VulkanGraphics::beginFrame(VkCommandBuffer cmd, std::vector<std::pair<VkRenderingAttachmentInfo, Image>> &attachments, VkExtent2D extent)
{
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    std::vector<VkRenderingAttachmentInfo> depthAttachments;

    // XXX: i dont know if this bariers are rightly placed
    for (auto &attachment : attachments) {
        if (attachment.first.imageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            colorAttachments.push_back(attachment.first);

            vkutils::insertImageBarrier(
                cmd, attachment.second.handle, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
        } else if (attachment.first.imageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
            depthAttachments.push_back(attachment.first);

            vkutils::insertImageBarrier(
                cmd, attachment.second.handle, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});
        }
    }

    VkRenderingInfo renderingInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.colorAttachmentCount = colorAttachments.size();
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = depthAttachments.data();
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0};
    renderingInfo.layerCount = 1;

    // Rendering begin
    vkCmdBeginRendering(cmd, &renderingInfo);

    // Setup dynamic state
    vkutils::setViewport(cmd, 0.0f, 0.0f, extent.width, extent.height);
    vkutils::setScissor(cmd, extent);
}

void VulkanGraphics::endFrame(VkCommandBuffer cmd, bool insertBarrier)
{
    vkCmdEndRendering(cmd);

    if (insertBarrier) {
        // transition image for presentation
        vkutils::insertImageBarrier(
            cmd, swapchainImages[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    }
}

void VulkanGraphics::createBuffer(Buffer &buffer, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage)
{
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

void VulkanGraphics::createImage(Image &image, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageViewType type, VkImageAspectFlags aspect)
{
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;

    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    vmaCreateImage(allocator, &imageInfo, &allocInfo, &image.handle, &image.allocation, &image.info);

    VkImageViewCreateInfo imageViewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    imageViewInfo.image = image.handle;
    imageViewInfo.viewType = type;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.format = format;
    imageViewInfo.subresourceRange = {aspect, 0, 1, 0, 1};

    vkCreateImageView(device, &imageViewInfo, nullptr, &image.view);

    // TODO: make it configurable
    image.sampler = createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR);
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

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, nullptr));
    VK_CHECK(vkQueueWaitIdle(queue));

    destroyBuffer(staging);
}

VkSampler VulkanGraphics::createSampler(VkFilter minFilter, VkFilter magFilter)
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

void VulkanGraphics::loadTextureInfo(TextureInfo &textureInfo, const char *file)
{
    int width, height, channels;
    unsigned char *data = stbi_load(file, &width, &height, &channels, STBI_rgb_alpha);
    if (!data) {
        printf("Failed to load texture data '%s'\n", file);
        return;
    }

    textureInfo.width = width;
    textureInfo.height = height;
    textureInfo.channels = STBI_rgb_alpha;
    textureInfo.pixels = data;
    textureInfo.path = file;
    textureInfo.loaded = true;
}

void VulkanGraphics::createTexture(Texture &texture, TextureInfo &info, VkFormat format)
{
    uint32_t size = info.width * info.height * info.channels;

    createImage(texture.image, info.width, info.height, format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT);

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

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, nullptr));
    VK_CHECK(vkQueueWaitIdle(queue));

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
