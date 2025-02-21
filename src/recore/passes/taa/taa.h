#pragma once

#include <recore/passes/pass.h>

namespace recore::passes {

class TAAPass : public Pass {
 public:
  explicit TAAPass(const vulkan::Device& device);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void resize(uint32_t width, uint32_t height) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  struct Input {
    const vulkan::Image& gMotion;
    const vulkan::Image& image;
  };

  void setInput(const Input& input);

  [[nodiscard]] const vulkan::Image& getOutputImage() const {
    return *mOutputImage;
  }

 private:
  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mDescriptors;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::ComputePipeline> mPipeline;
  vulkan::ShaderReflectionData::WorkgroupSize mWorkgroupSize{};

  uPtr<vulkan::Sampler> mSampler;

  uPtr<vulkan::Image> mPrevFrame;
  uPtr<vulkan::Image> mOutputImage;
};

}  // namespace recore::passes
