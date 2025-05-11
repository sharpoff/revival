#include <revival/vulkan/utils.h>
#include <fstream>

namespace vkutils
{
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

    VkPipelineLayout createPipelineLayout(VkDevice device, VkPushConstantRange pushConstant)
    {
        VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstant;

        VkPipelineLayout layout;
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout);
        return layout;
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

    void setViewport(VkCommandBuffer cmd, float x, float y, float width, float height)
    {
        VkViewport viewport = {};
        viewport.x = x;
        viewport.y = y;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    void setScissor(VkCommandBuffer cmd, VkExtent2D extent)
    {
        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

} // namespace vkutils
