#include <revival/passes/shadow_debug_pass.h>
#include <revival/vulkan/utils.h>
#include <revival/vulkan/graphics.h>
#include <revival/scene_manager.h>
#include <revival/vulkan/pipeline_builder.h>
#include <revival/vulkan/descriptor_writer.h>

void ShadowDebugPass::init(VulkanGraphics &graphics, Buffer &vertexBuffer)
{
    VkDevice device = graphics.getDevice();

    //
    // Descriptor set
    //
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    pool = vkutils::createDescriptorPool(device, poolSizes);

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
    };

    setLayout = vkutils::createDescriptorSetLayout(device, bindings.data(), bindings.size(), nullptr);
    set = vkutils::createDescriptorSet(device, pool, setLayout);

    //
    // Pipeline
    //
    auto vertex = vkutils::loadShaderModule(device, "build/shaders/quad.vert.spv");
    auto fragment = vkutils::loadShaderModule(device, "build/shaders/shadow_debug.frag.spv");
    vkutils::setDebugName(device, (uint64_t)vertex, VK_OBJECT_TYPE_SHADER_MODULE, "quad.vert");
    vkutils::setDebugName(device, (uint64_t)fragment, VK_OBJECT_TYPE_SHADER_MODULE, "shadow_debug.frag");

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
    pipeline = builder.build(device, 0, true);
    vkutils::setDebugName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, "shadow debug pipeline");

    vkDestroyShaderModule(device, vertex, nullptr);
    vkDestroyShaderModule(device, fragment, nullptr);
}

void ShadowDebugPass::shutdown(VkDevice device)
{
    vkDestroyPipelineLayout(device, layout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

void ShadowDebugPass::render(VulkanGraphics &graphics, VkCommandBuffer cmd, Image &shadowMap)
{
    // XXX: this is not very performant, updated every render.
    DescriptorWriter writer;
    VkDescriptorImageInfo textureInfo = {};
    textureInfo.imageView = shadowMap.view;
    textureInfo.sampler = shadowMap.sampler;
    textureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    writer.write(0, &textureInfo, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update(graphics.getDevice(), set);

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

    // NOTE: Fullscreen quad that is made of clipped triangle. See quad vertex shader.
    vkCmdDraw(cmd, 3, 1, 0, 0);

    graphics.endFrame(cmd);
}
