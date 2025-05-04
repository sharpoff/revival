#include "mesh_renderer.h"
#include "descriptor_writer.h"

// references to engine lights and materials
MeshRenderer::MeshRenderer(std::vector<Material> &materials, std::vector<Light> &lights, Camera &camera)
    : lights(lights), materials(materials), camera(camera)
{}

void MeshRenderer::create(VulkanDevice &vkDevice, std::vector<Texture> &textures)
{
    VkDevice device = vkDevice.getDevice();

    vkDevice.createBuffer(uboBuffer, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    setDebugName(device, (uint64_t)uboBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "uboBuffer");
    vkDevice.createBuffer(materialsBuffer, materials.size() * sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    setDebugName(device, (uint64_t)materialsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "materialsBuffer");
    vkDevice.createBuffer(lightsBuffer, lights.size() * sizeof(Light), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    setDebugName(device, (uint64_t)lightsBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "lightsBuffer");

    // create descriptors
    // use 1 to silent validation layers
    size_t texturesSize = textures.size() > 0 ? textures.size() : 1;
    size_t materialsSize = materials.size() > 0 ? materials.size() : 1;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize), VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(materialsSize), VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(lights.size()), VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    

    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(texturesSize)},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(materials.size() + lights.size())},
    };

    descriptorPool = createDescriptorPool(device, poolSizes);
    descriptorSetLayout = createDescriptorSetLayout(device, bindings);
    descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout);

    DescriptorWriter writer;
    writer.write(0, uboBuffer.buffer, uboBuffer.size, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    std::vector<VkDescriptorImageInfo> textureInfos(textures.size());
    if (textures.size() > 0) {
        for (size_t i = 0; i < textures.size(); i++) {
            textureInfos[i].imageView = textures[i].imageView;
            textureInfos[i].sampler = textures[i].sampler;
            textureInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        writer.write(1, textureInfos, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    }

    std::vector<VkDescriptorBufferInfo> materialInfos(materials.size());
    if (materials.size() > 0) {
        for (size_t i = 0; i < materials.size(); i++) {
            materialInfos[i].buffer = materialsBuffer.buffer;
            materialInfos[i].offset = 0;
            materialInfos[i].range = materialsBuffer.size;
        }
        writer.write(2, materialInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    std::vector<VkDescriptorBufferInfo> lightInfos(lights.size());
    if (lights.size() > 0) {
        for (size_t i = 0; i < lights.size(); i++) {
            lightInfos[i].buffer = lightsBuffer.buffer;
            lightInfos[i].offset = 0;
            lightInfos[i].range = lightsBuffer.size;
        }
        writer.write(3, lightInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    writer.update(device, descriptorSet);

    // create pipeline
    VkPushConstantRange pushConstant = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPush)}; // mesh push constant
    pipelineLayout = createPipelineLayout(device, descriptorSetLayout, pushConstant);

    PipelineDescription pipelineDesc;
    pipelineDesc.pipelineLayout = pipelineLayout;
    pipelineDesc.vertex = loadShaderModule(device, "build/shaders/mesh.vert.spv");
    setDebugName(device, (uint64_t)pipelineDesc.vertex, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.vert");
    pipelineDesc.fragment = loadShaderModule(device, "build/shaders/mesh.frag.spv");
    setDebugName(device, (uint64_t)pipelineDesc.fragment, VK_OBJECT_TYPE_SHADER_MODULE, "mesh.frag");
    pipelineDesc.depthTestEnabled = true;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.cullFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineDesc.attributeDescriptions = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
    };
    pipelineDesc.bindingDescriptions = {
        {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };

    pipeline = createGraphicsPipeline(device, pipelineDesc);
    setDebugName(device, (uint64_t)pipeline, VK_OBJECT_TYPE_PIPELINE, "mesh pipeline");

    updateDynamicBuffers(vkDevice, camera);
}

void MeshRenderer::destroy(VulkanDevice &vkDevice)
{
    vkDevice.destroyBuffer(uboBuffer);
    vkDevice.destroyBuffer(materialsBuffer);
    vkDevice.destroyBuffer(lightsBuffer);

    VkDevice device = vkDevice.getDevice();

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}

void MeshRenderer::draw(Scene &scene, VulkanDevice &vkDevice, Buffer &vertexBuffer, Buffer &indexBuffer)
{
    updateDynamicBuffers(vkDevice, camera);

    VkCommandBuffer cmd = vkDevice.getCommandBuffer();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    VkDeviceSize offset = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    MeshPush push;
    for (auto &mesh : scene.meshes) {
        push.model = scene.modelMatrix * mesh.matrix;
        push.materialIndex = mesh.materialIndex;

        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPush), &push);

        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }
}

void MeshRenderer::updateDynamicBuffers(VulkanDevice &vkDevice, Camera &camera)
{
    ubo.projection = math::perspective(glm::radians(60.0f), float(vkDevice.getSwapchainExtent().width) / vkDevice.getSwapchainExtent().height, 0.1f, 100.0f);
    ubo.view = glm::inverse(glm::translate(mat4(1.0f), camera.position) * glm::toMat4(camera.rotation));
    ubo.numLights = lights.size();
    ubo.cameraPos = camera.position;
    memcpy(uboBuffer.mapped, &ubo, sizeof(ubo));

    if (materials.size() > 0)
        memcpy(materialsBuffer.mapped, materials.data(), materialsBuffer.size);
    if (lights.size() > 0)
        memcpy(lightsBuffer.mapped, lights.data(), lightsBuffer.size);
}
