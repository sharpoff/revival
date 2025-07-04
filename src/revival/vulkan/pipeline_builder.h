#pragma once

#include <revival/vulkan/utils.h>

class PipelineBuilder
{
public:
    PipelineBuilder();
    void clear();

    void setBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate);
    void setAttributeDescription(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
    void setPipelineLayout(VkPipelineLayout &layout);
    void clearShaders();
    void setShader(VkShaderModule module, VkShaderStageFlagBits stage, VkSpecializationInfo *specializationInfo = nullptr);
    void setPolygonMode(VkPolygonMode polygonMode);
    void setCulling(VkCullModeFlags cullMode, VkFrontFace cullFace);
    void setDepthTest(bool mode);
    void setDepthBias(bool mode);

    void setTopology(VkPrimitiveTopology topology);
    void setPatchControlPoints(uint32_t points);

    VkPipeline build(VkDevice device, uint32_t colorAttachmentCount = 1, bool depthUsed = true);

private:
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    // states
    VkPipelineVertexInputStateCreateInfo vertexInputState;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    VkPipelineTessellationStateCreateInfo tessellationState;
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineRasterizationStateCreateInfo rasterizationState;
    VkPipelineMultisampleStateCreateInfo multisampleState;
    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    VkPipelineColorBlendStateCreateInfo colorBlendState;
    VkPipelineDynamicStateCreateInfo dynamicState;
    VkPipelineLayout pipelineLayout;
};
