#include <revival/engine.h>
#include <revival/fs.h>
#include <stdio.h>
#include <time.h>

bool Engine::init(const char *name, int width, int height, bool enableFullScreen)
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

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwMakeContextCurrent(window);

    sceneManager.addLight({mat4(1.0), vec3(18.0, 19.0, 22.0), vec3(1.0)});

    for (int i = 0; i < 10; i++) {
        sceneManager.addBillboard({vec3(-30.0f + 10.0f * i, 10.0f * i, -20.0f), -1, vec2(2.0f)});
    }

    sceneManager.loadScene("shadow_test", "models/shadow_test.gltf");
    sceneManager.loadScene("cube", "models/cube.gltf");
    sceneManager.loadScene("plane", "models/plane.gltf");

    if (!renderer.init(window, &camera, &sceneManager, &gameManager, &globals)) {
        printf("Failed to initialize renderer.\n");
        return false;
    }

    if (!physics.init()) {
        printf("Failed to initialize physics.\n");
        return false;
    }

    if (!audioManager.init()) {
        printf("Failed to initialize audio.\n");
        return false;
    }

    // load game objects
    for (int i = 0; i < 20; i++) {
        gameManager.createGameObject(physics, "cube" + std::to_string(i), &sceneManager.getSceneByName("cube"), Transform(vec3(0.0f, i * 20.0f, 0.0f)), vec3(1.0f), false);
    }

    gameManager.createGameObject(physics, "floor", &sceneManager.getSceneByName("plane"), Transform(vec3(0.0f, -1.0f, 0.0f), quat(1, 0, 0, 0), vec3(100.0f, 1.0f, 100.0f)), vec3(100.0f, 0.1f, 100.0f), true);

    // some settings
    camera.setProjection(math::perspective(glm::radians(60.0f), float(width) / height, 0.1f, 1000.0f));
    camera.setPosition(vec3(0.0, 2.0, 3.0));

    return true;
}

void Engine::shutdown()
{
    audioManager.shutdown();
    physics.shutdown(gameManager.getGameObjects());
    renderer.shutdown();

    glfwTerminate();
}

void Engine::run()
{
    double previousTime = glfwGetTime();

    // TEST audio
    // audioManager.playSound("audio/sunny_pierce.mp3");
    // audioManager.playSound("audio/doom.mp3");

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

void Engine::update(double deltaTime)
{
    // snap billboard position to light position
    // sceneManager.getBillboardByIndex(0).position = sceneManager.getLightByIndex(0).position;

    // MAKE THEM FOLLOW YOU!!!
    auto &billboards = sceneManager.getBillboards();
    for (auto &billboard : billboards) {
        float movespeed = float(deltaTime) * 5.0f;
        vec3 dir = glm::normalize(camera.getPosition() - billboard.position);
        billboard.position += dir * movespeed;
    }

    physics.update(deltaTime, gameManager.getGameObjects());
    camera.update(window, deltaTime);
}

void Engine::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Engine *engine = (Engine*)glfwGetWindowUserPointer(window);

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        engine->running = false;
    }

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        engine->globals.showImGui = !engine->globals.showImGui;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        engine->gameManager.createGameObject(engine->physics, "cube", &engine->sceneManager.getSceneByName("cube"), Transform(vec3(0.0f, 50.0f, 0.0f)), vec3(1.0f), false);
    }

    // fullscreen mode
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (engine->isFullscreen)
            glfwSetWindowMonitor(window, NULL, 0, 0, engine->windowWidth, engine->windowHeight, mode->refreshRate);
        else
            glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);

        engine->renderer.getGraphics().requestResize();

        engine->isFullscreen = !engine->isFullscreen;
    }

}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    Engine *engine = (Engine*)glfwGetWindowUserPointer(window);
    engine->renderer.getGraphics().requestResize();
}
