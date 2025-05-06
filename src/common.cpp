#include "common.h"
#include "types.h"
#include <fstream>

VkSemaphore createSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    VkSemaphore semaphore;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, nullptr, &semaphore));

    return semaphore;
}

VkFence createFence(VkDevice device, VkFenceCreateFlags flags)
{
    VkFenceCreateInfo createInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    createInfo.flags = flags;

    VkFence fence;
    VK_CHECK(vkCreateFence(device, &createInfo, nullptr, &fence));

    return fence;
}

void insertImageBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subresourceRange)
{
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = image;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = subresourceRange;

    vkCmdPipelineBarrier(cmd,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding> &bindings)
{
    VkDescriptorSetLayout layout;
    VkDescriptorSetLayoutCreateInfo layoutCI = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutCI.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &layout));

    return layout;
}

VkDescriptorSet createDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocateInfo.descriptorPool = pool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &set));
    return set;
}

VkDescriptorPool createDescriptorPool(VkDevice device, std::vector<VkDescriptorPoolSize> &poolSizes)
{
    VkDescriptorPoolCreateInfo createInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    createInfo.maxSets = 0;
    for (auto &poolSize : poolSizes) {
        createInfo.maxSets += poolSize.descriptorCount;
    }
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool descriptorPool;
    VK_CHECK(vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool));

    return descriptorPool;
}

void writeBuffer(VkDevice device, VkDescriptorSet set, uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkBuffer &buffer, VkDeviceSize size)
{
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = size;

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = descriptorCount;
    write.descriptorType = type;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, 0);
}

void writeImage(VkDevice device, VkDescriptorSet set, uint32_t binding, VkDescriptorType type, uint32_t descriptorCount, VkImageView imageView, VkSampler sampler)
{
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = descriptorCount;
    write.descriptorType = type;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, 0);
}

void writeImages(VkDevice device, VkDescriptorSet set, uint32_t binding, VkDescriptorType type, std::vector<VkDescriptorImageInfo> infos)
{
    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorCount = infos.size();
    write.descriptorType = type;
    write.pImageInfo = infos.data();

    vkUpdateDescriptorSets(device, 1, &write, 0, 0);
}


VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout)
{
    VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout);
    return layout;
}

VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout, VkPushConstantRange pushConstant)
{
    VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;

    VkPipelineLayout layout;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout);
    return layout;
}

VkShaderModule loadShaderModule(VkDevice device, const char *path)
{
    std::vector<char> spirv;
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        printf("Failed to open binary file - %s\n", path);
        return nullptr;
    }

    size_t size = file.tellg();
    spirv.resize(size);
    file.seekg(0);
    file.read(spirv.data(), size);
    file.close();

    VkShaderModuleCreateInfo shaderModuleInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleInfo.pCode = (uint32_t*)spirv.data();
    shaderModuleInfo.codeSize = spirv.size();

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &module));
    return module;
}

VkPipeline createGraphicsPipeline(VkDevice device, PipelineDescription &desc)
{
    VkPipelineVertexInputStateCreateInfo vertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputState.vertexAttributeDescriptionCount = desc.attributeDescriptions.size();
    vertexInputState.pVertexAttributeDescriptions = desc.attributeDescriptions.data();
    vertexInputState.vertexBindingDescriptionCount = desc.bindingDescriptions.size();
    vertexInputState.pVertexBindingDescriptions = desc.bindingDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyState.topology = desc.topology;

    VkPipelineTessellationStateCreateInfo tessellationState = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationState.lineWidth = 1.0;
    rasterizationState.polygonMode = desc.polygonMode;
    rasterizationState.cullMode = desc.cullMode;
    rasterizationState.frontFace = desc.cullFace;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;
    if (desc.depthTestEnabled) {
        depthStencilState.depthTestEnable = VK_TRUE;
        depthStencilState.depthWriteEnable = VK_TRUE;
        depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER;
    } else {
        depthStencilState.depthTestEnable = VK_FALSE;
        depthStencilState.depthWriteEnable = VK_FALSE;
        depthStencilState.depthCompareOp = VK_COMPARE_OP_NEVER;
        depthStencilState.depthBoundsTestEnable = VK_FALSE;
        depthStencilState.stencilTestEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =  VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.pDynamicStates = dynamicStates;
    dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);

    // shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    if (desc.vertex != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStageInfo.module = desc.vertex;
        shaderStageInfo.pName = "main";
        shaderStages.push_back(shaderStageInfo);
    }
    if (desc.fragment != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStageInfo.module = desc.fragment;
        shaderStageInfo.pName = "main";
        shaderStages.push_back(shaderStageInfo);
    }
    if (desc.tessellationControl != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        shaderStageInfo.module = desc.tessellationControl;
        shaderStageInfo.pName = "main";
        shaderStages.push_back(shaderStageInfo);
    }
    if (desc.tessellationEval != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo shaderStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        shaderStageInfo.module = desc.tessellationEval;
        shaderStageInfo.pName = "main";
        shaderStages.push_back(shaderStageInfo);
    }

    // dynamic rendering
    VkFormat colorAttachmentFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    if (desc.depthTestEnabled) {
        pipelineRenderingInfo.depthAttachmentFormat = depthAttachmentFormat;
        // pipelineRenderingInfo.stencilAttachmentFormat = depthAttachmentFormat;
    }

    // create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.pNext = &pipelineRenderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineInfo.pTessellationState = &tessellationState;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState;
    pipelineInfo.pMultisampleState = &multisampleState;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = desc.pipelineLayout;
    pipelineInfo.renderPass = nullptr; // dynamic rendering

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, 0, 1, &pipelineInfo, nullptr, &pipeline));

    if (desc.vertex != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, desc.vertex, nullptr);
    if (desc.fragment != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, desc.fragment, nullptr);
    if (desc.tessellationControl != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, desc.tessellationControl, nullptr);
    if (desc.tessellationEval != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, desc.tessellationEval, nullptr);

    return pipeline;
}

void setDebugName(VkDevice device, uint64_t objectHandle, VkObjectType objectType, const char *name)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    objectNameInfo.objectHandle = objectHandle;
    objectNameInfo.objectType = objectType;
    objectNameInfo.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
}

void beginDebugLabel(VkCommandBuffer cmd, const char *name, vec4 color)
{
    VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    label.color[0] = color[0], label.color[1] = color[1];
    label.color[2] = color[2], label.color[3] = color[3];
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}

void endDebugLabel(VkCommandBuffer cmd)
{
    vkCmdEndDebugUtilsLabelEXT(cmd);
}
