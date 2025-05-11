#include <revival/engine.h>
#include <stdio.h>
#include <time.h>

Engine::Engine(const char *name, int width, int height, bool isFullscreen)
    : width(width), height(height)
{
    srand(time(0));
    if (!glfwInit()) {
        printf("Failed to initialize glfw.\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (isFullscreen) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        window = glfwCreateWindow(mode->width, mode->height, name, glfwGetPrimaryMonitor(), NULL);
        this->width = mode->width;
        this->height = mode->height;
        isFullscreen = true;
    } else {
        window = glfwCreateWindow(this->width, this->height, name, NULL, NULL);
        isFullscreen = false;
    }
    if (!window) {
        printf("Failed to create glfw window.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwMakeContextCurrent(window);

    Renderer::initialize(&camera, window);

    // some settings
    camera.setPerspective(60.0f, float(width) / height, 0.1, 100.0f);
    camera.setPosition(vec3(0.0, 2.0, 3.0));
}

Engine::~Engine()
{
    Renderer::shutdown();
    glfwTerminate();
}

void Engine::run()
{
    double previousTime = glfwGetTime();

    running = true;
    while (!glfwWindowShouldClose(window) && running)
    {
        double deltaTime = glfwGetTime() - previousTime;
        previousTime = glfwGetTime();

        handleInput(deltaTime);
        update(deltaTime);

        Renderer::render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Engine::handleInput(double deltaTime)
{
    // exit application
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        running = false;
    }

    // fullscreen mode
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (fullscreen)
            glfwSetWindowMonitor(window, NULL, 0, 0, width, height, mode->refreshRate);
        else
            glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);

        Renderer::requestResize();

        fullscreen = !fullscreen;
    }

    camera.handleInput(window, deltaTime);
}

void Engine::update(double deltaTime)
{
    camera.update(deltaTime);
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    Renderer::requestResize();
}
