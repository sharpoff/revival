#include <revival/passes/shadow_pass.h>
#include <revival/vulkan/utils.h>
#include <revival/vulkan/graphics.h>
#include <revival/scene_manager.h>
#include <revival/vulkan/pipeline_builder.h>
#include <revival/vulkan/descriptor_writer.h>

void ShadowPass::init(VulkanGraphics &graphics, std::vector<Texture> &textures, std::vector<Light> &lights, Buffer &vertexBuffer)
{
    VkDevice device = graphics.getDevice();

    //
    // Create resources
    //

    // create shadow map for every light
    for (auto &light : lights) {
        uint32_t shadowMapIndex = textures.size();
        Texture &shadowMap = textures.emplace_back();

        graphics.createImage(shadowMap.image, shadowMapSize, shadowMapSize, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

        light.shadowMapIndex = shadowMapIndex;
        shadowMaps.push_back(shadowMap.image);
    }

    //
    // Descriptor set
    //
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}, // vertices
    };

    pool = vkutils::createDescriptorPool(device, poolSizes, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}, // vertices
    };
    VkDescriptorBindingFlags bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    setLayout = vkutils::createDescriptorSetLayout(device, bindings.data(), bindings.size(), &bindingFlags);
    set = vkutils::createDescriptorSet(device, pool, setLayout);

    DescriptorWriter writer;
    writer.write(0, vertexBuffer.buffer, vertexBuffer.size, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writer.update(device, set);

    //
    // Pipeline
    //
    auto vertex = vkutils::loadShaderModule(device, "build/shaders/depth.vert.spv");
    vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "depth.vert");

    // create pipeline layout
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4)};
    layout = vkutils::createPipelineLayout(device, &setLayout, &pushConstant);

    // create pipeline
    PipelineBuilder builder;
    builder.setPipelineLayout(layout);
    builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
    builder.setDepthTest(true);
    // builder.setDepthBias(true);
    builder.setCulling(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipeline = builder.build(device, 0, true);
    vkutils::setDebugName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, "shadow pipeline");

    vkDestroyShaderModule(device, vertex, nullptr);
}

void ShadowPass::shutdown(VkDevice device)
{
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

void ShadowPass::beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer indexBuffer, uint32_t lightIndex)
{
    Image &shadowMap = shadowMaps[lightIndex];

    VkRenderingAttachmentInfo depthAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttachment.clearValue.depthStencil = {0.0, 0};
    depthAttachment.imageView = shadowMap.view;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    std::vector<std::pair<VkRenderingAttachmentInfo, Image>> attachments = {
        std::make_pair(depthAttachment, shadowMap),
    };

    graphics.beginFrame(cmd, attachments, {shadowMapSize, shadowMapSize});

    vkCmdSetDepthBias(cmd, depthBiasConstant, 0.0f, depthBiasSlope);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void ShadowPass::endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, uint32_t lightIndex)
{
    Image &shadowMap = shadowMaps[lightIndex];

    graphics.endFrame(cmd, false);

    vkutils::insertImageBarrier(
        cmd, shadowMap.handle,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1});
}

void ShadowPass::render(VkCommandBuffer cmd, Scene &scene, mat4 lightMVP)
{
    for (auto &mesh : scene.meshes) {
        mat4 mvp = lightMVP * mesh.matrix;
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &mvp);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }
}

void ShadowPass::render(VkCommandBuffer cmd, GameObject &gameObject, mat4 lightMVP)
{
    if (!gameObject.scene) return;

    lightMVP *= gameObject.transform.getModelMatrix();
    for (auto &mesh : gameObject.scene->meshes) {
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &lightMVP);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }
}
