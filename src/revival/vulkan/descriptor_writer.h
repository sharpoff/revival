#pragma once

#include <volk.h>
#include <deque>
#include <vector>

class DescriptorWriter
{
public:
    void write(uint32_t binding, VkBuffer &buffer, uint32_t size, VkDescriptorType type);
    void write(uint32_t binding, VkImageView &imageView, VkSampler &sampler, VkImageLayout layout, VkDescriptorType type);
    void write(uint32_t binding, VkDescriptorImageInfo *infos, uint32_t infosSize, VkDescriptorType type);
    void write(uint32_t binding, VkDescriptorBufferInfo *infos, uint32_t infosSize, VkDescriptorType type);

    void update(VkDevice device, VkDescriptorSet set);
    void clear();

private:
    // to keep infos in the memory save them into deque
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;

    std::vector<VkWriteDescriptorSet> writes;
};
