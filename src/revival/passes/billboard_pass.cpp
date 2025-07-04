#include <revival/passes/billboard_pass.h>
#include <revival/vulkan/utils.h>
#include <revival/vulkan/graphics.h>
#include <revival/vulkan/pipeline_builder.h>
#include <revival/vulkan/descriptor_writer.h>

void BillboardPass::init(VulkanGraphics &graphics, std::vector<Texture> &textures)
{
    VkDevice device = graphics.getDevice();

    graphics.createBuffer(uboBuffer, sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    vkutils::setDebugName(device, (uint64_t)uboBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "billboardUBO");

    std::vector<Vertex> vertices = {
        {{-1.0, -1.0f, 0.0f}, {0.0f, 1.0f}, {}},
        {{1.0, -1.0f, 0.0f}, {1.0f, 1.0f}, {}},
        {{1.0, 1.0f, 0.0f}, {1.0f, 0.0f}, {}},

        {{1.0, 1.0f, 0.0f}, {1.0f, 0.0f}, {}},
        {{-1.0, 1.0f, 0.0f}, {0.0f, 0.0f}, {}},
        {{-1.0, -1.0f, 0.0f}, {0.0f, 1.0f}, {}},
    };

    uint32_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    graphics.createBuffer(vertexBuffer, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    graphics.uploadBuffer(vertexBuffer, vertices.data(), vertexBufferSize);

    uint32_t texturesSize = textures.size() > 0 ? textures.size() : 1; // use 1 to silent validation layers

    //
    // Descriptor set
    //
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, // ubo
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize)},
    };

    pool = vkutils::createDescriptorPool(device, poolSizes);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}, // ubo
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize), VK_SHADER_STAGE_FRAGMENT_BIT}, // textures
    };

    setLayout = vkutils::createDescriptorSetLayout(device, bindings.data(), bindings.size(), nullptr);
    set = vkutils::createDescriptorSet(device, pool, setLayout);

    DescriptorWriter writer;
    writer.write(0, uboBuffer.buffer, uboBuffer.size, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    std::vector<VkDescriptorImageInfo> textureInfos(textures.size());
    if (textures.size() > 0) {
        for (size_t i = 0; i < textures.size(); i++) {
            textureInfos[i].imageView = textures[i].image.view;
            textureInfos[i].sampler = textures[i].image.sampler;
            textureInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        writer.write(1, textureInfos.data(), textureInfos.size(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }
    writer.update(device, set);

    //
    // Pipeline
    //
    auto vertex = vkutils::loadShaderModule(device, "build/shaders/billboard.vert.spv");
    auto fragment = vkutils::loadShaderModule(device, "build/shaders/billboard.frag.spv");
    if (vertex == VK_NULL_HANDLE || fragment == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to load shaders.\n");
        exit(-1);
    }

    vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "billboard.vert");
    vkutils::setDebugName(device, (uint64_t)fragment, VK_OBJECT_TYPE_SHADER_MODULE, "billboard.frag");

    // create pipeline layout
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant)};
    layout = vkutils::createPipelineLayout(device, &setLayout, &pushConstant);

    // create pipeline
    PipelineBuilder builder;
    builder.setPipelineLayout(layout);
    builder.setShader(vertex, VK_SHADER_STAGE_VERTEX_BIT);
    builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.setCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    builder.setAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos));
    builder.setAttributeDescription(1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv));
    builder.setBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    pipeline = builder.build(device);
    vkutils::setDebugName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, "billboard pipeline");

    vkDestroyShaderModule(device, vertex, nullptr);
    vkDestroyShaderModule(device, fragment, nullptr);
}

void BillboardPass::shutdown(VulkanGraphics &graphics, VkDevice device)
{
    graphics.destroyBuffer(uboBuffer);
    graphics.destroyBuffer(vertexBuffer);

    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

void BillboardPass::beginFrame(VulkanGraphics &graphics, VkCommandBuffer cmd, Camera &camera)
{
    // update ubo
    UBO ubo;
    ubo.viewProj = camera.getProjection() * camera.getView();
    ubo.cameraRight = camera.getRight();
    ubo.cameraUp = camera.getUp();
    memcpy(uboBuffer.info.pMappedData, &ubo, sizeof(ubo));

    Image swapchainImage = {};
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);

    VkDeviceSize offset = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);
}

void BillboardPass::endFrame(VulkanGraphics &graphics, VkCommandBuffer cmd)
{
    graphics.endFrame(cmd);
}

void BillboardPass::render(VkCommandBuffer cmd, VkDevice device, vec3 center, vec2 size, int textureIndex)
{
    PushConstant push = {};
    push.center = center;
    push.size = size;
    push.textureIndex = textureIndex;
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

    vkCmdDraw(cmd, 6, 1, 0, 0);
}
