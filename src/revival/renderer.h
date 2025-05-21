#pragma once

#include <revival/vulkan/graphics.h>
#include <revival/camera.h>
#include <revival/types.h>
#include <revival/scene_manager.h>
#include <revival/shadow_pass.h>
#include <revival/shadow_debug_pass.h>
#include <revival/scene_pass.h>
#include <revival/skybox_pass.h>

class Physics;

class Renderer
{
public:
    bool init(Camera *pCamera, GLFWwindow *pWindow, SceneManager *pSceneManager);
    void shutdown();

    void render(std::vector<GameObject> &gameObjects);

    VulkanGraphics &getGraphics() { return graphics; };
private:
    void renderScene(VkCommandBuffer cmd, Scene &scene);

    void renderImgui(VkCommandBuffer cmd);

    void createResources();

    void updateDynamicBuffers();

    GLFWwindow *window;
    VulkanGraphics graphics;
    Camera *camera;
    SceneManager *sceneManager;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    std::vector<Texture> textures;
    Texture skybox;

    bool debugLightDepth = false;

    ShadowPass shadowPass;
    ShadowDebugPass shadowDebugPass;
    ScenePass scenePass;
    SkyboxPass skyboxPass;

    struct GlobalUBO
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };
};
