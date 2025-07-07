#include <revival/renderer.h>
#include <revival/vulkan/utils.h>
#include <revival/vulkan/descriptor_writer.h>
#include <revival/vulkan/pipeline_builder.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

Renderer::Renderer(Camera &camera, AssetManager &sceneManager)
    : camera(camera), sceneManager(sceneManager)
{}

void Renderer::init(GLFWwindow *window)
{
    if (!window) {
        Logger::println(LOG_ERROR, "Failed to initialzie renderer");
        exit(EXIT_FAILURE);
    }

    this->window = window;
    graphics.init(window);

    // imgui style settings
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;

    createResources();
    loadShaders("build/shaders/");

    createDescriptors();
    createPipelines();
}

void Renderer::shutdown()
{
    const VkDevice device = graphics.getDevice();

    vkDeviceWaitIdle(graphics.getDevice());

    for (auto &[_, shader] : shaders)
        vkDestroyShaderModule(device, shader, nullptr);

    graphics.destroyBuffer(uboBuffer);
    graphics.destroyBuffer(materialsBuffer);
    graphics.destroyBuffer(lightsBuffer);

    graphics.destroyBuffer(vertexBuffer);
    graphics.destroyBuffer(indexBuffer);

    for (auto &texture : sceneManager.getTextures())
        graphics.destroyTexture(texture);

    graphics.destroyTexture(skybox);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipelines.scene, nullptr);
    vkDestroyPipeline(device, pipelines.shadow, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);

    graphics.shutdown();
}

void Renderer::render()
{
    updateDynamicBuffers();

    VkCommandBuffer cmd = graphics.beginCommandBuffer();
    uint32_t scenesCount = sceneManager.getScenes().size();

    // //
    // // Shadow pass
    // //
    // if (scenesCount > 0) {
    //     vkutils::beginDebugLabel(cmd, "Shadow");
    //
    //     size_t i = 0;
    //     for (auto &light : sceneManager.getLights()) {
    //         int shadowMapIndex = light.shadowMapIndex;
    //         if (shadowMapIndex < 0) continue;
    //
    //         Image &shadowMap = shadowMaps[i];
    //
    //         VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    //         depthAttachment.clearValue.depthStencil = {0.0, 0};
    //         depthAttachment.imageView = shadowMap.view;
    //         depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    //         depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    //         depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //
    //         std::vector<Attachment> attachments = {
    //             std::pair(depthAttachment, shadowMap),
    //         };
    //
    //         graphics.beginFrame(cmd, attachments, {shadowMapSize, shadowMapSize});
    //
    //         vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.scene);
    //         vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &set, 0, nullptr);
    //         vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    //
    //         mat4 lightMVP = light.mvp;
    //
    //         // render
    //         for (Mesh &mesh : sceneManager.getSceneByName("sponza").meshes) {
    //             mat4 mvp = lightMVP * mesh.matrix;
    //             vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &mvp);
    //             vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    //         }
    //
    //         graphics.endFrame(cmd, false);
    //
    //         vkutils::insertImageBarrier(
    //             cmd, shadowMap.handle,
    //             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
    //             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //             {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});
    //
    //         i++;
    //     }
    //
    //     vkutils::endDebugLabel(cmd);
    // }

    //
    // Scene pass
    //
    if (scenesCount > 0) {
        vkutils::beginDebugLabel(cmd, "Scenes");

        const Image &renderImage = graphics.getRenderImage();
        const Image &depthImage = graphics.getDepthImage();
        const VkExtent2D extent = graphics.getSwapchainExtent();

        const VkImage &swapchainImage = graphics.getSwapchainImage();
        const VkImageView &swapchainImageView = graphics.getSwapchainImageView();

        // prepare attachments

        VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
        colorAttachment.imageView = renderImage.view;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.resolveImageView = swapchainImageView;
        colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depthAttachment.clearValue.depthStencil = {0.0, 0};
        depthAttachment.imageView = depthImage.view;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        // layout transion
        vkutils::insertImageBarrier(
            cmd, renderImage.handle, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        vkutils::insertImageBarrier(
            cmd, swapchainImage, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        vkutils::insertImageBarrier(
            cmd, depthImage.handle, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});

        vkutils::beginRendering(cmd, &colorAttachment, 1, &depthAttachment, extent);

        vkutils::setViewport(cmd, 0.0f, 0.0f, extent.width, extent.height);
        vkutils::setScissor(cmd, extent);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.scene);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &set, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        // render
        for (Mesh &mesh : sceneManager.getSceneByName("sponza").meshes) {
            PushConstant push = {};
            push.model = mesh.matrix;
            push.materialIndex = mesh.materialIndex;

            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                    sizeof(push), &push);

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
        }

        // frame end
        vkCmdEndRendering(cmd);
        vkutils::insertImageBarrier(
            cmd, swapchainImage, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        vkutils::endDebugLabel(cmd);
    }

    //
    // Imgui Pass
    //
    renderImgui(cmd);

    graphics.endCommandBuffer(cmd);
    graphics.submitCommandBuffer(cmd);
}

