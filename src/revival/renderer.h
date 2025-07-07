#pragma once

#include <revival/vulkan/graphics.h>
#include <revival/camera.h>
#include <revival/types.h>
#include <revival/asset_manager.h>

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
    void loadShaders(std::filesystem::path dir);
    void createDescriptors();
    void createPipelines();

    GLFWwindow *window;
    VulkanGraphics graphics;
    Camera &camera;
    AssetManager &sceneManager;

    std::unordered_map<std::string, VkShaderModule> shaders;

    VkPipelineLayout pipelineLayout;
    struct {
        VkPipeline scene;
        VkPipeline shadow;
    } pipelines;

    VkDescriptorPool pool;
    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    Texture skybox;

    const uint32_t shadowMapSize = 2048;
    const float depthBiasConstant = 1.25f;
    const float depthBiasSlope = 1.75f;

    std::vector<Image> shadowMaps;

    struct PushConstant
    {
        alignas(16) mat4 model;
        int materialIndex;
    };

    struct GlobalUBO
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };
};
