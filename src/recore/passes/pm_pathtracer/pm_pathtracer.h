#pragma once

#include <recore/passes/pass.h>

#include <recore/scene/gpu_scene.h>

#include <recore/shaders/hashgrid.glslh>

namespace recore::passes {

class PhotonMappingPathTracerPass : public Pass {
 public:
  explicit PhotonMappingPathTracerPass(const vulkan::Device& device,
                                       const scene::GPUScene& scene);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void resize(uint32_t width, uint32_t height) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  struct Input {
    const vulkan::Image& gPosition;
    const vulkan::Image& gNormal;
    const vulkan::Image& gAlbedo;
    const vulkan::Image& gEmissive;
    const vulkan::Image& gMaterial;
  };

  void setInput(const Input& input);

  void setPhotons(const HashGrid* hashGrid, VkDeviceAddress photonsAddress) {
    mHashGrid = hashGrid;
    mPhotonsAddress = photonsAddress;
  }

  [[nodiscard]] const vulkan::Image& getOutputImage() const {
    return *mOutputImage;
  }

 private:
  const scene::GPUScene& mScene;

  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mDescriptors;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::ComputePipeline> mPipeline;
  vulkan::ShaderReflectionData::WorkgroupSize mWorkgroupSize{};

  uPtr<vulkan::Image> mOutputImage;

  uPtr<vulkan::Sampler> mSampler;

  const HashGrid* mHashGrid;
  VkDeviceAddress mPhotonsAddress;
};

}  // namespace recore::passes
