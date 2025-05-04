struct Material
{
    int albedoTexture;
    int specularTexture;
    int normalTexture;
    int emissiveTexture;

    vec4 albedoFactor;
    float specularFactor;
    vec3 emissiveFactor;
};

struct Light
{
    vec3 pos;
    vec3 color;
};
