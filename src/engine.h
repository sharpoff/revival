#pragma once

#include <GLFW/glfw3.h>
#include "device.h"
#include "mesh_renderer.h"

class Engine
{
public:
    Engine(const char *name, int width, int height, bool isFullscreen = true);
    ~Engine();
    void run();
private:
    void draw();
    void handleInput(double deltaTime);
    void update(double deltaTime);
    void drawImgui();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow *window;
    int width;
    int height;

    VulkanDevice vkDevice;

    // buffers
    Buffer vertexBuffer;
    Buffer indexBuffer;

    // renderers (pipelines, descriptors and other pipeline specific stuff)
    MeshRenderer meshRenderer;

    // global data
    Camera camera;
    vec2 cursorPrevious;
    std::unordered_map<std::string, Scene> scenes;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<std::filesystem::path> texturePaths;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Light> lights;

    bool isRunning = false;
    bool isFullscreen = false;
};
