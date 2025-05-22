#include <revival/renderer.h>
#include <revival/vulkan/utils.h>
#include <revival/scene_manager.h>
#include <revival/vulkan/descriptor_writer.h>
#include <revival/vulkan/pipeline_builder.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <revival/physics/physics.h>

bool Renderer::init(GLFWwindow *pWindow, Camera *pCamera, SceneManager *pSceneManager, Globals *pGlobals)
{
    if (!pCamera || !pWindow || !pSceneManager || !pGlobals) return false;

    window = pWindow;
    camera = pCamera;
    sceneManager = pSceneManager;
    globals = pGlobals;

    graphics.init(window);

    createResources();

    shadowPass.init(graphics, textures, sceneManager->getLights(), vertexBuffer);
    shadowDebugPass.init(graphics, vertexBuffer);
    scenePass.init(graphics, textures, vertexBuffer, uboBuffer, materialsBuffer, lightsBuffer);
    skyboxPass.init(graphics, skybox);
    billboardPass.init(graphics, textures);

    return true;
}

void Renderer::shutdown()
{
    vkDeviceWaitIdle(graphics.getDevice());
    VkDevice device = graphics.getDevice();

    // Resources
    graphics.destroyBuffer(uboBuffer);
    graphics.destroyBuffer(materialsBuffer);
    graphics.destroyBuffer(lightsBuffer);

    graphics.destroyBuffer(vertexBuffer);
    graphics.destroyBuffer(indexBuffer);

    for (auto &texture : textures)
        graphics.destroyTexture(texture);

    graphics.destroyTexture(skybox);

    // Passes
    shadowPass.shutdown(device);
    shadowDebugPass.shutdown(device);
    scenePass.shutdown(device);
    skyboxPass.shutdown(graphics, device);
    billboardPass.shutdown(graphics, device);

    graphics.shutdown();
}

void Renderer::render(std::vector<GameObject> &gameObjects)
{
    updateDynamicBuffers();

    VkCommandBuffer cmd = graphics.beginCommandBuffer();
    uint32_t scenesCount = sceneManager->getScenes().size();

    // XXX: right now every pass should specify load and store ops appropriately inside the classes, based on other passes.
    // maybe it would be better to give them as parameters?

    //
    // Skybox Pass
    //
    if (scenesCount > 0)
    {
        vkutils::beginDebugLabel(cmd, "Skybox", {0.3, 0.6, 0.3, 1.0});
        skyboxPass.render(graphics, cmd, vertexBuffer.buffer, indexBuffer.buffer, *camera, sceneManager->getSceneByName("cube"));
        vkutils::endDebugLabel(cmd);
    }

    //
    // Shadow Pass
    //
    if (scenesCount > 0)
    {
        vkutils::beginDebugLabel(cmd, "Shadow", {0.3, 0.3, 0.3, 0.5});
        shadowPass.beginFrame(graphics, cmd, indexBuffer.buffer, 0);

        mat4 lightMVP = sceneManager->getLightByIndex(0).mvp;

        for (auto &object : gameObjects) {
            shadowPass.render(cmd, object, lightMVP);
        }

        shadowPass.endFrame(graphics, cmd, 0);
        vkutils::endDebugLabel(cmd);
    }

    //
    // Scene Pass
    //
    if (scenesCount > 0)
    {
        vkutils::beginDebugLabel(cmd, "Scenes");
        scenePass.beginFrame(graphics, cmd, indexBuffer.buffer);

        for (auto &object : gameObjects) {
            scenePass.render(cmd, object);
        }

        scenePass.endFrame(graphics, cmd);
        vkutils::endDebugLabel(cmd);
    }

    // Billboard Pass
    {
        vkutils::beginDebugLabel(cmd, "Billboards", {0.3, 0.0, 0.0, 0.5});
        billboardPass.beginFrame(graphics, cmd);
        billboardPass.render(cmd, graphics.getDevice(), *camera, sceneManager->getLightByIndex(0).position, vec2(1.0f), getTextureIndexByFilename("textures/cacodemon.png"));
        billboardPass.endFrame(graphics, cmd);
        vkutils::endDebugLabel(cmd);
    }

    //
    // Shadow Debug Pass (Fullscreen quad)
    //
    if (debugLightDepth) {
        vkutils::beginDebugLabel(cmd, "Shadow debug");
        shadowDebugPass.render(graphics, cmd, shadowPass.getShadowMapByLightIndex(0));
        vkutils::endDebugLabel(cmd);
    }

    // Imgui Pass
    if (globals->showImGui)
    {
        vkutils::beginDebugLabel(cmd, "Dear ImGUI", {0.3, 0.3, 0.0, 0.5});
        renderImgui(cmd);
        vkutils::endDebugLabel(cmd);
    }

    graphics.endCommandBuffer(cmd);
    graphics.submitCommandBuffer(cmd);
}

