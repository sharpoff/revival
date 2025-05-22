#pragma once

#include <GLFW/glfw3.h>
#include <revival/renderer.h>
#include <revival/camera.h>
#include <revival/physics/physics.h>

#include <revival/scene_manager.h>
#include <revival/game_manager.h>
#include <revival/audio_manager.h>
#include <revival/globals.h>

class Engine
{
public:
    bool init(const char *name, int width, int height, bool isFullscreen = true);
    void shutdown();
    void run();
private:
    void update(double deltaTime);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow *window;
    Renderer renderer;
    Physics physics;

    SceneManager sceneManager;
    GameManager gameManager;
    AudioManager audioManager;

    const char *windowName;
    int windowWidth;
    int windowHeight;

    Camera camera;

    Globals globals;

    bool running = false;
    bool isFullscreen = false;
};
