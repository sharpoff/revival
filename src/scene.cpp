#include "scene.h"
#include <stdio.h>
#include <string.h>

bool loadScene(Scene *scene, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices, std::vector<std::filesystem::path> &texturePaths, std::vector<Material> &materials, std::vector<Light> &lights, std::filesystem::path file)
{
    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, file.c_str(), &data);
    if (result != cgltf_result_success) {
        printf("Failed to load gltf scene.\n");
        return false;
    }
    result = cgltf_load_buffers(&options, data, file.c_str());

    size_t textureOffset = texturePaths.size();
    size_t materialOffset = materials.size();

    // load materials
    for (size_t i = 0; i < data->materials_count; i++) {
        cgltf_material material = data->materials[i];
        Material m;
        m.specularFactor = 1.0;

        if (material.has_pbr_metallic_roughness) {
            if (material.pbr_metallic_roughness.base_color_texture.texture)
                m.albedoTexture = textureOffset + cgltf_texture_index(data, material.pbr_metallic_roughness.base_color_texture.texture);

            m.albedoFactor = vec4(material.pbr_metallic_roughness.base_color_factor[0], material.pbr_metallic_roughness.base_color_factor[1], material.pbr_metallic_roughness.base_color_factor[2], material.pbr_metallic_roughness.base_color_factor[3]);

            if (material.pbr_metallic_roughness.metallic_roughness_texture.texture) {
                m.specularTexture = textureOffset + cgltf_texture_index(data, material.pbr_metallic_roughness.metallic_roughness_texture.texture);

                m.specularFactor = material.pbr_metallic_roughness.metallic_factor;
            }
        }

        if (material.normal_texture.texture) {
            m.normalTexture = textureOffset + cgltf_texture_index(data, material.normal_texture.texture);
        }

        if (material.emissive_texture.texture) {
            m.emissiveTexture = textureOffset + cgltf_texture_index(data, material.emissive_texture.texture);

            m.emissiveFactor = vec3(material.emissive_factor[0], material.emissive_factor[1], material.emissive_factor[2]);
        }

        materials.push_back(m);
    }

    // load textures
    for (size_t i = 0; i < data->textures_count; i++) {
        cgltf_texture texture = data->textures[i];

        std::filesystem::path path = file.parent_path() / texture.image->uri;
        texturePaths.push_back(path);
    }

    // load scene
    cgltf_scene *root = data->scene;
    for (size_t i = 0; i < root->nodes_count; i++) {
        cgltf_node *node = root->nodes[i];
        loadNode(scene, vertices, indices, lights, data, node, materialOffset);

        for (size_t j = 0; j < node->children_count; j++) {
            loadNode(scene, vertices, indices, lights, data, node, materialOffset);
        }
    }

    cgltf_free(data);

    return true;
}

void loadNode(Scene *scene, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices, std::vector<Light> &lights, cgltf_data *data, cgltf_node *node, uint32_t materialOffset)
{

    if (node->mesh) {
        float m[16];
        cgltf_node_transform_world(node, m);
        mat4 matrix = glm::make_mat4(m);
        loadMesh(scene, vertices, indices, matrix, data, node->mesh, materialOffset);
    }

    if (node->light && node->light->type == cgltf_light_type_directional) {
        float m[16];
        cgltf_node_transform_world(node, m);
        mat4 matrix = glm::make_mat4(m);
        vec3 pos = vec3(matrix[0][3], matrix[1][3], matrix[2][3]);
        scene->lights.push_back({pos, glm::make_vec3(node->light->color)});
        lights.push_back({pos, glm::make_vec3(node->light->color)});
    }

    for (size_t i = 0; i < node->children_count; i++) {
        loadNode(scene, vertices, indices, lights, data, node->children[i], materialOffset);
    }
}

void loadMesh(Scene *scene, std::vector<Vertex> &vertices, std::vector<uint32_t> &indices, mat4 nodeMatrix, cgltf_data *data, cgltf_mesh *gltfMesh, uint32_t materialOffset)
{
    for (size_t i = 0; i < gltfMesh->primitives_count; i++) {
        cgltf_primitive prim = gltfMesh->primitives[i];

        // load vertices
        size_t vertexOffset = vertices.size();

        std::vector<Vertex> meshVertices(prim.attributes[0].data->count);
        size_t vertexCount = meshVertices.size();
        std::vector<float> temp(vertexCount * 4);

        if (const cgltf_accessor *pos = cgltf_find_accessor(&prim, cgltf_attribute_type_position, 0)) {
            assert(cgltf_num_components(pos->type) == 3);
            cgltf_accessor_unpack_floats(pos, temp.data(), vertexCount * 3);

            for (size_t i = 0; i < vertexCount; i++) {
                meshVertices[i].pos.x = temp[i * 3 + 0];
                meshVertices[i].pos.y = temp[i * 3 + 1];
                meshVertices[i].pos.z = temp[i * 3 + 2];
            }
        }

        if (const cgltf_accessor *uv = cgltf_find_accessor(&prim, cgltf_attribute_type_texcoord, 0)) {
            assert(cgltf_num_components(uv->type) == 2);
            cgltf_accessor_unpack_floats(uv, temp.data(), vertexCount * 2);

            for (size_t i = 0; i < vertexCount; i++) {
                meshVertices[i].uv.x = temp[i * 2 + 0];
                meshVertices[i].uv.y = temp[i * 2 + 1];
            }
        }

        if (const cgltf_accessor *normal = cgltf_find_accessor(&prim, cgltf_attribute_type_normal, 0)) {
            assert(cgltf_num_components(normal->type) == 3);
            cgltf_accessor_unpack_floats(normal, temp.data(), vertexCount * 3);

            for (size_t i = 0; i < vertexCount; i++) {
                meshVertices[i].normal.x = temp[i * 3 + 0];
                meshVertices[i].normal.y = temp[i * 3 + 1];
                meshVertices[i].normal.z = temp[i * 3 + 2];
            }
        }

        // load meshIndices
        std::vector<uint32_t> meshIndices(prim.indices->count);
        cgltf_accessor_unpack_indices(prim.indices, meshIndices.data(), 4, meshIndices.size());
        for (auto &index : meshIndices) {
            index += vertexOffset;
        }

        Mesh mesh;
        mesh.indexCount = meshIndices.size();
        mesh.indexOffset = indices.size();
        mesh.matrix = nodeMatrix;
        if (prim.material)
            mesh.materialIndex = cgltf_material_index(data, prim.material) >= 0 ? materialOffset + cgltf_material_index(data, prim.material) : -1;

        vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
        indices.insert(indices.end(), meshIndices.begin(), meshIndices.end());
        scene->meshes.push_back(mesh);
    }
}
