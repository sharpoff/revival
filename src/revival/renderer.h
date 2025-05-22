#pragma once

#include <revival/vulkan/graphics.h>
#include <revival/camera.h>
#include <revival/types.h>
#include <revival/scene_manager.h>
#include <revival/game_manager.h>
#include <revival/globals.h>

#include <revival/passes/shadow_pass.h>
#include <revival/passes/shadow_debug_pass.h>
#include <revival/passes/scene_pass.h>
#include <revival/passes/skybox_pass.h>
#include <revival/passes/billboard_pass.h>

class Physics;

class Renderer
{
public:
    bool init(GLFWwindow *pWindow, Camera *pCamera, SceneManager *pSceneManager, GameManager *pGameManager, Globals *pGlobals);
    void shutdown();

    void render();

    VulkanGraphics &getGraphics() { return graphics; };
private:
    void renderScene(VkCommandBuffer cmd, Scene &scene);
    void renderImgui(VkCommandBuffer cmd);
    void updateDynamicBuffers();
    void createResources();

    GLFWwindow *window;
    VulkanGraphics graphics;
    Camera *camera;
    SceneManager *sceneManager;
    GameManager *gameManager;
    Globals *globals;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    Texture skybox;

    bool debugLightDepth = false;

    ShadowPass shadowPass;
    ShadowDebugPass shadowDebugPass;
    ScenePass scenePass;
    SkyboxPass skyboxPass;
    BillboardPass billboardPass;

    struct GlobalUBO
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };
};
