#pragma once

#include <revival/vulkan/common.h>
#include <revival/vulkan/resources.h>
#include <revival/types.h>

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

class VulkanGraphics;

class DebugRendererImp : public JPH::DebugRendererSimple
{
public:
    void init(VulkanGraphics &graphics);
    void shutdown(VkDevice device);

    void setDrawState(VkCommandBuffer cmd);

    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;

    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;

    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, JPH::ColorArg inColor, float inHeight) override;

private:
    struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    } line;

    struct {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    } triangle;


    struct LinePushConstant
    {
        alignas(16) vec3 from;
        alignas(16) vec3 to;
    };

    struct TrianglePushConstant
    {
        alignas(16) vec3 vert[3];
    };

    VulkanGraphics *graphics;

    VkCommandBuffer cmd; // current command buffer, should be set with setDrawState before draw
};
