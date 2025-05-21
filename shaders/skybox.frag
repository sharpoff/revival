#version 450

layout (binding = 1) uniform samplerCube skybox;

layout (location = 0) in vec3 inUVW;
layout (location = 0) out vec4 fragColor;

void main()
{
    fragColor = texture(skybox, inUVW);
}
