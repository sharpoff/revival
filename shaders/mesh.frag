#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "types.glsl"
#include "light.glsl"

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

vec2 poissonDisk[4] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760)
);

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

    vec3 color = vec3(0.0);

    // TODO: check light types(directional, point, spot)
    for (uint i = 0; i < ubo.numLights; i++) {
        Light light = lights[i];

        vec3 lightDir = normalize(light.pos - inWorldPos);
        vec3 viewDir = normalize(ubo.cameraPos - inWorldPos);
        float NoL = dot(normal, lightDir);

        // Shadow mapping
        vec4 lightSpace = light.mvp * vec4(inWorldPos, 1.0);
        vec3 ambient = 0.15 * light.color;
        vec3 projCoords = lightSpace.xyz / lightSpace.w;

        vec2 coords = (projCoords.xy * 0.5 + 0.5);
        float closestDepth = TEX(light.shadowMapIndex, coords).r;
        float currentDepth = projCoords.z;

        float bias = max(0.0005 * (1.0 - NoL), 0.0001);

        // poisson sampling
        float shadow = 1.0;
        if (light.shadowMapIndex > 0) {
            for (int i = 0; i < 4; i++) {
                if (TEX(light.shadowMapIndex, coords + poissonDisk[i] / 700.0).r  <  currentDepth - bias) {
                    shadow-=0.2;
                }
            }
        }

        // Lighting
        vec3 halfwayDir = normalize(lightDir + viewDir);

        vec3 lighting = blinnPhong(albedo, normal, halfwayDir) * light.color;
        color += lighting * (ambient + (1.0 - shadow));
    }

    color += emissive;

    fragColor = vec4(color, 1.0);
}
