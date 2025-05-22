#include <revival/passes/scene_pass.h>
#include <revival/vulkan/utils.h>
#include <revival/vulkan/graphics.h>
#include <revival/scene_manager.h>
#include <revival/vulkan/pipeline_builder.h>
#include <revival/vulkan/descriptor_writer.h>

void ScenePass::init(VulkanGraphics &graphics, std::vector<Texture> &textures, Buffer &vertexBuffer, Buffer &uboBuffer, Buffer &materialsBuffer, Buffer &lightsBuffer)
{
    VkDevice device = graphics.getDevice();

    //
    // Descriptor set
    //
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

    writer.update(device, set);

    //
    // Pipeline
    //
    auto vertex = vkutils::loadShaderModule(device, "build/shaders/mesh.vert.spv");
    auto fragment = vkutils::loadShaderModule(device, "build/shaders/mesh.frag.spv");
    vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.vert");
    vkutils::setDebugName(device, (uint64_t)fragment, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.frag");

    // create pipeline layout
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant)};
    layout = vkutils::createPipelineLayout(device, &setLayout, &pushConstant);

    // create pipeline
    PipelineBuilder builder;
    builder.setPipelineLayout(layout);
    builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
    builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.setDepthTest(true);
    builder.setCulling(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipeline = builder.build(device);
    vkutils::setDebugName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, "scene pipeline");

    vkDestroyShaderModule(device, vertex, nullptr);
    vkDestroyShaderModule(device, fragment, nullptr);
}

void ScenePass::shutdown(VkDevice device)
{
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

void ScenePass::beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer &indexBuffer)
{
    Image &depthImage = graphics.getDepthImage();

    Image swapchainImage = {};
    swapchainImage.view = graphics.getSwapchainImageView();
    swapchainImage.handle = graphics.getSwapchainImage();

    VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
    colorAttachment.imageView = swapchainImage.view;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
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

    graphics.beginFrame(cmd, attachments, graphics.getSwapchainExtent());

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void ScenePass::endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd)
{
    graphics.endFrame(cmd);
}

void ScenePass::render(VkCommandBuffer cmd, Scene &scene)
{
    PushConstant push = {};
    for (auto &mesh : scene.meshes) {
        push.model = mesh.matrix;
        push.materialIndex = mesh.materialIndex;

        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }
}

void ScenePass::render(VkCommandBuffer cmd, GameObject &gameObject)
{
    if (!gameObject.scene) return;

    PushConstant push = {};
    for (auto &mesh : gameObject.scene->meshes) {
        push.model = gameObject.transform.getModelMatrix() * mesh.matrix;
        push.materialIndex = mesh.materialIndex;

        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }
}
