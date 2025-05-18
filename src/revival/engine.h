#pragma once

#include <GLFW/glfw3.h>
#include <revival/renderer.h>
#include <revival/camera.h>
#include <revival/physics/physics.h>

class Engine
{
public:
    void init(const char *name, int width, int height, bool isFullscreen = true);
    void shutdown();
    void run();
private:
    void handleInput(double deltaTime);
    void update(double deltaTime);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow *window;
    Renderer renderer;
    Physics physics;

    const char *windowName;
    int windowWidth;
    int windowHeight;

    Camera camera;

    bool running = false;
    bool isFullscreen = false;
};
