#pragma once

#include <revival/vulkan/graphics.h>
#include <revival/camera.h>
#include <revival/types.h>
#include <revival/asset_manager.h>

#include <revival/passes/shadow_pass.h>
#include <revival/passes/shadow_debug_pass.h>
#include <revival/passes/scene_pass.h>
#include <revival/passes/skybox_pass.h>
#include <revival/passes/billboard_pass.h>
#include <revival/logger.h>

class Physics;

class Renderer
{
public:
    Renderer(Camera &camera, AssetManager &sceneManager);
    void init(GLFWwindow *window);
    void shutdown();

    void render();

    VulkanGraphics &getGraphics() { return graphics; };
private:
    void renderImgui(VkCommandBuffer cmd);
    void updateDynamicBuffers();
    void createResources();

    GLFWwindow *window;
    VulkanGraphics graphics;
    Camera &camera;
    AssetManager &sceneManager;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    Texture skybox;

    bool debugLightDepth = false;
    float shadowBias = 0.00001;

    ShadowPass shadowPass;
    ShadowDebugPass shadowDebugPass;
    ScenePass scenePass;
    SkyboxPass skyboxPass;
    // BillboardPass billboardPass;

    struct GlobalUBO
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };

    bool initialized = false;
};
