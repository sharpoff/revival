#version 450

#extension GL_GOOGLE_include_directive : enable

#include "types.glsl"

layout (binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    uint numLights;
    vec3 cameraPos;
} ubo;

layout (push_constant) uniform PushConstants
{
    mat4 model;
    int materialIndex;
} push;

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outWorldPos;

void main()
{
    vec4 worldPos = push.model * vec4(pos, 1.0);
    gl_Position = ubo.projection * ubo.view * worldPos;
    outNormal = normal;
    outUV = uv;
    outWorldPos = vec3(worldPos);
}
