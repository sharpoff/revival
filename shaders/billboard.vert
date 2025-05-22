#version 450

layout (binding = 0) uniform UBO
{
    mat4 viewProj;
    vec3 cameraUp;
    vec3 cameraRight;
    vec3 center;
    vec2 size;
    int textureIndex;
} ubo;

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec2 outUV;
layout (location = 1) out int textureIndex;

void main()
{
    vec3 pos = ubo.center + ubo.cameraRight * inPos.x * ubo.size.x + ubo.cameraUp * inPos.y * ubo.size.y;

    gl_Position = ubo.viewProj * vec4(pos, 1.0);
    outUV = inUV;
    textureIndex = ubo.textureIndex;
}
