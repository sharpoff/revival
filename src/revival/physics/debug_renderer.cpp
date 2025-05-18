#include <revival/physics/debug_renderer.h>
#include <revival/vulkan/graphics.h>
#include <revival/vulkan/pipeline_builder.h>
#include <revival/physics/helpers.h>

using namespace JPH;

void DebugRendererImp::init(VulkanGraphics &graphics)
{
    VkDevice device = graphics.getDevice();
    this->graphics = &graphics;

    //
    // Pipelines
    //
    auto vertexLine = vkutils::loadShaderModule(device, "build/shaders/debug_line.vert.spv");
    auto fragment = vkutils::loadShaderModule(device, "build/shaders/stub.frag.spv");

    // line pipeline layout
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(LinePushConstant)};
    VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &line.layout);

    // triangle pipeline layout
    pushConstant = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(TrianglePushConstant)};
    layoutInfo.pPushConstantRanges = &pushConstant;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &triangle.layout);

    //
    // Line pipeline
    //
    PipelineBuilder builder;
    builder.setPipelineLayout(line.layout);
    builder.setShader(vertexLine, VK_SHADER_STAGE_VERTEX_BIT);
    builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.setDepthTest(false);
    builder.setCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setPolygonMode(VK_POLYGON_MODE_LINE);
    line.pipeline = builder.build(device);

    vkDestroyShaderModule(device, vertexLine, nullptr);

    //
    // Triangle pipeline
    //
    auto vertexTriangle = vkutils::loadShaderModule(device, "build/shaders/debug_triangle.vert.spv");

    builder.clearShaders();
    builder.setShader(vertexTriangle, VK_SHADER_STAGE_VERTEX_BIT);
    builder.setShader(fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
    builder.setCulling(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    triangle.pipeline = builder.build(device);

    vkDestroyShaderModule(device, vertexTriangle, nullptr);
    vkDestroyShaderModule(device, fragment, nullptr);
}

void DebugRendererImp::shutdown(VkDevice device)
{
    vkDestroyPipelineLayout(device, line.layout, nullptr);
    vkDestroyPipeline(device, line.pipeline, nullptr);

    vkDestroyPipelineLayout(device, triangle.layout, nullptr);
    vkDestroyPipeline(device, triangle.pipeline, nullptr);
}

void DebugRendererImp::setDrawState(VkCommandBuffer cmd)
{
    this->cmd = cmd;
}

void DebugRendererImp::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, line.pipeline);
    LinePushConstant push = {
        .from = toGlm(inFrom),
        .to = toGlm(inTo),
    };
    vkCmdPushConstants(cmd, line.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

    vkCmdDraw(cmd, 2, 1, 0, 0);
}

void DebugRendererImp::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, triangle.pipeline);
    TrianglePushConstant push = {
        .vert = {toGlm(inV1), toGlm(inV2), toGlm(inV3)},
    };
    vkCmdPushConstants(cmd, triangle.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

    vkCmdDraw(cmd, 3, 1, 0, 0);

}

void DebugRendererImp::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, JPH::ColorArg inColor, float inHeight)
{
}
