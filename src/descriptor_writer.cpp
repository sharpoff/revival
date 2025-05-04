#include "descriptor_writer.h"

void DescriptorWriter::write(uint32_t binding, VkBuffer &buffer, uint32_t size, VkDescriptorType type)
{
    VkDescriptorBufferInfo &bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = 0,
        .range = size
    });

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // will be added on update
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &bufferInfo;

    writes.push_back(write);
}

void DescriptorWriter::write(uint32_t binding, VkImageView &imageView, VkSampler &sampler, VkImageLayout layout, VkDescriptorType type)
{
    VkDescriptorImageInfo &imageInfo = imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = imageView,
        .imageLayout = layout
    });

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // will be added on update
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &imageInfo;

    writes.push_back(write);
}

void DescriptorWriter::write(uint32_t binding, std::vector<VkDescriptorImageInfo> &infos, VkDescriptorType type)
{
    for (auto &info : infos) {
        imageInfos.push_back(info);
    }

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // will be added on update
    write.descriptorCount = infos.size();
    write.descriptorType = type;
    write.pImageInfo = infos.data();

    writes.push_back(write);
}

void DescriptorWriter::write(uint32_t binding, std::vector<VkDescriptorBufferInfo> &infos, VkDescriptorType type)
{
    for (auto &info : infos) {
        bufferInfos.push_back(info);
    }

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; // will be added on update
    write.descriptorCount = infos.size();
    write.descriptorType = type;
    write.pBufferInfo = infos.data();

    writes.push_back(write);
}

void DescriptorWriter::update(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet &write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}
