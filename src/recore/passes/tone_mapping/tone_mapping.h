#pragma once

#include <recore/passes/pass.h>

namespace recore::passes {

struct ToneMappingSettings {
  float gamma = 2.2f;
  float exposure = 1.f;
};

class ToneMappingPass : public Pass {
 public:
  explicit ToneMappingPass(const vulkan::Device& device);

  void resize(uint32_t width, uint32_t height) override;

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  void setInput(const vulkan::Image& inputImage);

  [[nodiscard]] const vulkan::Image& getOutputImage() const {
    return *mOutputImage;
  }

  [[nodiscard]] ToneMappingSettings& settings() { return mSettings; }

 private:
  uPtr<vulkan::RenderPass> mRenderPass;

  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mDescriptors;

  uPtr<vulkan::Sampler> mSampler;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::RasterPipeline> mPipeline;

  uPtr<vulkan::Image> mOutputImage;
  uPtr<vulkan::Framebuffer> mFramebuffer;

  ToneMappingSettings mSettings;
};

}  // namespace recore::passes
