#include <revival/renderer.h>
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
    VulkanGraphics vkContext;
    Camera *camera;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    Buffer uboBuffer;
    Buffer materialsBuffer;
    Buffer lightsBuffer;

    Image depthImage;

    Image shadowMap;
    unsigned int shadowMapIndex;

    std::vector<Texture> textures;

    const int shadowMapSize = 2048;

    bool debugShadowMap = true;
    bool depthViewOn = false;

    struct GlobalUBO
    {
        alignas(16) mat4 projection;
        alignas(16) mat4 view;
        uint numLights;
        alignas(16) vec3 cameraPos;
    };

    struct MeshPC
    {
        alignas(16) mat4 model;
        int materialIndex;
    };

    struct {
        struct {
            VkPipelineLayout layout;
            VkPipeline pipeline;
        } scene;

        struct {
            VkPipelineLayout layout;
            VkPipeline pipeline;
        } shadowMap;

        struct {
            VkPipelineLayout layout;
            VkPipeline pipeline;
        } quad;
    } pipelines;

    struct {
        struct {
            VkDescriptorPool pool;
            VkDescriptorSetLayout setLayout;
            VkDescriptorSet set;
        } scene;

        struct {
            VkDescriptorPool pool;
            VkDescriptorSetLayout setLayout;
            VkDescriptorSet set;
        } shadowMap;

        struct {
            VkDescriptorPool pool;
            VkDescriptorSetLayout setLayout;
            VkDescriptorSet set;
        } quad;
    } descriptors;


    void initialize(Camera *pCamera, GLFWwindow *pWindow)
    {
        window = pWindow;
        camera = pCamera;

        vkContext.init(window);

        SceneManager::addLight({mat4(1.0), vec3(18.0, 19.0, 22.0), vec3(1.0)});

        // load scenes
        SceneManager::loadScene("shadow_test", "models/shadow_test.gltf");
        // SceneManager::loadScene("sponza", "models/sponza/Sponza.gltf");

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

        vkContext.destroyImage(depthImage);

        vkContext.destroyBuffer(vertexBuffer);
        vkContext.destroyBuffer(indexBuffer);

        for (auto &texture : textures) {
            vkContext.destroyTexture(texture);
        }

        // Pipelines
        vkDestroyPipelineLayout(device, pipelines.scene.layout, nullptr);
        vkDestroyPipelineLayout(device, pipelines.shadowMap.layout, nullptr);
        vkDestroyPipelineLayout(device, pipelines.quad.layout, nullptr);

        vkDestroyPipeline(device, pipelines.scene.pipeline, nullptr);
        vkDestroyPipeline(device, pipelines.shadowMap.pipeline, nullptr);
        vkDestroyPipeline(device, pipelines.quad.pipeline, nullptr);

        // Descriptors
        vkDestroyDescriptorPool(device, descriptors.scene.pool, nullptr);
        vkDestroyDescriptorPool(device, descriptors.shadowMap.pool, nullptr);
        vkDestroyDescriptorPool(device, descriptors.quad.pool, nullptr);

        vkDestroyDescriptorSetLayout(device, descriptors.scene.setLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptors.shadowMap.setLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptors.quad.setLayout, nullptr);

        vkContext.shutdown();
    }

    void render()
    {
        updateDynamicBuffers();

        VkCommandBuffer cmd = vkContext.beginCommandBuffer();

        Image swapchainImage;
        swapchainImage.view = vkContext.getSwapchainImageView();
        swapchainImage.handle = vkContext.getSwapchainImage();

        //
        // Shadow map
        //
        {
            VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            depthAttachment.clearValue.depthStencil = {0.0, 0};
            depthAttachment.imageView = shadowMap.view;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            std::vector<std::pair<VkRenderingAttachmentInfo, Image>> attachments = {
                std::make_pair(depthAttachment, shadowMap),
            };

            vkutils::beginDebugLabel(cmd, "Shadow", {0.3, 0.3, 0.3, 0.5});
            vkContext.beginFrame(cmd, attachments, {shadowMapSize, shadowMapSize});

            Light &light = SceneManager::getLights()[0];
            renderShadowMap(cmd, light.mvp, SceneManager::getSceneByName("shadow_test"));
            // renderShadowMap(cmd, light.mvp, SceneManager::getSceneByName("sponza"));

            vkContext.endFrame(cmd, false);
            vkutils::endDebugLabel(cmd);

            // Is this necessary?
            vkutils::insertImageBarrier(
                cmd, shadowMap.handle,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});
        }

        //
        // Scenes
        //
        {
            VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
            colorAttachment.imageView = swapchainImage.view;
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            depthAttachment.clearValue.depthStencil = {0.0, 0};
            depthAttachment.imageView = depthImage.view;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            std::vector<std::pair<VkRenderingAttachmentInfo, Image>> attachments = {
                std::make_pair(colorAttachment, swapchainImage),
                std::make_pair(depthAttachment, depthImage),
            };

            vkutils::beginDebugLabel(cmd, "Scenes");
            vkContext.beginFrame(cmd, attachments, vkContext.getSwapchainExtent());

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.scene.pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.scene.layout, 0, 1, &descriptors.scene.set, 0, nullptr);
            vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            renderScene(cmd, SceneManager::getSceneByName("shadow_test"));
            // renderScene(cmd, SceneManager::getSceneByName("sponza"));
            vkContext.endFrame(cmd);
            vkutils::endDebugLabel(cmd);
        }

        //
        // Fullscreen Quad (depth debug)
        //
        if (depthViewOn) {
            VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
            colorAttachment.imageView = swapchainImage.view;
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            std::vector<std::pair<VkRenderingAttachmentInfo, Image>> attachments = {
                std::make_pair(colorAttachment, swapchainImage),
            };

            vkutils::beginDebugLabel(cmd, "Quad");
            vkContext.beginFrame(cmd, attachments, vkContext.getSwapchainExtent());

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.quad.pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.quad.layout, 0, 1, &descriptors.quad.set, 0, nullptr);

            vkCmdDraw(cmd, 3, 1, 0, 0);

            vkContext.endFrame(cmd);
            vkutils::endDebugLabel(cmd);
        }

        // Imgui
        {
            vkutils::beginDebugLabel(cmd, "Dear ImGUI", {0.3, 0.3, 0.0, 0.5});
            renderImgui(cmd);
            vkutils::endDebugLabel(cmd);
        }

        vkContext.endCommandBuffer(cmd);
        vkContext.submitCommandBuffer(cmd);
    }

    void renderShadowMap(VkCommandBuffer cmd, mat4 mvp, Scene &scene)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadowMap.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadowMap.layout, 0, 1, &descriptors.shadowMap.set, 0, nullptr);
        vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        for (auto &mesh : scene.meshes) {
            vkCmdPushConstants(cmd, pipelines.shadowMap.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &mvp);

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
        }
    }

    void renderScene(VkCommandBuffer cmd, Scene &scene)
    {
        MeshPC push;
        for (auto &mesh : scene.meshes) {
            push.model = scene.matrix * mesh.matrix;
            push.materialIndex = mesh.materialIndex;

            vkCmdPushConstants(cmd, pipelines.scene.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPC), &push);

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
        }
    }

    void renderImgui(VkCommandBuffer cmd)
    {
        Image swapchainImage;
        swapchainImage.view = vkContext.getSwapchainImageView();
        swapchainImage.handle = vkContext.getSwapchainImage();

        // TODO: implement it
        VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
        colorAttachment.imageView = swapchainImage.view;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        std::vector<std::pair<VkRenderingAttachmentInfo, Image>> attachments = {
            std::make_pair(colorAttachment, swapchainImage),
        };

        vkContext.beginFrame(cmd, attachments, vkContext.getSwapchainExtent());

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        {
            ImGui::Begin("Debug");
            ImGui::Text("Verices: %zu", SceneManager::getVertices().size());
            ImGui::Text("Textures: %zu", textures.size());
            ImGui::Text("Materials: %zu", SceneManager::getMaterials().size());
            ImGui::Text("Scenes: %zu", SceneManager::getScenes().size());
            ImGui::Text("Lights: %zu", SceneManager::getLights().size());

            ImGui::Checkbox("Depth view", &depthViewOn);
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

        vkContext.endFrame(cmd);
    }

    void resizeResources()
    {
        vkContext.destroyImage(depthImage);

        vkContext.createImage(depthImage, vkContext.getSwapchainExtent().width, vkContext.getSwapchainExtent().height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);
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
            TextureInfo info;
            vkContext.loadTextureInfo(info, texturePaths[i].c_str());
            vkContext.createTexture(textures[i], info, VK_FORMAT_R8G8B8A8_SRGB);
        }

        // depth image
        vkContext.createImage(depthImage, vkContext.getSwapchainExtent().width, vkContext.getSwapchainExtent().height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

        // shadowMap image
        vkContext.createImage(shadowMap, shadowMapSize, shadowMapSize, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT);

        // HACK: pushing image into textures to get it from shader as a shadowmap(texture) index
        textures.push_back({shadowMap});
        shadowMapIndex = textures.size() - 1;

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
        vkContext.createBuffer(uboBuffer, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
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

        //
        // Scene descriptor set
        //
        {
            // use 1 to silent validation layers
            size_t texturesSize = textures.size() > 0 ? textures.size() : 1;

            std::vector<VkDescriptorPoolSize> poolSizes = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize)},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, // ubo
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}, // lights, materials, vertices
            };

            descriptors.scene.pool = vkutils::createDescriptorPool(device, poolSizes);

            std::vector<VkDescriptorSetLayoutBinding> bindings = {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}, // vertices
                {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}, // ubo
                {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // materials
                {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // lights
                {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize), VK_SHADER_STAGE_FRAGMENT_BIT}, // textures
            };

            descriptors.scene.setLayout = vkutils::createDescriptorSetLayout(device, bindings);
            descriptors.scene.set = vkutils::createDescriptorSet(device, descriptors.scene.pool, descriptors.scene.setLayout);

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
                    textureInfos[i].imageView = textures[i].image.view;
                    textureInfos[i].sampler = textures[i].image.sampler;
                    textureInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
                writer.write(4, textureInfos.data(), textureInfos.size(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            }

            writer.update(device, descriptors.scene.set);
        }

        //
        // Shadow map descriptor set
        //
        {
            std::vector<VkDescriptorPoolSize> poolSizes = {
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}, // vertices
            };

            descriptors.shadowMap.pool = vkutils::createDescriptorPool(device, poolSizes);

            std::vector<VkDescriptorSetLayoutBinding> bindings = {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}, // vertices
            };

            descriptors.shadowMap.setLayout = vkutils::createDescriptorSetLayout(device, bindings);
            descriptors.shadowMap.set = vkutils::createDescriptorSet(device, descriptors.shadowMap.pool, descriptors.shadowMap.setLayout);

            DescriptorWriter writer;
            writer.write(0, vertexBuffer.buffer, vertexBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

            writer.update(device, descriptors.shadowMap.set);
        }

        //
        // Quad descriptor set
        //
        {
            std::vector<VkDescriptorPoolSize> poolSizes = {
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}, // vertices
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
            };

            descriptors.quad.pool = vkutils::createDescriptorPool(device, poolSizes);

            std::vector<VkDescriptorSetLayoutBinding> bindings = {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}, // vertices
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
            };

            descriptors.quad.setLayout = vkutils::createDescriptorSetLayout(device, bindings);
            descriptors.quad.set = vkutils::createDescriptorSet(device, descriptors.quad.pool, descriptors.quad.setLayout);

            DescriptorWriter writer;
            writer.write(0, vertexBuffer.buffer, vertexBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

            VkDescriptorImageInfo textureInfo;
            textureInfo.imageView = textures[shadowMapIndex].image.view;
            textureInfo.sampler = textures[shadowMapIndex].image.sampler;
            textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            writer.write(1, &textureInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            writer.update(device, descriptors.quad.set);
        }
    }

    void createPipelines()
    {
        VkDevice device = vkContext.getDevice();

        //
        // Scene pipeline
        //
        {
            auto vertex = vkutils::loadShaderModule(device, "build/shaders/mesh.vert.spv");
            auto fragment = vkutils::loadShaderModule(device, "build/shaders/mesh.frag.spv");
            vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.vert");
            vkutils::setDebugName(device, (uint64_t)fragment, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.frag");

            // create pipeline layout
            VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPC)};
            pipelines.scene.layout = vkutils::createPipelineLayout(device, descriptors.scene.setLayout, pushConstant);

            // create pipeline
            PipelineBuilder builder;
            builder.setPipelineLayout(pipelines.scene.layout);
            builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
            builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
            builder.setDepthTest(true);
            builder.setCulling(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            pipelines.scene.pipeline = builder.build(device);
            vkutils::setDebugName(device, (uint64_t)pipelines.scene.pipeline, VK_OBJECT_TYPE_PIPELINE, "scene pipeline");

            vkDestroyShaderModule(device, vertex, nullptr);
            vkDestroyShaderModule(device, fragment, nullptr);
        }

        //
        // Shadowmap pipeline
        //
        {
            auto vertex = vkutils::loadShaderModule(device, "build/shaders/depth.vert.spv");
            vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "depth.vert");

            // create pipeline layout
            VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4)};
            pipelines.shadowMap.layout = vkutils::createPipelineLayout(device, descriptors.shadowMap.setLayout, pushConstant);

            // create pipeline
            PipelineBuilder builder;
            builder.setPipelineLayout(pipelines.shadowMap.layout);
            builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
            builder.setDepthTest(true);
            builder.setCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            pipelines.shadowMap.pipeline = builder.build(device, 0, true);
            vkutils::setDebugName(device, (uint64_t)pipelines.shadowMap.pipeline, VK_OBJECT_TYPE_PIPELINE, "shadowmap pipeline");

            vkDestroyShaderModule(device, vertex, nullptr);
        }

        //
        // Quad pipeline
        //
        {
            auto vertex = vkutils::loadShaderModule(device, "build/shaders/quad.vert.spv");
            auto fragment = vkutils::loadShaderModule(device, "build/shaders/quad.frag.spv");
            vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "quad.vert");
            vkutils::setDebugName(device, (uint64_t)fragment, VK_OBJECT_TYPE_SHADER_MODULE, "quad.frag");

            // create pipeline layout
            pipelines.quad.layout = vkutils::createPipelineLayout(device, descriptors.quad.setLayout);

            // create pipeline
            PipelineBuilder builder;
            builder.setPipelineLayout(pipelines.quad.layout);
            builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
            builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
            builder.setDepthTest(false);
            builder.setCulling(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            builder.setPolygonMode(VK_POLYGON_MODE_FILL);
            pipelines.quad.pipeline = builder.build(device, 0, true);
            vkutils::setDebugName(device, (uint64_t)pipelines.quad.pipeline, VK_OBJECT_TYPE_PIPELINE, "quad pipeline");

            vkDestroyShaderModule(device, vertex, nullptr);
            vkDestroyShaderModule(device, fragment, nullptr);
        }
    }

    void updateDynamicBuffers()
    {
        std::vector<Material> &materials = SceneManager::getMaterials();
        std::vector<Light> &lights = SceneManager::getLights();

        // const mat4 biasMat = mat4( 
        //     0.5, 0.0, 0.0, 0.0,
        //     0.0, 0.5, 0.0, 0.0,
        //     0.0, 0.0, 0.5, 0.0,
        //     0.5, 0.5, 0.5, 1.0
        // );

        for (auto &light : lights) {
            mat4 projection = math::perspective(glm::radians(45.0f), 1.0f, 1.0f, 100.0f);
            mat4 view = glm::lookAt(light.position, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
            mat4 mvp = projection * view * mat4(1.0f);

            // light.mvp = biasMat * mvp;
            light.mvp = mvp;
            light.shadowMapIndex = shadowMapIndex;
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

} // namespace Renderer
