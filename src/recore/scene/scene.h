#pragma once

#include <filesystem>

#include <glm/glm.hpp>

#include "camera.h"

#include "ecs.h"

#include "scene.glslh"

namespace recore::scene {

class Scene {
 public:
  struct Texture {
    enum class Format { UNORM, SRGB };

    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> image;
    Format format = Format::UNORM;
  };

  enum class UpdateFlags {
    None = 0,
    Camera = 1,
    ModelMatrix = 2,
  };

  explicit Scene() = default;
  ~Scene() = default;

  // Delete copy & move constructors.
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  struct GLTFLoadDesc {
    std::filesystem::path path;
    const glm::vec3& translation{0.f};
    const glm::vec3& rotation{0.f};
    const glm::vec3& scale{1.f};
    std::optional<std::string> name;
  };

  void loadGLTF(const GLTFLoadDesc& desc);

  void addLight(const Light& light);

  void update(float deltaTime);

  [[nodiscard]] const std::vector<Vertex>& getVertices() const {
    return mVertices;
  }

  [[nodiscard]] const std::vector<uint32_t>& getIndices() const {
    return mIndices;
  }

  [[nodiscard]] const std::vector<Mesh>& getMeshes() const { return mMeshes; }

  [[nodiscard]] const std::vector<GeometryInstance>& getGeometryInstances()
      const {
    return mGeometryInstances;
  }

  [[nodiscard]] const std::vector<glm::mat4>& getModelMatrices() const {
    return mModelMatrices;
  }

  [[nodiscard]] const std::vector<Texture>& getTextures() const {
    return mTextures;
  }

  [[nodiscard]] const std::vector<Material>& getMaterials() const {
    return mMaterials;
  }

  [[nodiscard]] const std::vector<Light>& getLights() const { return mLights; }

  [[nodiscard]] std::vector<Light>& getLights() { return mLights; }

  void setCamera(Camera camera) {
    auto oldAspect = mCamera.getAspect();
    mCamera = camera;
    mCamera.setAspect(oldAspect);
  }

  [[nodiscard]] Camera& getCamera() { return mCamera; }

  [[nodiscard]] const Camera& getCamera() const { return mCamera; }

  [[nodiscard]] UpdateFlags getUpdates() const { return mUpdates; }

  [[nodiscard]] uint32_t getFrameCount() const { return mFrameCount; }

  void resetFrameCount() {
    mFrameCount = 0;
    resetStaticFrameCount();
  }

  [[nodiscard]] uint32_t getStaticFrameCount() const {
    return mStaticFrameCount;
  }

  void resetStaticFrameCount() { mStaticFrameCount = 0; }

  [[nodiscard]] ECS& getECS() { return mECS; }

  [[nodiscard]] bool& enableAnimation() { return mEnableAnimation; }

  void setName(const std::string& name) { mName = name; }

  [[nodiscard]] const std::string& getName() const { return mName; }

  struct AABB {
    glm::vec3 min;
    glm::vec3 max;
  };

  [[nodiscard]] AABB getAABB() const;

 private:
  // Rendering data... (meshes, instances, materials, textures, ...)
  std::vector<Vertex> mVertices;
  std::vector<uint32_t> mIndices;

  std::vector<Mesh> mMeshes;
  std::vector<GeometryInstance> mGeometryInstances;
  std::vector<glm::mat4> mModelMatrices;

  std::vector<Texture> mTextures;
  std::vector<Material> mMaterials;

  std::vector<Light> mLights;

  // Meta data
  Camera mCamera;
  UpdateFlags mUpdates;

  // Clock
  uint32_t mStaticFrameCount = 0;
  uint32_t mFrameCount = 0;
  bool mEnableAnimation = false;

  // ECS
  ECS mECS;

  std::string mName = "unnamed scene";
};

}  // namespace recore::scene

namespace recore {
RECORE_ENUM_CLASS_OPERATORS(scene::Scene::UpdateFlags)
}
