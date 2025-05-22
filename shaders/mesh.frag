#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "types.glsl"

layout (binding = 1) uniform UBO
{
    mat4 projection;
    mat4 view;
    uint numLights;
    vec3 cameraPos;
} ubo;

layout (binding = 2) readonly buffer MaterialData
{
    Material materials[];
};

layout (binding = 3) readonly buffer LightData
{
    Light lights[];
};

layout (binding = 4) uniform sampler2D textures[];

layout (push_constant) uniform PushConstant
{
    mat4 model;
    int materialIndex;
} push;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inWorldPos;

layout (location = 0) out vec4 fragColor;

#define TEX(id, uv) texture(textures[nonuniformEXT(id)], uv)

void main()
{
    vec3 albedo = vec3(1.0);
    float specular = 0.5;
    vec3 emissive = vec3(0.0);
    vec3 normal = inNormal;

    if (push.materialIndex > -1) {
        Material material = materials[push.materialIndex];

        albedo = material.albedoFactor.rgb;
        if (material.albedoTexture > -1)
            albedo = TEX(material.albedoTexture, inUV).rgb;

        specular = material.specularFactor;
        if (material.specularTexture > -1)
            specular = TEX(material.specularTexture, inUV).r;

        emissive = material.emissiveFactor;
        if (material.emissiveTexture > -1)
            emissive = TEX(material.emissiveTexture, inUV).rgb;

        if (material.normalTexture > -1)
            normal = TEX(material.normalTexture, inUV).rgb;
    }

    vec3 albedoOut = albedo;
    vec3 diffuseOut = vec3(0.0);
    vec3 specularOut = vec3(0.0);

    float shadowOut = 1.0;

    for (uint i = 0; i < ubo.numLights; i++) {
        Light light = lights[i];

        // Lighting
        vec3 lightDir = normalize(light.pos - inWorldPos);
        vec3 viewDir = normalize(ubo.cameraPos - inWorldPos);
        vec3 halfwayDir = normalize(lightDir + viewDir); // blinn-phong

        diffuseOut += max(dot(normal, lightDir), 0.1) * light.color;
        specularOut += specular * 0.5 * pow(max(dot(viewDir, halfwayDir), 0.0), 32) * light.color;

        // Shadow
        vec4 lightSpace = light.mvp * vec4(inWorldPos, 1.0);
        vec3 ambient = 0.15 * light.color;
        vec3 projCoords = lightSpace.xyz / lightSpace.w;

        vec2 coords = (projCoords.xy * 0.5 + 0.5);
        float closestDepth = TEX(light.shadowMapIndex, coords).r;
        float currentDepth = projCoords.z;

        float bias = 0.0005;
        // float bias = max(0.05 * (dot(normal, lightDir)), 0.005);
        float shadow = currentDepth + bias > closestDepth ? 1.0 : 0.5;
        if (currentDepth + bias >= 1.0 && currentDepth + bias <= closestDepth)
            shadow = 0.0;
        
        shadowOut *= shadow;
    }

    vec3 color = albedoOut * (diffuseOut + specularOut + emissive) * shadowOut;

    fragColor = vec4(color, 1.0);
}
