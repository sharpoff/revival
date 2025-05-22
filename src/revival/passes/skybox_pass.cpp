#include <revival/passes/skybox_pass.h>
#include <revival/vulkan/utils.h>
#include <revival/vulkan/graphics.h>
#include <revival/scene_manager.h>
#include <revival/vulkan/pipeline_builder.h>
#include <revival/vulkan/descriptor_writer.h>

void SkyboxPass::init(VulkanGraphics &graphics, Texture &skybox)
{
    VkDevice device = graphics.getDevice();

    graphics.createBuffer(uboBuffer, sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    vkutils::setDebugName(device, (uint64_t)uboBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "skyboxUBO");

    //
    // Descriptor set
    //
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, // ubo
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, // skybox
    };

    pool = vkutils::createDescriptorPool(device, poolSizes);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}, // ubo
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}, // skybox
    };

    setLayout = vkutils::createDescriptorSetLayout(device, bindings.data(), bindings.size(), nullptr);
    set = vkutils::createDescriptorSet(device, pool, setLayout);

    DescriptorWriter writer;
    writer.write(0, uboBuffer.buffer, uboBuffer.size, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write(1, skybox.image.view, skybox.image.sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update(device, set);

    //
    // Pipeline
    //
    auto vertex = vkutils::loadShaderModule(device, "build/shaders/skybox.vert.spv");
    auto fragment = vkutils::loadShaderModule(device, "build/shaders/skybox.frag.spv");
    vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "skybox.vert");
    vkutils::setDebugName(device, (uint64_t)fragment, VK_OBJECT_TYPE_SHADER_MODULE, "skybox.frag");

    // create pipeline layout
    layout = vkutils::createPipelineLayout(device, &setLayout, nullptr);

    // create pipeline
    PipelineBuilder builder;
    builder.setPipelineLayout(layout);
    builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
    builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.setDepthTest(false);
    builder.setCulling(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    builder.setAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos));
    builder.setBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    pipeline = builder.build(device);
    vkutils::setDebugName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, "skybox pipeline");

    vkDestroyShaderModule(device, vertex, nullptr);
    vkDestroyShaderModule(device, fragment, nullptr);
}

void SkyboxPass::shutdown(VulkanGraphics &graphics, VkDevice device)
{
    graphics.destroyBuffer(uboBuffer);

    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

void SkyboxPass::render(VulkanGraphics &graphics, VkCommandBuffer cmd, VkBuffer &vertexBuffer, VkBuffer &indexBuffer, Camera &camera, Scene &cubeScene)
{
    // update ubo
    UBO ubo = {};
    ubo.projection = camera.getProjection();
    ubo.view = camera.getView();
    memcpy(uboBuffer.info.pMappedData, &ubo, sizeof(ubo));

    Image swapchainImage = {};
    swapchainImage.view = graphics.getSwapchainImageView();
    swapchainImage.handle = graphics.getSwapchainImage();

    VkRenderingAttachmentInfo colorAttachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.clearValue.color = {{0.0, 0.0, 0.0, 1.0}};
    colorAttachment.imageView = swapchainImage.view;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    std::vector<std::pair<VkRenderingAttachmentInfo, Image>> attachments = {
        std::make_pair(colorAttachment, swapchainImage),
    };

    graphics.beginFrame(cmd, attachments, graphics.getSwapchainExtent());

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);

    VkDeviceSize offset = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    for (auto &mesh : cubeScene.meshes) {
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }

    graphics.endFrame(cmd);
}