void Renderer::renderImgui(VkCommandBuffer cmd)
{
    Image swapchainImage;
    swapchainImage.view = graphics.getSwapchainImageView();
    swapchainImage.handle = graphics.getSwapchainImage();

    VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
    colorAttachment.imageView = swapchainImage.view;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    std::vector<std::pair<VkRenderingAttachmentInfo, Image>> attachments = {
        std::make_pair(colorAttachment, swapchainImage),
    };

    graphics.beginFrame(cmd, attachments, graphics.getSwapchainExtent());

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    {
        ImGui::Begin("Debug");
        ImGui::Text("Verices: %zu", sceneManager->getVertices().size());
        ImGui::Text("Textures: %zu", textures.size());
        ImGui::Text("Materials: %zu", sceneManager->getMaterials().size());
        ImGui::Text("Scenes: %zu", sceneManager->getScenes().size());
        ImGui::Text("Lights: %zu", sceneManager->getLights().size());

        ImGui::Checkbox("Debug depth", &debugLightDepth);
        ImGui::End();
    }

    {
        std::vector<Light> &lights = sceneManager->getLights();
        ImGui::Begin("Lights");
        for (size_t i = 0; i < lights.size(); i++) {
            ImGui::PushID(i);
            if (ImGui::TreeNode(std::string("Light " + std::to_string(i)).c_str())) {
                ImGui::ColorEdit3("color", &lights[i].color[0]);
                ImGui::DragFloat3("position", &lights[i].position[0], 1.0f, -100.0f, 100.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    graphics.endFrame(cmd);
}

void Renderer::createResources()
{
    VkDevice device = graphics.getDevice();

    // TODO: add proper caching system
    // load textures from paths
    auto &texturePaths = sceneManager->getTexturePaths();
    for (size_t i = 0; i < texturePaths.size(); i++) {
        printf("Loading texture: %s\n", texturePaths[i].c_str());
        addTexture(texturePaths[i]);
    }
    addTexture("textures/cacodemon.png");

    // load skybox texture
    graphics.createTextureCubemap(skybox, "textures/skybox", VK_FORMAT_R8G8B8A8_SRGB);

    // load global vertices and indices
    auto &vertices = sceneManager->getVertices();
    auto &indices = sceneManager->getIndices();

    uint32_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    uint32_t indexBufferSize = indices.size() * sizeof(uint32_t);
    graphics.createBuffer(vertexBuffer, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    graphics.createBuffer(indexBuffer, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    graphics.uploadBuffer(vertexBuffer, vertices.data(), vertexBufferSize);
    graphics.uploadBuffer(indexBuffer,  indices.data(), indexBufferSize);

    // create buffers
    graphics.createBuffer(uboBuffer, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    vkutils::setDebugName(device, (uint64_t)uboBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "globalUBO");

    std::vector<Material> &materials = sceneManager->getMaterials();
    graphics.createBuffer(materialsBuffer, materials.size() * sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkutils::setDebugName(device, (uint64_t)materialsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "materialsBuffer");

    std::vector<Light> &lights = sceneManager->getLights();
    if (lights.size() > 0) {
        graphics.createBuffer(lightsBuffer, lights.size() * sizeof(Light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        vkutils::setDebugName(device, (uint64_t)lightsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "lightsBuffer");
    }
}

void Renderer::updateDynamicBuffers()
{
    std::vector<Material> &materials = sceneManager->getMaterials();
    std::vector<Light> &lights = sceneManager->getLights();

    for (auto &light : lights) {
        mat4 projection = math::perspective(glm::radians(45.0f), 1.0f, 1.0f, 100.0f);
        mat4 view = glm::lookAt(light.position, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4 mvp = projection * view;
        light.mvp = mvp;
    }

    if (lights.size() > 0)
        memcpy(lightsBuffer.info.pMappedData, lights.data(), lightsBuffer.size);

    GlobalUBO ubo = {
        .projection = camera->getProjection(),
        .view = camera->getView(),
        .numLights = static_cast<uint>(lights.size()),
        .cameraPos = camera->getPosition(),
    };
    memcpy(uboBuffer.info.pMappedData, &ubo, sizeof(ubo));

    if (materials.size() > 0)
        memcpy(materialsBuffer.info.pMappedData, materials.data(), materialsBuffer.size);
}

uint32_t Renderer::addTexture(std::string filename)
{
    uint32_t index = textures.size();
    Texture &texture = textures.emplace_back();

    TextureInfo info;
    graphics.loadTextureInfo(info, filename.c_str());
    graphics.createTexture(texture, info, VK_FORMAT_R8G8B8A8_SRGB);
    textureMap[filename] = index;

    return index;
}

uint32_t Renderer::getTextureIndexByFilename(std::string filename)
{
    return textureMap[filename];
}

Texture &Renderer::getTextureByIndex(uint32_t index)
{
    return textures[index];
}
