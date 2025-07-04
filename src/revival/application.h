#pragma once

#include <GLFW/glfw3.h>
#include <revival/camera.h>
#include <revival/renderer.h>

class Application
{
public:
    Application(std::string name, int width, int height, bool enableFullScreen);
    ~Application();

    void run();
private:
    void update(double deltaTime);
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

    GLFWwindow *window;
    Renderer renderer;

    AssetManager sceneManager;

    Camera camera;

    std::string name;
    int width;
    int height;

    bool running = false;
    bool fullscreen = false;
};
