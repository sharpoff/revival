#include <revival/application.h>
#include <revival/util.h>
#include <stdio.h>
#include <time.h>

Application::Application(std::string name, int width, int height, bool enableFullScreen)
    : renderer(camera, sceneManager), name(name), width(width), height(height)
{
    // change current directory from build/ to a project root
    std::filesystem::current_path(util::getProjectRoot());

    if (!glfwInit()) {
        Logger::println(LOG_ERROR, "Failed to initialize glfw.");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // create window, fullscreen or not
    if (enableFullScreen) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

        window = glfwCreateWindow(mode->width, mode->height, name.c_str(), glfwGetPrimaryMonitor(), NULL);
        width = mode->width;
        height = mode->height;
        fullscreen = true;
    } else {
        window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
        fullscreen = false;
    }
    if (!window) {
        Logger::println(LOG_ERROR, "Failed to create glfw window.");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // set callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwMakeContextCurrent(window);

    // camera settings
    camera.setProjection(math::perspective(glm::radians(60.0f), float(width) / height, 0.1f, 300.0f));
    camera.setPosition(vec3(0.0, 2.0, 3.0));

    // loading lights/objects/scenes/models
    sceneManager.addLight({mat4(1.0), vec3(7.0, 29.0, 0.0), vec3(1.0)});

    sceneManager.loadScene("sponza", "models/sponza/Sponza.gltf");
    sceneManager.loadScene("cube", "models/cube.gltf");

    renderer.init(window);
}

Application::~Application()
{
    renderer.shutdown();

    glfwTerminate();
}

void Application::run()
{
    double previousTime = glfwGetTime();

    running = true;
    while (!glfwWindowShouldClose(window) && running)
    {
        double deltaTime = glfwGetTime() - previousTime;
        previousTime = glfwGetTime();

        update(deltaTime);

        renderer.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Application::update(double deltaTime)
{
    camera.update(window, deltaTime);
}

void Application::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        app->running = false;
    }

    // fullscreen mode
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (app->fullscreen)
            glfwSetWindowMonitor(window, nullptr, 0, 0, app->width, app->height, mode->refreshRate);
        else
            glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);

        app->renderer.getGraphics().requestResize();

        app->fullscreen = !app->fullscreen;
    }

}

void Application::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
    app->renderer.getGraphics().requestResize();
}
