#pragma once

#include <recore/passes/pass.h>

#include <recore/passes/rsm/rsm.h>

#include "lpv.glslh"

namespace recore::passes {

struct LPVSettings {
  uint32_t numIterations = 0;
  bool onlyRSMOccluder = true;
};

class LightPropagationVolumePass : public Pass {
 public:
  explicit LightPropagationVolumePass(const vulkan::Device& device,
                                      const scene::Scene& scene);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  void setRSM(const RSM& rsm);
  void addOccluder(const vulkan::ImageView& position,
                   const vulkan::ImageView& normal);

  [[nodiscard]] LPVGrid& getLPV() { return mLPVGrid; }

  [[nodiscard]] LPVSettings& settings() { return mSettings; }

 private:
  void injectLight(const vulkan::CommandBuffer& commandBuffer,
                   vulkan::RenderFrame& currentFrame);
  void injectGeometry(const vulkan::CommandBuffer& commandBuffer,
                      vulkan::RenderFrame& currentFrame);
  void propagate(const vulkan::CommandBuffer& commandBuffer,
                 vulkan::RenderFrame& currentFrame);

  uPtr<vulkan::Sampler> mSampler;

  // Light injection phase
  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mLightDescriptors;

  struct {
    uPtr<vulkan::PipelineLayout> pipelineLayout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mInjectLightPass;

  // Occluder injection phase
  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    std::vector<uPtr<vulkan::DescriptorSet>> sets;
  } mOccluderDescriptors;

  uint32_t mNumOccluders = 0;

  struct {
    uPtr<vulkan::PipelineLayout> pipelineLayout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mInjectGeometryPass;

  // Light propagation volume

  struct {
    std::array<uPtr<vulkan::Buffer>, 3> radianceVolumeBuffers;
    uPtr<vulkan::Buffer> geometryVolume;
  } mBuffers;

  LPVGrid mLPVGrid;
  glm::uvec2 mRSMResolution{1024, 1024};

  struct {
    uPtr<vulkan::PipelineLayout> pipelineLayout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mPropagationPass;

  LPVSettings mSettings;
};

};  // namespace recore::passes
