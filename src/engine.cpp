#include "engine.h"
#include "scene.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

Engine::Engine(const char *name, int width, int height, bool isFullscreen)
    : width(width), height(height), meshRenderer(materials, lights, camera)
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
    if (!window)
    {
        printf("Failed to create glfw window.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetWindowUserPointer(window, &vkDevice);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwMakeContextCurrent(window);

    vkDevice.create(window);

    lights = {
        {{2.0, 1.0, 4.0}, {0.8, 0.8, 0.8}},
        {{-4.0, 3.0, 4.0}, {0.8, 0.8, 0.8}},
    };

    // load gltf scenes
    loadScene(&scenes["cube"], vertices, indices, texturePaths, materials, lights, "models/cube.gltf");
    loadScene(&scenes["shadow_test"], vertices, indices, texturePaths, materials, lights, "models/shadow_test.gltf");
    loadScene(&scenes["helmet"], vertices, indices, texturePaths, materials, lights, "models/DamagedHelmet/DamagedHelmet.gltf");

    // load textures
    textures.resize(texturePaths.size());
    for (size_t i = 0; i < textures.size(); i++) {
        printf("Loading texture: %s\n", texturePaths[i].c_str());
        vkDevice.createTexture(textures[i], texturePaths[i].c_str(), VK_FORMAT_R8G8B8A8_SRGB);
    }

    // load vertices and indices
    uint32_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    uint32_t indexBufferSize = indices.size() * sizeof(uint32_t);
    vkDevice.createBuffer(vertexBuffer, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkDevice.createBuffer(indexBuffer, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkDevice.uploadBuffer(vertexBuffer, vertices.data(), vertexBufferSize);
    vkDevice.uploadBuffer(indexBuffer,  indices.data(), indexBufferSize);

    // load renderers
    meshRenderer.create(vkDevice, textures);

    camera.position = vec3(0.0f, 1.5f, 3.0f);
    camera.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
    camera.moveSpeed = 1.5f;
    camera.rotationSpeed = glm::radians(10.0f);
}

Engine::~Engine()
{
    vkDeviceWaitIdle(vkDevice.getDevice());

    for (size_t i = 0; i < textures.size(); i++) {
        vkDevice.destroyImage(textures[i].image);
        vkDestroyImageView(vkDevice.getDevice(), textures[i].imageView, nullptr);
        vkDestroySampler(vkDevice.getDevice(), textures[i].sampler, nullptr);
    }

    meshRenderer.destroy(vkDevice);

    vkDevice.destroyBuffer(vertexBuffer);
    vkDevice.destroyBuffer(indexBuffer);

    vkDevice.destroy();
    glfwTerminate();
}

void Engine::run()
{
    double previousTime = glfwGetTime();

    isRunning = true;
    while (!glfwWindowShouldClose(window) && isRunning)
    {
        double deltaTime = glfwGetTime() - previousTime;
        previousTime = glfwGetTime();

        handleInput(deltaTime);
        update(deltaTime);
        draw();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void Engine::draw()
{
    VkCommandBuffer cmd = vkDevice.getCommandBuffer();

    auto drawFrame = [&]() {
        beginDebugLabel(cmd, "Meshes");
        scenes["shadow_test"].modelMatrix = mat4(1.0);
        meshRenderer.draw(scenes["shadow_test"], vkDevice, vertexBuffer, indexBuffer);

        meshRenderer.draw(scenes["helmet"], vkDevice, vertexBuffer, indexBuffer);

        for (size_t i = 0; i < lights.size(); i++) {
            scenes["cube"].modelMatrix = glm::translate(lights[i].position) * glm::scale(vec3(0.2));
            meshRenderer.draw(scenes["cube"], vkDevice, vertexBuffer, indexBuffer);
        }
        endDebugLabel(cmd);

        beginDebugLabel(cmd, "Dear ImGUI", {0.3, 0.3, 0.0, 0.5});
        drawImgui();
        endDebugLabel(cmd);
    };
    vkDevice.draw(drawFrame);
}

void Engine::drawImgui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    {
        ImGui::Begin("Lights");

        for (size_t i = 0; i < lights.size(); i++) {
            ImGui::PushID(i);
            std::string name = std::string("Light" + std::to_string(i));
            ImGui::ColorEdit3((name + " color").c_str(), &lights[i].color[0]);
            ImGui::DragFloat3((name + " position").c_str(), &lights[i].position[0], 1.0f, -100.0f, 100.0f);
            ImGui::PopID();
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Scenes");

        size_t i = 0;
        for (auto &[name, scene] : scenes) {
            ImGui::PushID(i++);
            static vec3 pos = vec3(1.0);
            if (ImGui::DragFloat3((name + " position").c_str(), &pos[0], 1.0f, -100.0f, 100.0f)) {
                scene.modelMatrix = glm::translate(mat4(1.0), pos);
            }
            ImGui::PopID();
        }

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkDevice.getCommandBuffer());
}

void Engine::handleInput(double deltaTime)
{
    // exit application
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        isRunning = false;
    }

    // fullscreen mode
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (isFullscreen) {
            glfwSetWindowMonitor(window, NULL, 0, 0, width, height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);
        }

        vkDevice.requestResize();

        isFullscreen = !isFullscreen;
    }

    // keyboard camera movement
    float speedBoost = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.5 : 1;
    vec2 move = vec2(glfwGetKey(window, GLFW_KEY_D), glfwGetKey(window, GLFW_KEY_W)) - vec2(glfwGetKey(window, GLFW_KEY_A), glfwGetKey(window, GLFW_KEY_S));
    camera.position += float(move.x * deltaTime * camera.moveSpeed * speedBoost) * (camera.rotation * vec3(1, 0, 0));
    camera.position += float(move.y * deltaTime * camera.moveSpeed * speedBoost) * (camera.rotation * vec3(0, 0, -1));
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        camera.position += float(deltaTime * camera.moveSpeed * speedBoost) * (camera.rotation * vec3(0, 1, 0));
    }
    else if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        camera.position -= float(deltaTime * camera.moveSpeed * speedBoost) * (camera.rotation * vec3(0, 1, 0));
    }

    // mouse camera movement
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (!ImGui::GetIO().WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        vec2 mouseMotion = vec2(xpos - cursorPrevious.x, ypos - cursorPrevious.y);

        camera.rotation = glm::rotate(glm::quat(1, 0, 0, 0), float(-mouseMotion.x * camera.rotationSpeed * deltaTime), vec3(0, 1, 0)) * camera.rotation;
        camera.rotation = glm::rotate(glm::quat(1, 0, 0, 0), float(-mouseMotion.y * camera.rotationSpeed * deltaTime), camera.rotation * vec3(1, 0, 0)) * camera.rotation;
    }
    cursorPrevious = vec2(xpos, ypos);
}

void Engine::update(double deltaTime)
{
    // update lights
    for (auto &light : lights) {
        float r = 0.02;
        light.position += vec3(r * cos(glfwGetTime()), 0, r * sin(glfwGetTime()));
    }
}

void Engine::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto device = reinterpret_cast<VulkanDevice*>(glfwGetWindowUserPointer(window));
    device->requestResize();
}
