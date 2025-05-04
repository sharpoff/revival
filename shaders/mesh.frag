#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "types.glsl"

layout (binding = 0) uniform UBO
{
    mat4 projection;
    mat4 view;
    uint numLights;
    vec3 cameraPos;
} ubo;

layout (binding = 1) uniform sampler2D textures[];

layout (binding = 2) readonly buffer MaterialData
{
    Material materials[];
};

layout (binding = 3) readonly buffer LightData
{
    Light lights[];
};

layout (push_constant) uniform PushConstant
{
    mat4 model;
    int materialIndex;
} push;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inWorldPos;

layout (location = 0) out vec4 fragColor;

void main()
{
    vec3 albedo = vec3(1.0);
    float specular = 0.1;
    vec3 emissive = vec3(0.0);
    vec3 normal = normalize(inNormal);

    if (push.materialIndex > -1) {
        Material material = materials[push.materialIndex];

        albedo = material.albedoFactor.rgb;
        if (material.albedoTexture > -1)
            albedo = texture(textures[nonuniformEXT(material.albedoTexture)], inUV).rgb;

        specular = material.specularFactor;
        if (material.specularTexture > -1)
            specular = texture(textures[nonuniformEXT(material.specularTexture)], inUV).r;

        emissive = material.emissiveFactor;
        if (material.emissiveTexture > -1)
            emissive = texture(textures[nonuniformEXT(material.emissiveTexture)], inUV).rgb;

        if (material.normalTexture > -1)
            normal = texture(textures[nonuniformEXT(material.normalTexture)], inUV).rgb;
    }

    vec3 diffuse = vec3(0.0);
    vec3 spec = vec3(0.0);
    for (uint i = 0; i < ubo.numLights; i++) {
        Light light = lights[i];
        vec3 lightDir = normalize(light.pos - inWorldPos);
        vec3 viewDir = normalize(ubo.cameraPos - inWorldPos);
        vec3 reflectDir = reflect(-viewDir, normal);

        diffuse += max(dot(normal, lightDir), 0.1) * light.color;
        spec += pow(max(dot(viewDir, reflectDir), 0.0), 64) * 0.5;
    }

    vec3 color = diffuse * albedo + spec * specular + emissive;
    fragColor = vec4(color, 1.0);
}
