#pragma once

#include <revival/vulkan/graphics.h>
#include <revival/camera.h>
#include <revival/types.h>
#include <revival/shadow_pass.h>
#include <revival/shadow_debug_pass.h>
#include <revival/scene_pass.h>

class Renderer
{
public:
    void initialize(Camera *pCamera, GLFWwindow *pWindow);
    void shutdown();

    void render();

    VulkanGraphics &getGraphics() { return graphics; };
private:
    void renderScene(VkCommandBuffer cmd, Scene &scene);

    void renderImgui(VkCommandBuffer cmd);

    void createResources();

    void updateDynamicBuffers();

    GLFWwindow *window;
    VulkanGraphics graphics;
    Camera *camera;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    std::vector<Texture> textures;

    bool debugShadowMap = false;

    ShadowPass shadowPass;
    ShadowDebugPass shadowDebugPass;
    ScenePass scenePass;

    struct GlobalUBO
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };
};
