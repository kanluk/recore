#pragma once

#include "scene.h"

#include <recore/vulkan/api/acceleration.h>
#include <recore/vulkan/api/buffer.h>
#include <recore/vulkan/api/descriptor.h>
#include <recore/vulkan/api/image.h>

#include <recore/vulkan/context.h>

namespace recore::scene {

class GPUScene {
 public:
  explicit GPUScene(const vulkan::Device& device,
                    const Scene& scene,
                    bool enableRayTracing = false);
  ~GPUScene() = default;

  // Delete copy & move constructors.
  GPUScene(const GPUScene&) = delete;
  GPUScene& operator=(const GPUScene&) = delete;
  GPUScene(GPUScene&&) = delete;
  GPUScene& operator=(GPUScene&&) = delete;

  void upload();
  void update(const vulkan::CommandBuffer& commandBuffer,
              vulkan::RenderFrame& currentFrame);

  [[nodiscard]] const Scene& getScene() const { return mScene; }

  [[nodiscard]] const vulkan::DescriptorSetLayout& getDescriptorSetLayout()
      const {
    return *mDescriptor.layout;
  }

  [[nodiscard]] const vulkan::DescriptorSet& getDescriptorSet() const {
    return *mDescriptor.set;
  }

  [[nodiscard]] const vulkan::Buffer& getVertexBuffer() const {
    return *mBuffers.vertices;
  }

  [[nodiscard]] const vulkan::Buffer& getIndexBuffer() const {
    return *mBuffers.indices;
  }

  [[nodiscard]] VkDeviceAddress getSceneDataDeviceAddress() const {
    return mBuffers.sceneData->getDeviceAddress();
  }

  void enableCameraJitter() { mEnableCameraJitter = true; }

 private:
  const vulkan::Device& mDevice;
  const Scene& mScene;

  bool mEnableRayTracing = false;
  bool mEnableCameraJitter = false;

  struct {
    uPtr<vulkan::Buffer> vertices;
    uPtr<vulkan::Buffer> indices;
    uPtr<vulkan::Buffer> meshes;
    uPtr<vulkan::Buffer> geometryInstances;
    uPtr<vulkan::Buffer> modelMatrices;
    uPtr<vulkan::Buffer> materials;
    uPtr<vulkan::Buffer> lights;
    uPtr<vulkan::Buffer> sceneData;
  } mBuffers;

  SceneData mSceneData;

  std::vector<uPtr<vulkan::Image>> mImages;
  uPtr<vulkan::Sampler> mSampler;

  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mDescriptor;

  struct {
    std::vector<uPtr<vulkan::BLAS>> blases;
    uPtr<vulkan::TLAS> tlas;
    uPtr<vulkan::Buffer> identityTransform;
  } mAcceleration;

  void createBLASes();

  void createTLAS();

  void buildAccelerationStructure(const vulkan::CommandBuffer& commandBuffer);
};

};  // namespace recore::scene
