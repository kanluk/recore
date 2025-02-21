#pragma once

#include <recore/passes/pass.h>

#include <array>

namespace recore::passes {

class SVGFPass : public Pass {
 public:
  explicit SVGFPass(const vulkan::Device& device);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void resize(uint32_t width, uint32_t height) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  struct Input {
    const vulkan::Image& gPosition;
    const vulkan::Image& gNormal;
    const vulkan::Image& gAlbedo;
    const vulkan::Image& gMotion;
    const vulkan::Image& noisyImage;
  };

  void setInput(const Input& input);

  [[nodiscard]] const vulkan::Image& getOutputImage() const {
    return *mOutputImage;
  }

 private:
  // Reprojection
  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mReprojectionDescriptors;

  struct {
    uPtr<vulkan::PipelineLayout> layout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mReprojectionPipeline;

  // A Trous
  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    std::array<uPtr<vulkan::DescriptorSet>, 3> sets;
  } mATrousDescriptors;

  struct {
    uPtr<vulkan::PipelineLayout> layout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mATrousPipeline;

  // Finalize
  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mFinalizeDescriptors;

  struct {
    uPtr<vulkan::PipelineLayout> layout;
    uPtr<vulkan::ComputePipeline> pipeline;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mFinalizePipeline;

  std::array<uPtr<vulkan::Image>, 2> mFilteredImages;

  const vulkan::Image* mPosition;
  const vulkan::Image* mNormal;
  const vulkan::Image* mAlbedo;

  uPtr<vulkan::Image> mOutputImage, mIllumination, mHistoryLength;
  uPtr<vulkan::Image> mPrevPosition, mPrevNormal, mPrevAlbedo,
      mPrevIllumination, mPrevHistoryLength;

  uPtr<vulkan::Sampler> mSampler;
};

}  // namespace recore::passes
