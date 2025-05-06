#include "renderers/solid_renderer.h"

// reference from the engine
SolidRenderer::SolidRenderer(Camera &camera)
    : camera(camera)
{}

void SolidRenderer::create(VulkanDevice &vkDevice)
{
    VkDevice device = vkDevice.getDevice();

    // create pipeline
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant)};
    VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pPushConstantRanges = &pushConstant;
    layoutInfo.pushConstantRangeCount = 1;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);

    PipelineDescription pipelineDesc;
    pipelineDesc.pipelineLayout = pipelineLayout;
    pipelineDesc.vertex = loadShaderModule(device, "build/shaders/solid.vert.spv");
    setDebugName(device, (uint64_t)pipelineDesc.vertex, VK_OBJECT_TYPE_SHADER_MODULE, "solid.vert");
    pipelineDesc.fragment = loadShaderModule(device, "build/shaders/solid.frag.spv");
    setDebugName(device, (uint64_t)pipelineDesc.fragment, VK_OBJECT_TYPE_SHADER_MODULE, "solid.frag");
    pipelineDesc.depthTestEnabled = true;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.cullFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineDesc.attributeDescriptions = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
    };
    pipelineDesc.bindingDescriptions = {
        {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };

    pipeline = createGraphicsPipeline(device, pipelineDesc);
    setDebugName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, "solid color pipeline");
}

void SolidRenderer::destroy(VulkanDevice &vkDevice)
{
    VkDevice device = vkDevice.getDevice();

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
}

void SolidRenderer::draw(Scene &scene, vec3 color, VulkanDevice &vkDevice, Buffer &vertexBuffer, Buffer &indexBuffer)
{
    VkCommandBuffer cmd = vkDevice.getCommandBuffer();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offset = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    mat4 projection = math::perspective(glm::radians(60.0f), float(vkDevice.getSwapchainExtent().width) / vkDevice.getSwapchainExtent().height, 0.1f, 100.0f);
    mat4 view = glm::inverse(glm::translate(mat4(1.0f), camera.position) * glm::toMat4(camera.rotation));

    PushConstant push;
    for (auto &mesh : scene.meshes) {
        push.mvp = projection * view * (scene.modelMatrix * mesh.matrix);
        push.color = color;

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }
}
