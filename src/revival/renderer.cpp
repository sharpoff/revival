#include <revival/renderer.h>
#include <revival/resources.h>
#include <revival/vulkan/utils.h>
#include <revival/scene_manager.h>
#include <revival/vulkan/descriptor_writer.h>
#include <revival/vulkan/pipeline_builder.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

namespace Renderer
{
    GLFWwindow *window;
    VulkanContext vkContext;
    Camera *camera;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Texture depthTexture;

    std::vector<Texture> textures;

    struct UniformBuffer
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };

    struct PushConstant
    {
        alignas(16) mat4 model;
        int materialIndex;
    };

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    struct {
        VkPipelineLayout pipelineLayout;
        VkPipeline scenePipeline;
    } pipelines;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    void initialize(Camera *pCamera, GLFWwindow *pWindow)
    {
        window = pWindow;
        camera = pCamera;

        vkContext.create(window);

        SceneManager::addLight({vec3(2.0, 4.0, 4.0), vec3(1.0)});

        // load scenes
        SceneManager::loadScene("shadow_test", "models/shadow_test.gltf");
        SceneManager::loadScene("helmet", "models/DamagedHelmet/DamagedHelmet.gltf");

        createResources();
        createDescriptors();
        createPipelines();
    }

    void shutdown()
    {
        vkDeviceWaitIdle(vkContext.getDevice());
        VkDevice device = vkContext.getDevice();

        // Resources
        vkContext.destroyBuffer(uboBuffer);
        vkContext.destroyBuffer(materialsBuffer);
        vkContext.destroyBuffer(lightsBuffer);

        vkContext.destroyTexture(depthTexture);

        vkContext.destroyBuffer(vertexBuffer);
        vkContext.destroyBuffer(indexBuffer);

        for (auto &texture : textures) {
            vkContext.destroyTexture(texture);
        }

        // Pipelines
        vkDestroyPipelineLayout(device, pipelines.pipelineLayout, nullptr);
        vkDestroyPipeline(device, pipelines.scenePipeline, nullptr);

        // Descriptors
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkContext.destroy();
    }

    void render()
    {
        updateDynamicBuffers();

        auto cmd = vkContext.beginCommandBuffer();
        vkContext.beginFrame(cmd, depthTexture.image.image, depthTexture.imageView);
        vkutils::beginDebugLabel(cmd, "Scenes", {0.3, 0.3, 0.3, 0.5});

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.scenePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        renderScene(cmd, SceneManager::getSceneByName("shadow_test"));
        renderScene(cmd, SceneManager::getSceneByName("helmet"));

        renderImgui(cmd);

        vkutils::endDebugLabel(cmd);
        vkContext.endFrame(cmd);
        vkContext.endCommandBuffer(cmd);
        vkContext.submitCommandBuffer(cmd);
    }

    void renderScene(VkCommandBuffer cmd, Scene &scene)
    {
        PushConstant push;
        for (auto &mesh : scene.meshes) {
            push.model = scene.matrix * mesh.matrix;
            push.materialIndex = mesh.materialIndex;

            vkCmdPushConstants(cmd, pipelines.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &push);

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
        }
    }

    void renderImgui(VkCommandBuffer cmd)
    {
        vkutils::beginDebugLabel(cmd, "Dear ImGUI", {0.3, 0.3, 0.0, 0.5});

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        {
            ImGui::Begin("Stats");
            ImGui::Text("Verices: %zu", SceneManager::getVertices().size());
            ImGui::Text("Textures: %zu", textures.size());
            ImGui::Text("Materials: %zu", SceneManager::getMaterials().size());
            ImGui::Text("Scenes: %zu", SceneManager::getScenes().size());
            ImGui::Text("Lights: %zu", SceneManager::getLights().size());
            ImGui::End();
        }

        {
            std::vector<Light> &lights = SceneManager::getLights();

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

        vkutils::endDebugLabel(cmd);
    }

    void resizeResources()
    {
        vkContext.destroyTexture(depthTexture);

        vkContext.createDepthTexture(depthTexture, vkContext.getSwapchainExtent().width, vkContext.getSwapchainExtent().height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void requestResize()
    {
        vkContext.requestResize();
    }

    void createResources()
    {
        VkDevice device = vkContext.getDevice();

        // load textures from paths
        auto &texturePaths = SceneManager::getTexturePaths();
        textures.resize(texturePaths.size());
        for (size_t i = 0; i < textures.size(); i++) {
            printf("Loading texture: %s\n", texturePaths[i].c_str());
            vkContext.createTexture(textures[i], texturePaths[i].c_str(), VK_FORMAT_R8G8B8A8_SRGB);
        }

        // depth image
        vkContext.createDepthTexture(depthTexture, vkContext.getSwapchainExtent().width, vkContext.getSwapchainExtent().height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        // load global vertices and indices
        auto &vertices = SceneManager::getVertices();
        auto &indices = SceneManager::getIndices();

        uint32_t vertexBufferSize = vertices.size() * sizeof(Vertex);
        uint32_t indexBufferSize = indices.size() * sizeof(uint32_t);
        vkContext.createBuffer(vertexBuffer, vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        vkContext.createBuffer(indexBuffer, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        vkContext.uploadBuffer(vertexBuffer, vertices.data(), vertexBufferSize);
        vkContext.uploadBuffer(indexBuffer,  indices.data(), indexBufferSize);

        // create buffers
        vkContext.createBuffer(uboBuffer, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        vkutils::setDebugName(device, (uint64_t)uboBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "uboBuffer");

        std::vector<Material> &materials = SceneManager::getMaterials();
        vkContext.createBuffer(materialsBuffer, materials.size() * sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        vkutils::setDebugName(device, (uint64_t)materialsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "materialsBuffer");

        std::vector<Light> &lights = SceneManager::getLights();
        if (lights.size() > 0) {
            vkContext.createBuffer(lightsBuffer, lights.size() * sizeof(Light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            vkutils::setDebugName(device, (uint64_t)lightsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "lightsBuffer");
        }
    }

    void createDescriptors()
    {
        VkDevice device = vkContext.getDevice();

        // use 1 to silent validation layers
        size_t texturesSize = textures.size() > 0 ? textures.size() : 1;

        std::vector<VkDescriptorPoolSize> poolSizes = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize)},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, // ubo
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}, // lights, materials, vertices
        };

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}, // vertices
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}, // ubo
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // materials
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // lights
            {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize), VK_SHADER_STAGE_FRAGMENT_BIT}, // textures
        };

        descriptorPool = vkutils::createDescriptorPool(device, poolSizes);
        descriptorSetLayout = vkutils::createDescriptorSetLayout(device, bindings);
        descriptorSet = vkutils::createDescriptorSet(device, descriptorPool, descriptorSetLayout);

        DescriptorWriter writer;
        writer.write(0, vertexBuffer.buffer, vertexBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        writer.write(1, uboBuffer.buffer, uboBuffer.size, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        if (materialsBuffer.size > 0) {
            writer.write(2, materialsBuffer.buffer, materialsBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        }

        if (lightsBuffer.size > 0) {
            writer.write(3, lightsBuffer.buffer, lightsBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        }

        std::vector<VkDescriptorImageInfo> textureInfos(textures.size());
        if (textures.size() > 0) {
            for (size_t i = 0; i < textures.size(); i++) {
                textureInfos[i].imageView = textures[i].imageView;
                textureInfos[i].sampler = textures[i].sampler;
                textureInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            writer.write(4, textureInfos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }

        writer.update(device, descriptorSet);
    }

    void createPipelines()
    {
        VkDevice device = vkContext.getDevice();

        // Scene pipeline
        {
            auto vertex = vkutils::loadShaderModule(device, "build/shaders/mesh.vert.spv");
            auto fragment = vkutils::loadShaderModule(device, "build/shaders/mesh.frag.spv");
            vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.vert");
            vkutils::setDebugName(device, (uint64_t)fragment, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.frag");

            // create pipeline layout
            VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant)};
            pipelines.pipelineLayout = vkutils::createPipelineLayout(device, descriptorSetLayout, pushConstant);

            // create pipeline
            PipelineBuilder builder;
            builder.setPipelineLayout(pipelines.pipelineLayout);
            builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
            builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
            builder.setDepthTest(true);
            builder.setCulling(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            builder.setBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
            pipelines.scenePipeline = builder.build(device);
            vkutils::setDebugName(device, (uint64_t)pipelines.scenePipeline, VK_OBJECT_TYPE_PIPELINE, "scene pipeline");

            vkDestroyShaderModule(device, vertex, nullptr);
            vkDestroyShaderModule(device, fragment, nullptr);
        }
    }

    void updateDynamicBuffers()
    {
        std::vector<Material> &materials = SceneManager::getMaterials();
        std::vector<Light> &lights = SceneManager::getLights();

        UniformBuffer ubo = {
            .projection = camera->getProjection(),
            .view = camera->getView(),
            .numLights = static_cast<uint>(lights.size()),
            .cameraPos = camera->getPosition(),
        };
        memcpy(uboBuffer.info.pMappedData, &ubo, sizeof(ubo));

        if (materials.size() > 0)
            memcpy(materialsBuffer.info.pMappedData, materials.data(), materialsBuffer.size);
        if (lights.size() > 0)
            memcpy(lightsBuffer.info.pMappedData, lights.data(), lightsBuffer.size);
    }

} // namespace Renderer
