#version 450

layout (location = 0) out vec2 outUV;

layout (binding = 0) uniform PushConstant
{
    mat4 viewProj;
    vec3 cameraUp;
    vec3 cameraRight;
    vec3 center;
    vec2 size;
} push;

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 vertPos = outUV * 2.0f + -1.0f;

    // vec2 pos = 
    // gl_Position = 
}
