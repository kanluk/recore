#include "scene.h"

#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <iostream>

#include <glm/gtc/type_ptr.hpp>

#include "ecs_components.h"

namespace recore::scene {

// NOLINTNEXTLINE(readability-function-cognitive-complexity) turn your brain on
void Scene::loadGLTF(const Scene::GLTFLoadDesc& desc) {
  // https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanglTFModel.cpp

  tinygltf::TinyGLTF loader{};
  tinygltf::Model gltfModel{};

  std::string errors;
  std::string warnings;

  auto resolvedPath = RECORE_ASSETS_DIR / desc.path;

  bool result = loader.LoadASCIIFromFile(
      &gltfModel, &errors, &warnings, resolvedPath.string());
  if (!result) {
    std::cerr << "Failed to load GLTF model " << resolvedPath
              << ". Err: " << errors << std::endl;
  }

  TransformComponent transformComponent;
  transformComponent.translation = desc.translation;
  transformComponent.scale = desc.scale;
  transformComponent.rotation = desc.rotation;
  auto modelMatrix = transformComponent.getMat4();

  mModelMatrices.push_back(modelMatrix);
  uint32_t modelMatrixID = mModelMatrices.size() - 1;
  if (desc.name) {
    auto entity = mECS.createEntity(*desc.name);
    entity.addComponent<TransformComponent>(transformComponent);
    entity.addComponent<SceneNodeComponent>(modelMatrixID);
  }

  uint32_t materialOffset = mMaterials.size();

  for (auto& gltfMaterial : gltfModel.materials) {
    uint32_t baseColorID = -1;
    uint32_t normalMapID = -1;
    uint32_t metallicRoughnessID = -1;

    if (gltfMaterial.pbrMetallicRoughness.baseColorTexture.index >= 0) {
      auto baseColorImage =
          gltfModel.images[gltfModel
                               .textures[gltfMaterial.pbrMetallicRoughness
                                             .baseColorTexture.index]
                               .source];

      Texture baseColor{};
      baseColor.width = baseColorImage.width;
      baseColor.height = baseColorImage.height;
      baseColor.image = baseColorImage.image;
      baseColor.format = Texture::Format::SRGB;
      baseColorID = mTextures.size();
      mTextures.push_back(baseColor);
    }

    if (gltfMaterial.normalTexture.index >= 0) {
      auto normalMapImage =
          gltfModel.images[gltfModel.textures[gltfMaterial.normalTexture.index]
                               .source];

      Texture normalMap{};
      normalMap.width = normalMapImage.width;
      normalMap.height = normalMapImage.height;
      normalMap.image = normalMapImage.image;
      normalMapID = mTextures.size();
      mTextures.push_back(normalMap);
    }

    if (gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
      auto metallicRoughnessTexture =
          gltfModel.images[gltfModel
                               .textures[gltfMaterial.pbrMetallicRoughness
                                             .metallicRoughnessTexture.index]
                               .source];

      Texture metallicRoughness{};
      metallicRoughness.width = metallicRoughnessTexture.width;
      metallicRoughness.height = metallicRoughnessTexture.height;
      metallicRoughness.image = metallicRoughnessTexture.image;
      metallicRoughnessID = mTextures.size();
      mTextures.push_back(metallicRoughness);
    }

    Material material{};
    material.baseColorID = baseColorID;
    material.normalMapID = normalMapID;
    material.metallicRoughnessID = metallicRoughnessID;
    material.baseColorFactor = glm::vec4(
        gltfMaterial.pbrMetallicRoughness.baseColorFactor[0],
        gltfMaterial.pbrMetallicRoughness.baseColorFactor[1],
        gltfMaterial.pbrMetallicRoughness.baseColorFactor[2],
        gltfMaterial.pbrMetallicRoughness.baseColorFactor[3]);
    material.emissiveFactor = glm::vec4(gltfMaterial.emissiveFactor[0],
                                        gltfMaterial.emissiveFactor[1],
                                        gltfMaterial.emissiveFactor[2],
                                        1.0);
    material.metallicFactor = static_cast<float>(
        gltfMaterial.pbrMetallicRoughness.metallicFactor);
    material.roughnessFactor = static_cast<float>(
        gltfMaterial.pbrMetallicRoughness.roughnessFactor);
    material.alphaMode = gltfMaterial.alphaMode == "OPAQUE"
                             ? MATERIAL_ALPHA_MODE_OPAQUE
                             : MATERIAL_ALPHA_MODE_MASK;
    material.ior = 1.f;

    // Handle GLTF extensions:

    // Emissive strenght: scale emissive factor components by another factor
    if (gltfMaterial.extensions.find("KHR_materials_emissive_strength") !=
        gltfMaterial.extensions.end()) {
      const tinygltf::Value& emissiveStrenghtExtension =
          gltfMaterial.extensions.at("KHR_materials_emissive_strength");
      if (emissiveStrenghtExtension.Has("emissiveStrength") &&
          emissiveStrenghtExtension.Get("emissiveStrength").IsNumber()) {
        material.emissiveFactor = glm::vec4(
            glm::vec3(material.emissiveFactor) *
                static_cast<float>(
                    emissiveStrenghtExtension.Get("emissiveStrength")
                        .Get<double>()),
            1.f);
      }
    }

    // Index of refraction for dielectric materials
    if (gltfMaterial.extensions.find("KHR_materials_ior") !=
        gltfMaterial.extensions.end()) {
      const tinygltf::Value& iorExtension = gltfMaterial.extensions.at(
          "KHR_materials_ior");
      if (iorExtension.Has("ior") && iorExtension.Get("ior").IsNumber()) {
        material.ior = static_cast<float>(
            iorExtension.Get("ior").Get<double>());
      }
    }

    mMaterials.push_back(material);
  }

  for (auto& gltfMesh : gltfModel.meshes) {
    for (auto& primitive : gltfMesh.primitives) {
      auto firstVertex = static_cast<uint32_t>(mVertices.size());
      auto firstIndex = static_cast<uint32_t>(mIndices.size());

      // For now, load position, normal, and uv
      const float* positions = nullptr;
      const float* normals = nullptr;
      const float* texCoords = nullptr;

      uint32_t vertexCount = 0;
      uint32_t indexCount = 0;

      auto& accessor =
          gltfModel.accessors[primitive.attributes.find("POSITION")->second];
      auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
      vertexCount = accessor.count;
      positions = reinterpret_cast<const float*>(
          &(gltfModel.buffers[bufferView.buffer]
                .data[accessor.byteOffset + bufferView.byteOffset]));

      accessor =
          gltfModel.accessors[primitive.attributes.find("NORMAL")->second];
      bufferView = gltfModel.bufferViews[accessor.bufferView];
      normals = reinterpret_cast<const float*>(
          &(gltfModel.buffers[bufferView.buffer]
                .data[accessor.byteOffset + bufferView.byteOffset]));

      bool hasTexCoords = primitive.attributes.find("TEXCOORD_0") !=
                          primitive.attributes.end();
      if (hasTexCoords) {
        accessor =
            gltfModel
                .accessors[primitive.attributes.find("TEXCOORD_0")->second];
        bufferView = gltfModel.bufferViews[accessor.bufferView];
        texCoords = reinterpret_cast<const float*>(
            &(gltfModel.buffers[bufferView.buffer]
                  .data[accessor.byteOffset + bufferView.byteOffset]));
      }

      // Fill vertex buffer
      for (size_t vertexID = 0; vertexID < vertexCount; vertexID++) {
        Vertex vertex{};
        vertex.position = glm::make_vec3(&positions[vertexID * 3]);
        vertex.normal = glm::make_vec3(&normals[vertexID * 3]);

        if (hasTexCoords) {
          vertex.texCoord = glm::make_vec2(&texCoords[vertexID * 2]);
        }

        mVertices.push_back(vertex);
      }

      // Fill index buffer
      accessor = gltfModel.accessors[primitive.indices];
      bufferView = gltfModel.bufferViews[accessor.bufferView];
      indexCount = accessor.count;

      // TODO: use first vertex instead of offseting the index buffer here

      switch (accessor.componentType) {
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
          const auto* buffer = reinterpret_cast<const uint32_t*>(
              &(gltfModel.buffers[bufferView.buffer]
                    .data[accessor.byteOffset + bufferView.byteOffset]));
          for (size_t index = 0; index < indexCount; index++) {
            mIndices.push_back(buffer[index] + firstVertex);
          }
        } break;
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
          const auto* buffer = reinterpret_cast<const uint16_t*>(
              &(gltfModel.buffers[bufferView.buffer]
                    .data[accessor.byteOffset + bufferView.byteOffset]));
          for (size_t index = 0; index < indexCount; index++) {
            mIndices.push_back(buffer[index] + firstVertex);
          }
        } break;
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
          const auto* buffer = reinterpret_cast<const uint8_t*>(
              &(gltfModel.buffers[bufferView.buffer]
                    .data[accessor.byteOffset + bufferView.byteOffset]));
          for (size_t index = 0; index < indexCount; index++) {
            mIndices.push_back(buffer[index] + firstVertex);
          }
        } break;
        default:
          std::cerr << "Unknown index component type" << std::endl;
      }

