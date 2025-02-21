#pragma once

#include <recore/passes/pass.h>

namespace recore::passes {

class PrefixSumPass : public Pass {
 public:
  explicit PrefixSumPass(const vulkan::Device& device,
                         const vulkan::Buffer& dataBuffer);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

 private:
  const vulkan::Buffer& mDataBuffer;

  uPtr<vulkan::Buffer> mWorkgroupPrefixSumsBuffer;
  uPtr<vulkan::Buffer> mTotalSumBuffer;
  uPtr<vulkan::Buffer> mPrevTotalSumBuffer;

  uPtr<vulkan::Pipeline> mPipeline;
  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  vulkan::ShaderReflectionData::WorkgroupSize mWorkgroupSize{};

  struct {
    uPtr<vulkan::Pipeline> pipeline;
    uPtr<vulkan::PipelineLayout> pipelineLayout;
    vulkan::ShaderReflectionData::WorkgroupSize workgroupSize{};
  } mAddPass;
};

}  // namespace recore::passes
