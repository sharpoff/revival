#pragma once
#include "device.h"
#include "scene.h"

class SolidRenderer
{
public:
    SolidRenderer(Camera &camera);

    void create(VulkanDevice &vkDevice);
    void destroy(VulkanDevice &vkDevice);
    void draw(Scene &scene, vec3 color, VulkanDevice &vkDevice, Buffer &vertexBuffer, Buffer &indexBuffer);
private:
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    struct PushConstant
    {
        mat4 mvp;
        vec3 color;
    };

    // reference from the engine
    Camera &camera;
};
