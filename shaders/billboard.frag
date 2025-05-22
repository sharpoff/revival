#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 inUV;
layout (location = 1) flat in int textureIndex;

layout (location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D textures[];

void main()
{
    if (textureIndex > -1)
        fragColor = texture(textures[nonuniformEXT(textureIndex)], inUV);
    else
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);

    if (fragColor.a <= 0.5) discard;
}