void Renderer::renderImgui(VkCommandBuffer cmd)
{
    vkutils::beginDebugLabel(cmd, "Dear ImGUI", {0.3, 0.3, 0.0, 0.5});

    // const Image &renderImage = graphics.getRenderImage();
    const VkExtent2D extent = graphics.getSwapchainExtent();

    const VkImage &swapchainImage = graphics.getSwapchainImage();
    const VkImageView &swapchainImageView = graphics.getSwapchainImageView();

    VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
    colorAttachment.imageView = swapchainImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // layout transion
    vkutils::insertImageBarrier(
        cmd, swapchainImage, 0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    vkutils::beginRendering(cmd, &colorAttachment, 1, nullptr, extent);

    vkutils::setViewport(cmd, 0.0f, 0.0f, extent.width, extent.height);
    vkutils::setScissor(cmd, extent);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    {
        ImGui::Begin("Debug");
        ImGui::Text("Verices: %zu", sceneManager.getVertices().size());
        ImGui::Text("Textures: %zu", sceneManager.getTextures().size());
        ImGui::Text("Materials: %zu", sceneManager.getMaterials().size());
        ImGui::Text("Scenes: %zu", sceneManager.getScenes().size());
        ImGui::Text("Lights: %zu", sceneManager.getLights().size());
        ImGui::End();
    }

    {
        std::vector<Light> &lights = sceneManager.getLights();
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

    vkCmdEndRendering(cmd);
    vkutils::insertImageBarrier(
        cmd, swapchainImage, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    vkutils::endDebugLabel(cmd);
}

void Renderer::createResources()
{
    const VkDevice device = graphics.getDevice();

    // load textures from paths
    auto &texturePaths = sceneManager.getTexturePaths();
    for (size_t i = 0; i < texturePaths.size(); i++) {
        Logger::println(LOG_INFO, "Loading texture: ", texturePaths[i]);
        sceneManager.addTexture(graphics, texturePaths[i]);
    }

    // load skybox texture
    graphics.createTextureCubemap(skybox, "textures/skybox", VK_FORMAT_R8G8B8A8_SRGB);

    // load global vertex and index buffers
    auto &vertices = sceneManager.getVertices();
    auto &indices = sceneManager.getIndices();

    uint32_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    uint32_t indexBufferSize = indices.size() * sizeof(uint32_t);
    graphics.createBuffer(vertexBuffer, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    graphics.createBuffer(indexBuffer, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    graphics.uploadBuffer(vertexBuffer, vertices.data(), vertexBufferSize);
    graphics.uploadBuffer(indexBuffer,  indices.data(), indexBufferSize);

    // ubo buffer
    graphics.createBuffer(uboBuffer, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    vkutils::setDebugName(device, (uint64_t)uboBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "globalUBO");

    auto &lights = sceneManager.getLights();
    auto &textures = sceneManager.getTextures();
    auto &materials = sceneManager.getMaterials();

    // materials buffer
    graphics.createBuffer(materialsBuffer, materials.size() * sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkutils::setDebugName(device, (uint64_t)materialsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "materialsBuffer");

    // lights buffer
    graphics.createBuffer(lightsBuffer, lights.size() * sizeof(Light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkutils::setDebugName(device, (uint64_t)lightsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "lightsBuffer");

    // create shadow map for every light
    for (auto &light : lights) {
        uint shadowMapIndex = textures.size();
        Texture &shadowMap = textures.emplace_back();

        graphics.createImage(shadowMap.image, shadowMapSize, shadowMapSize, VK_FORMAT_D32_SFLOAT,
             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
             VK_IMAGE_ASPECT_DEPTH_BIT, graphics.getMultisampleCount(), VK_IMAGE_VIEW_TYPE_2D,
             VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        light.shadowMapIndex = shadowMapIndex;
        shadowMaps.push_back(shadowMap.image);
    }
}

void Renderer::loadShaders(std::filesystem::path dir)
{
    const VkDevice device = graphics.getDevice();

    using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

    for (const auto& dirEntry : recursive_directory_iterator(dir)) {
        if (dirEntry.is_regular_file()) {
            auto file = dirEntry.path();
            Logger::println(LOG_INFO, "Loading shader: ", file);

            auto shader = vkutils::loadShaderModule(device, file);

            if (shader != VK_NULL_HANDLE) {
                shaders[file.filename().string()] = shader;
                vkutils::setDebugName(device, (uint64_t)shader, VK_OBJECT_TYPE_SHADER_MODULE, file.filename().c_str());
            } else {
                Logger::println(LOG_ERROR, "Failed to load shader: ", file);
            }

        }
    }
}

void Renderer::createDescriptors()
{
    const VkDevice device = graphics.getDevice();
    auto &textures = sceneManager.getTextures();
    size_t texturesSize = textures.size() > 0 ? textures.size() : 1; // use 1 to silent validation layers

    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize)},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, // ubo
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}, // lights, materials, vertices
    };

    pool = vkutils::createDescriptorPool(device, poolSizes);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}, // vertices
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}, // ubo
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // materials
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // lights
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize), VK_SHADER_STAGE_FRAGMENT_BIT}, // textures
    };

    setLayout = vkutils::createDescriptorSetLayout(device, bindings.data(), bindings.size(), nullptr);
    set = vkutils::createDescriptorSet(device, pool, setLayout);

    DescriptorWriter writer;
    writer.write(0, vertexBuffer.buffer, vertexBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writer.write(1, uboBuffer.buffer, uboBuffer.size, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write(2, materialsBuffer.buffer, materialsBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writer.write(3, lightsBuffer.buffer, lightsBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    std::vector<VkDescriptorImageInfo> textureInfos(textures.size());
    for (size_t i = 0; i < textures.size(); i++) {
        textureInfos[i].imageView = textures[i].image.view;
        textureInfos[i].sampler = textures[i].image.sampler;
        textureInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    writer.write(4, textureInfos.data(), textureInfos.size(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.update(device, set);
}

void Renderer::createPipelines()
{
    const VkDevice device = graphics.getDevice();

    // create pipeline layout
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant)};
    pipelineLayout = vkutils::createPipelineLayout(device, &setLayout, &pushConstant);

    // create pipeline
    PipelineBuilder builder;

    // scene pipeline
    builder.setPipelineLayout(pipelineLayout);
    builder.setShader(shaders["mesh.vert.spv"], VK_SHADER_STAGE_VERTEX_BIT);
    builder.setShader(shaders["mesh.frag.spv"], VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.setDepthTest(true);
    builder.setCulling(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    builder.setMultisampleCount(graphics.getMultisampleCount());
    pipelines.scene = builder.build(device);

    builder.clearShaders();

    // shadow pipeline
    builder.setPipelineLayout(pipelineLayout);
    builder.setShader(shaders["depth.vert.spv"], VK_SHADER_STAGE_VERTEX_BIT);
    // builder.setDepthBias(true);
    builder.setCulling(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelines.shadow = builder.build(device, 0, true);

    vkutils::setDebugName(device, reinterpret_cast<uint64_t>(pipelines.scene), VK_OBJECT_TYPE_PIPELINE, "scene pipeline");
    vkutils::setDebugName(device, reinterpret_cast<uint64_t>(pipelines.shadow), VK_OBJECT_TYPE_PIPELINE, "shadow pipeline");
}

void Renderer::updateDynamicBuffers()
{
    std::vector<Material> &materials = sceneManager.getMaterials();
    std::vector<Light> &lights = sceneManager.getLights();

    for (auto &light : lights) {
        mat4 projection = math::perspective(glm::radians(45.0f), 1.0f, 1.0f, 100.0f);
        mat4 view = glm::lookAt(light.position, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4 mvp = projection * view;
        light.mvp = mvp;
    }

    if (lights.size() > 0)
        memcpy(lightsBuffer.info.pMappedData, lights.data(), lightsBuffer.size);

    GlobalUBO ubo = {
        .projection = camera.getProjection(),
        .view = camera.getView(),
        .numLights = static_cast<uint>(lights.size()),
        .cameraPos = camera.getPosition(),
    };
    memcpy(uboBuffer.info.pMappedData, &ubo, sizeof(ubo));

    if (materials.size() > 0)
        memcpy(materialsBuffer.info.pMappedData, materials.data(), materialsBuffer.size);
}
