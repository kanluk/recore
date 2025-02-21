#pragma once

#include <recore/passes/pass.h>

#include <recore/vulkan/api/pipeline.h>

#include <recore/scene/scene.h>

namespace recore::passes {

struct AccumulatorSettings {
  bool enabled = false;
  uint32_t maxSamples = 0;
};

class AccumulatorPass : public Pass {
 public:
  explicit AccumulatorPass(const vulkan::Device& device,
                           const scene::Scene& scene);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void resize(uint32_t width, uint32_t height) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  void setInput(const vulkan::Image& inputImage);

  [[nodiscard]] const vulkan::Image& getAccumulatorImage() const {
    return *mAccumulatorImage;
  }

  [[nodiscard]] const vulkan::Image& getMomentImage() const {
    return *mMomentImage;
  }

  [[nodiscard]] AccumulatorSettings& settings() { return mSettings; }

  void reset() { mFrameCount = 0; }

  [[nodiscard]] uint32_t getFrameCount() const { return mFrameCount; }

 private:
  const scene::Scene& mScene;

  uPtr<vulkan::Image> mAccumulatorImage;
  uPtr<vulkan::Image> mMomentImage;

  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mDescriptors;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::ComputePipeline> mPipeline;
  vulkan::ShaderReflectionData::WorkgroupSize mWorkgroupSize{};

  AccumulatorSettings mSettings;

  uint32_t mFrameCount{0};
};

}  // namespace recore::passes
