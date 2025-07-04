#include <revival/scene_manager.h>
#include <revival/fs.h>

#include <revival/vulkan/graphics.h>

Scene &SceneManager::loadScene(std::string name, std::filesystem::path path)
{
    sceneMap[name] = loadModel(path);

    return sceneMap[name];
}

Scene &SceneManager::loadModel(std::filesystem::path path)
{
    const aiScene *aScene = aiImportFile(path.c_str(), aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_FlipUVs);

    if (aScene == nullptr) {
        printf("Failed to load model %s - %s\n", path.c_str(), aiGetErrorString());
        exit(EXIT_FAILURE);
    }

    std::filesystem::path dir = path.parent_path();

    uint32_t materialOffset = materials.size();

    // load materials
    for (uint32_t i = 0; i < aScene->mNumMaterials; i++) {
        aiMaterial *m = aScene->mMaterials[i];
        Material material;
        // TODO
        // material.albedoFactor;
        // material.emissiveFactor;

        float factor;

        if (aiGetMaterialFloat(m, AI_MATKEY_SPECULAR_FACTOR, &factor) == AI_SUCCESS) {
            material.specularFactor = factor;
        }

        aiString texPath;
        if (aiGetMaterialTexture(m, aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            material.albedoId = texturePaths.size();
            texturePaths.push_back(dir / texPath.data);
        }

        if (aiGetMaterialTexture(m, aiTextureType_SPECULAR, 0, &texPath) == AI_SUCCESS) {
            material.specularId = texturePaths.size();
            texturePaths.push_back(dir / texPath.data);
        }

        if (aiGetMaterialTexture(m, aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
            material.normalId = texturePaths.size();
            texturePaths.push_back(dir / texPath.data);
        }

        if (aiGetMaterialTexture(m, aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS) {
            material.emissiveId = texturePaths.size();
            texturePaths.push_back(dir / texPath.data);
        }

        materials.push_back(material);
    }

    Scene &scene = scenes.emplace_back();
    processNode(scene, aScene, aScene->mRootNode, dir, materialOffset);

    return scene;
}

void SceneManager::processNode(Scene &scene, const aiScene *aScene, const aiNode *aNode, std::filesystem::path directory, uint32_t materialOffset)
{
    mat4 nodeMatrix = mat4(1.0);
    if (aNode->mParent)
        nodeMatrix = toGlm(aNode->mParent->mTransformation) * toGlm(aNode->mTransformation);
    else
        nodeMatrix = toGlm(aNode->mTransformation);

    for (unsigned int i = 0; i < aNode->mNumMeshes; i++) {
        const aiMesh *aMesh = aScene->mMeshes[aNode->mMeshes[i]];

        Mesh mesh = processMesh(aScene, aMesh, directory, materialOffset);
        mesh.matrix = nodeMatrix;

        scene.meshes.push_back(mesh);
    }

    for (unsigned int i = 0 ; i < aNode->mNumChildren; i++) {
        processNode(scene, aScene, aNode->mChildren[i], directory, materialOffset);
    }
}

Mesh SceneManager::processMesh(const aiScene *aiScene, const aiMesh *aiMesh, std::filesystem::path directory, uint32_t materialOffset)
{
    Mesh mesh = {};

    uint32_t indexOffset = indices.size();
    uint32_t vertexOffset = vertices.size();
    uint32_t indexCount = 0;

    for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.pos.x = aiMesh->mVertices[i].x;
        vertex.pos.y = aiMesh->mVertices[i].y;
        vertex.pos.z = aiMesh->mVertices[i].z;

        if (aiMesh->HasNormals()) {
            vertex.normal.x = aiMesh->mNormals[i].x;
            vertex.normal.y = aiMesh->mNormals[i].y;
            vertex.normal.z = aiMesh->mNormals[i].z;
        } else {
            vertex.normal = vec3(0);
        }

        if (aiMesh->HasTextureCoords(0)) {
            vertex.uv.x = aiMesh->mTextureCoords[0][i].x;
            vertex.uv.y = aiMesh->mTextureCoords[0][i].y;
        } else {
            vertex.uv = vec2(0);
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
        aiFace aFace = aiMesh->mFaces[i];
        for (unsigned int j = 0; j < aFace.mNumIndices; j++) {
            indices.push_back(aFace.mIndices[j] + vertexOffset);
            indexCount++;
        }
    }

    mesh.indexOffset = indexOffset;
    mesh.indexCount = indexCount;
    mesh.materialIndex = aiMesh->mMaterialIndex >= 0 ? aiMesh->mMaterialIndex + materialOffset : -1;

    return mesh;
}

mat4 SceneManager::toGlm(const aiMatrix4x4 &m)
{
    glm::mat4 mat;
    mat[0][0] = m.a1; mat[1][0] = m.a2; mat[2][0] = m.a3; mat[3][0] = m.a4;
    mat[0][1] = m.b1; mat[1][1] = m.b2; mat[2][1] = m.b3; mat[3][1] = m.b4;
    mat[0][2] = m.c1; mat[1][2] = m.c2; mat[2][2] = m.c3; mat[3][2] = m.c4;
    mat[0][3] = m.d1; mat[1][3] = m.d2; mat[2][3] = m.d3; mat[3][3] = m.d4;

    return mat;
}

uint32_t SceneManager::addTexture(VulkanGraphics &graphics, std::string filename)
{
    uint32_t index = textures.size();
    Texture &texture = textures.emplace_back();

    TextureInfo info;
    graphics.loadTextureInfo(info, filename.c_str());
    graphics.createTexture(texture, info, VK_FORMAT_R8G8B8A8_SRGB);
    textureMap[filename] = index;

    return index;
}

uint32_t SceneManager::getTextureIndexByFilename(std::string filename)
{
    return textureMap[filename];
}

Texture &SceneManager::getTextureByIndex(uint32_t index)
{
    return textures[index];
}

uint32_t SceneManager::addMaterial(Material material)
{
    uint32_t index = materials.size();
    materials.push_back(material);
    return index;
}

uint32_t SceneManager::addLight(Light light)
{
    uint32_t index = lights.size();
    lights.push_back(light);
    return index;
}

uint32_t SceneManager::addBillboard(Billboard billboard)
{
    uint32_t index = billboards.size();
    billboards.push_back(billboard);
    return index;
}
