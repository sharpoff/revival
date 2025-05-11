#pragma once

#include <GLFW/glfw3.h>
#include <revival/renderer.h>
#include <revival/camera.h>

class Engine
{
public:
    Engine(const char *name, int width, int height, bool isFullscreen = true);
    ~Engine();
    void run();
private:
    void handleInput(double deltaTime);
    void update(double deltaTime);

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow *window;
    int width;
    int height;

    Camera camera;

    bool running = false;
    bool fullscreen = false;
};
