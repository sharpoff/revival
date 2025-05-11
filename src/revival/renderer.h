#pragma once

#include <revival/vulkan/vk_context.h>
#include <revival/camera.h>
#include <revival/types.h>

namespace Renderer
{
    void initialize(Camera *pCamera, GLFWwindow *pWindow);
    void shutdown();

    void render();
    void renderScene(VkCommandBuffer cmd, Scene &scene);

    void renderImgui(VkCommandBuffer cmd);

    void requestResize();
    void resizeResources();

    void createResources();
    void createDescriptors();
    void createPipelines();

    void updateDynamicBuffers();
};
