#pragma once

#include <revival/vulkan/graphics.h>
#include <revival/camera.h>
#include <revival/types.h>
#include <revival/scene_manager.h>
#include <revival/passes/shadow_pass.h>
#include <revival/passes/shadow_debug_pass.h>
#include <revival/passes/scene_pass.h>
#include <revival/passes/skybox_pass.h>
#include <revival/passes/billboard_pass.h>
#include <revival/globals.h>

class Physics;

class Renderer
{
public:
    bool init(GLFWwindow *pWindow, Camera *pCamera, SceneManager *pSceneManager, Globals *pGlobals);
    void shutdown();

    void render(std::vector<GameObject> &gameObjects);

    VulkanGraphics &getGraphics() { return graphics; };
private:
    void renderScene(VkCommandBuffer cmd, Scene &scene);
    void renderImgui(VkCommandBuffer cmd);
    void updateDynamicBuffers();
    void createResources();

    uint32_t addTexture(std::string filename);
    uint32_t getTextureIndexByFilename(std::string filename);
    Texture &getTextureByIndex(uint32_t index);

    GLFWwindow *window;
    VulkanGraphics graphics;
    Camera *camera;
    SceneManager *sceneManager;
    Globals *globals;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    std::vector<Texture> textures;
    std::unordered_map<std::string, uint32_t> textureMap; // index into textures vector
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
