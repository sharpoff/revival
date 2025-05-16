#version 450

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D depthMap;

void main()
{
    float depth = texture(depthMap, inUV).r;
    fragColor = vec4(vec3(depth), 1.0);
}
