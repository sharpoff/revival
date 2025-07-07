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
    mat4 mvp;
    vec3 pos;
    vec3 color;
    int shadowMapIndex;
};

struct Vertex
{
    vec3 pos;
    vec2 uv;
    vec3 normal;
};
