#pragma once

#include <volk.h>
#include <vector>
#include "math/math.h"

struct Vertex
{
    alignas(16) vec3 pos;
    alignas(16) vec2 uv;
    alignas(16) vec3 normal;
};

// should match the shader
struct Material
{
    int albedoId = -1;
    int specularId = -1;
    int normalId = -1;
    int emissiveId = -1;

    alignas(16) vec4 albedoFactor = vec4(1.0);
    float specularFactor = 0.0;
    alignas(16) vec3 emissiveFactor = vec3(0.0);
};

// should match the shader
struct Light
{
    alignas(16) vec3 position;
    alignas(16) vec3 color;
};

struct Mesh
{
    mat4 matrix;

    int indexOffset;
    int indexCount;

    int materialIndex = -1;
};

struct Scene
{
    mat4 matrix = mat4(1.0);

    std::vector<Mesh> meshes;
};
