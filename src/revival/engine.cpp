#include <revival/engine.h>
#include <revival/fs.h>
#include <stdio.h>
#include <time.h>

#include <revival/scene_manager.h>
#include <revival/game_manager.h>

void Engine::init(const char *name, int width, int height, bool enableFullScreen)
{
    windowName = name;
    windowWidth = width;
    windowHeight = height;

    // change current directory from build/ to a project root
    std::filesystem::current_path(fs::getProjectRoot());

    srand(time(0));

    if (!glfwInit()) {
        printf("Failed to initialize glfw.\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    if (enableFullScreen) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        window = glfwCreateWindow(mode->width, mode->height, name, glfwGetPrimaryMonitor(), NULL);
        windowWidth = mode->width;
        windowHeight = mode->height;
        isFullscreen = true;
    } else {
        window = glfwCreateWindow(windowWidth, windowHeight, name, NULL, NULL);
        isFullscreen = false;
    }
    if (!window) {
        printf("Failed to create glfw window.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetWindowUserPointer(window, &renderer);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwMakeContextCurrent(window);

    SceneManager::getLights().push_back({mat4(1.0), vec3(18.0, 19.0, 22.0), vec3(1.0)});
    SceneManager::loadScene("shadow_test", "models/shadow_test.gltf");
    // SceneManager::loadScene("cube", "models/cube.gltf");

    renderer.init(&camera, window);
    physics.init();

    // load game objects
    // for (int i = 0; i < 20; i++) {
    //     GameManager::createGameObject(physics, "cube" + std::to_string(i), &SceneManager::getSceneByName("cube"), Transform(vec3(0.0f, i * 20.0f, 0.0f)), vec3(1.0f), false);
    // }
    // GameManager::createGameObject(physics, "floor", nullptr, Transform(vec3(0.0f, -1.0f, 0.0f)), vec3(100.0f, 1.0f, 100.0f), true);

    // some settings
    camera.setPerspective(60.0f, float(width) / height, 0.1, 100.0f);
    camera.setPosition(vec3(0.0, 2.0, 3.0));
}

void Engine::shutdown()
{
    physics.shutdown();
    renderer.shutdown();

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

        renderer.render(&physics);

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
        if (isFullscreen)
            glfwSetWindowMonitor(window, NULL, 0, 0, windowWidth, windowHeight, mode->refreshRate);
        else
            glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);

        renderer.getGraphics().requestResize();

        isFullscreen = !isFullscreen;
    }

    camera.handleInput(window, deltaTime);
}

void Engine::update(double deltaTime)
{
    physics.update(deltaTime);
    camera.update(deltaTime);
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    Renderer *renderer = (Renderer*)glfwGetWindowUserPointer(window);
    renderer->getGraphics().requestResize();
}
