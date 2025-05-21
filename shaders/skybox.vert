#version 450

layout (binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
} ubo;

layout (location = 0) in vec3 pos;
layout (location = 0) out vec3 outUVW;

void main()
{
    outUVW = pos;

    mat4 view = mat4(mat3(ubo.view)); // remove translation from view
    gl_Position = ubo.projection * view * vec4(pos, 1.0);
}
