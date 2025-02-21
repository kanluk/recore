#pragma once

#include <recore/passes/pass.h>

#include <recore/scene/gpu_scene.h>

#include <recore/shaders/hashgrid.glslh>

namespace recore::passes {

class PhotonTracerPass : public Pass {
 public:
  explicit PhotonTracerPass(const vulkan::Device& device,
                            const scene::GPUScene& scene);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  [[nodiscard]] const HashGrid* getHashGrid() const { return &mHashGrid; }

  [[nodiscard]] VkDeviceAddress getPhotonsAddress() const {
    return mBuffers.photons->getDeviceAddress();
  }

 private:
  const scene::GPUScene& mScene;

  HashGrid mHashGrid = {
      .cells = VkDeviceAddress{0},
      .size = 10000000,
      .scale = 0.1f,
  };

  struct {
    uPtr<vulkan::Buffer> hashGrid;
    uPtr<vulkan::Buffer> photons;
  } mBuffers;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::ComputePipeline> mPipeline;
  vulkan::ShaderReflectionData::WorkgroupSize mWorkgroupSize{};

  uPtr<vulkan::Sampler> mSampler;
};

}  // namespace recore::passes
