#include <revival/renderer.h>
#include <revival/vulkan/utils.h>
#include <revival/scene_manager.h>
#include <revival/vulkan/descriptor_writer.h>
#include <revival/vulkan/pipeline_builder.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <revival/physics/physics.h>

bool Renderer::init(Camera *pCamera, GLFWwindow *pWindow, SceneManager *pSceneManager)
{
    if (!pCamera || !pWindow || !pSceneManager) return false;

    window = pWindow;
    camera = pCamera;
    sceneManager = pSceneManager;

    graphics.init(window);

    createResources();

    shadowPass.init(graphics, textures, vertexBuffer);
    shadowDebugPass.init(graphics, vertexBuffer, shadowPass.getShadowMap());
    scenePass.init(graphics, textures, vertexBuffer, uboBuffer, materialsBuffer, lightsBuffer);
    skyboxPass.init(graphics, skybox);

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

    for (auto &texture : textures) {
        graphics.destroyTexture(texture);
    }

    graphics.destroyTexture(skybox);

    // Passes
    shadowPass.shutdown(device);
    shadowDebugPass.shutdown(device);
    scenePass.shutdown(device);
    skyboxPass.shutdown(graphics, device);

    graphics.shutdown();
}

void Renderer::render(std::vector<GameObject> &gameObjects)
{
    updateDynamicBuffers();

    VkCommandBuffer cmd = graphics.beginCommandBuffer();

    // XXX: right now every pass should specify load and store ops appropriately inside a class, based on other passes. maybe it would be better to give it as a parameter?

    //
    // Skybox Pass
    //
    {
        vkutils::beginDebugLabel(cmd, "Skybox", {0.3, 0.6, 0.3, 1.0});
        skyboxPass.render(graphics, cmd, vertexBuffer.buffer, indexBuffer.buffer, *camera, sceneManager->getSceneByName("cube"));
        vkutils::endDebugLabel(cmd);
    }

    //
    // Shadow Pass
    //
    {
        vkutils::beginDebugLabel(cmd, "Shadow", {0.3, 0.3, 0.3, 0.5});
        shadowPass.beginFrame(graphics, cmd, indexBuffer.buffer);

        mat4 lightMVP = sceneManager->getLightByIndex(0).mvp;

        for (auto &object : gameObjects) {
            shadowPass.render(cmd, object, lightMVP);
        }

        // shadowPass.render(cmd, sceneManager->getSceneByName("sponza"), lightMVP);

        shadowPass.endFrame(graphics, cmd);
        vkutils::endDebugLabel(cmd);
    }

    //
    // Scene Pass
    //
    {
        vkutils::beginDebugLabel(cmd, "Scenes");
        scenePass.beginFrame(graphics, cmd, indexBuffer.buffer);

        for (auto &object : gameObjects) {
            scenePass.render(cmd, object);
        }

        // scenePass.render(cmd, sceneManager->getSceneByName("sponza"));

        scenePass.endFrame(graphics, cmd);
        vkutils::endDebugLabel(cmd);
    }

    //
    // Shadow Debug Pass (Fullscreen quad)
    //
    if (debugLightDepth) {
        vkutils::beginDebugLabel(cmd, "Shadow debug");
        shadowDebugPass.render(graphics, cmd);
        vkutils::endDebugLabel(cmd);
    }

    // Imgui Pass
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

    // load textures from paths
    auto &texturePaths = sceneManager->getTexturePaths();
    textures.resize(texturePaths.size());
    for (size_t i = 0; i < textures.size(); i++) {
        printf("Loading texture: %s\n", texturePaths[i].c_str());
        TextureInfo info;
        graphics.loadTextureInfo(info, texturePaths[i].c_str());
        graphics.createTexture(textures[i], info, VK_FORMAT_R8G8B8A8_SRGB);
    }

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

        // TODO: make shadow map per light
        light.shadowMapIndex = shadowPass.getShadowMapIndex();
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
