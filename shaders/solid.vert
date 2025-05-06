#version 450

layout (push_constant) uniform PushConstant
{
    mat4 mvp;
    vec3 color;
} push;

layout (location = 0) in vec3 pos;
layout (location = 0) out vec3 outColor;

void main()
{
    gl_Position = push.mvp * vec4(pos, 1.0);
    outColor = push.color;
}
