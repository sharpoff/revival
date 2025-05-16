#version 450

#extension GL_GOOGLE_include_directive : enable

#include "types.glsl"

layout (binding = 0) readonly buffer Vertices
{
    Vertex vertices[];
};

layout (push_constant) uniform PushConstant
{
    mat4 mvp;
} push;

void main()
{
    Vertex vertex = vertices[gl_VertexIndex];

    gl_Position = push.mvp * vec4(vertex.pos, 1.0);
}
