#version 450

#extension GL_GOOGLE_include_directive : enable

#include "types.glsl"

layout (binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
};

layout (binding = 1) uniform UBO
{
    mat4 projection;
    mat4 view;
    uint numLights;
    vec3 cameraPos;
} ubo;

layout (push_constant) uniform PushConstant
{
    mat4 model;
    int materialIndex;
} push;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outWorldPos;

void main()
{
    Vertex vertex = vertices[gl_VertexIndex];

    vec4 worldPos = push.model * vec4(vertex.pos, 1.0);
    gl_Position = ubo.projection * ubo.view * worldPos;
    outNormal = normalize(vertex.normal);
    outUV = vertex.uv;
    outWorldPos = vec3(worldPos);
}