      // Create Mesh
      Mesh mesh{};
      mesh.firstVertex = firstVertex;
      mesh.vertexCount = vertexCount;
      mesh.firstIndex = firstIndex;
      mesh.indexCount = indexCount;

      uint32_t meshID = mMeshes.size();
      mMeshes.push_back(mesh);

      GeometryInstance geometryInstance{};
      geometryInstance.materialID = materialOffset + primitive.material;
      geometryInstance.modelMatrixID = modelMatrixID;
      geometryInstance.meshID = meshID;
      mGeometryInstances.push_back(geometryInstance);
    }
  }
}

void Scene::addLight(const Light& light) {
  mLights.push_back(light);
}

void Scene::update(float deltaTime) {
  mFrameCount++;
  mStaticFrameCount++;

  // Reset update flags and handle possible new update
  mUpdates = UpdateFlags::None;

  // Handle camera move event
  if (mCamera.hasMoved()) {
    mUpdates |= UpdateFlags::Camera;

    resetStaticFrameCount();
  }

  // Handle geometry move event
  mECS.getRegistry()
      .view<TagComponent, TransformComponent, SceneNodeComponent>()
      .each([&](auto entity, auto& tag, auto& transform, auto& sceneNode) {
        mModelMatrices.at(sceneNode.modelMatrixID) = transform.getMat4();
        mUpdates |= UpdateFlags::ModelMatrix;

        // TODO: remember to reset static frame counter for dynamic scenes...
        // resetStaticFrameCount();
      });
}

Scene::AABB Scene::getAABB() const {
  AABB aabb{};

  aabb.min = glm::vec3{std::numeric_limits<float>::max()};
  aabb.max = glm::vec3{std::numeric_limits<float>::min()};

  for (const auto& instance : mGeometryInstances) {
    const auto& mesh = mMeshes.at(instance.meshID);
    const auto& modelMatrix = mModelMatrices.at(instance.modelMatrixID);

    for (uint32_t i = 0; i < mesh.vertexCount; i++) {
      const auto& vertex = mVertices.at(mesh.firstVertex + i);
      auto position = glm::vec3(modelMatrix * glm::vec4(vertex.position, 1.f));

      aabb.min = glm::min(aabb.min, position);
      aabb.max = glm::max(aabb.max, position);
    }
  }

  return aabb;
}

}  // namespace recore::scene
