#pragma once

#include <recore/passes/pass.h>

#include <recore/scene/gpu_scene.h>

#include <recore/passes/gbuffer/gbuffer.h>
#include <recore/passes/prefixsum/prefixsum.h>

#include <recore/shaders/hashgrid.glslh>

namespace recore::passes {

struct GuidingPathTracerSettings {
  bool train = true;
  bool guide = true;
  float selectGuidingProbability = 0.8f;
};

class GuidedPathTracerPass : public Pass {
 public:
  explicit GuidedPathTracerPass(const vulkan::Device& device,
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
  };

  void setInput(const Input& input);

  [[nodiscard]] const vulkan::Image& getOutputImage() const {
    return *mOutputImage;
  }

  [[nodiscard]] GuidingPathTracerSettings& settings() { return mSettings; }

  void resetGrid();

 private:
  void prepareBuffers(const vulkan::CommandBuffer& commandBuffer,
                      vulkan::RenderFrame& currentFrame) const;

  void executePathTracer(const vulkan::CommandBuffer& commandBuffer,
                         vulkan::RenderFrame& currentFrame);

  void computeCellCounterPrefixSum(const vulkan::CommandBuffer& commandBuffer,
                                   vulkan::RenderFrame& currentFrame) const;

  void prepareGuidingIndices(const vulkan::CommandBuffer& commandBuffer,
                             vulkan::RenderFrame& currentFrame);

  void updateGuidingMixture(const vulkan::CommandBuffer& commandBuffer,
                            vulkan::RenderFrame& currentFrame) const;

  const scene::GPUScene& mScene;

  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mDescriptors;

  HashGrid mHashGrid = {
      .cells = VkDeviceAddress{0},
      .size = 10000000,
      .scale = 0.1f,
  };
  uint32_t mNumGuidingSamples = 0;

  struct {
    uPtr<vulkan::Buffer> hashGrid;
    uPtr<vulkan::Buffer> vmms;
    uPtr<vulkan::Buffer> guidingSamples;
    uPtr<vulkan::Buffer> cellCounters;
    uPtr<vulkan::Buffer> cellCountersPrefix;
    uPtr<vulkan::Buffer> cellPrefixSums;
    uPtr<vulkan::Buffer> cellIndices;
  } mBuffers;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::ComputePipeline> mPipeline;
  vulkan::ShaderReflectionData::WorkgroupSize mWorkgroupSize{};

  uPtr<vulkan::Sampler> mSampler;

  uPtr<vulkan::Image> mOutputImage;

  uPtr<PrefixSumPass> mPrefixSumPass;

  struct GuidingIndicesPass {
    uPtr<vulkan::PipelineLayout> pipelineLayout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mGuidingIndicesPass;

  struct GuidingEMPass {
    uPtr<vulkan::PipelineLayout> pipelineLayout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mGuidingEMPass;

  GuidingPathTracerSettings mSettings;
};

}  // namespace recore::passes
