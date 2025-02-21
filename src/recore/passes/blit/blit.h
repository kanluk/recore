#pragma once

#include <recore/core/base.h>

#include <recore/vulkan/api/command.h>
#include <recore/vulkan/api/device.h>
#include <recore/vulkan/api/image.h>
#include <recore/vulkan/api/pipeline.h>
#include <recore/vulkan/api/renderpass.h>

#include <recore/scene/gpu_scene.h>
#include <recore/vulkan/shader_library.h>

namespace recore::passes {
class BlitPass : public NoCopyMove {
 public:
  explicit BlitPass(const vulkan::Device& device,
                    const vulkan::RenderPass& renderPass);

  void initialize(vulkan::ShaderLibrary& shaderLibrary);

  void reloadPipeline(vulkan::ShaderLibrary& shaderLibrary);

  void setInput(const vulkan::ImageView& image, VkImageLayout layout);

  void draw(const vulkan::CommandBuffer& commandBuffer);

 private:
  const vulkan::Device& mDevice;
  const vulkan::RenderPass& mRenderPass;

  struct {
    uPtr<vulkan::DescriptorPool> pool;
    uPtr<vulkan::DescriptorSetLayout> layout;
    uPtr<vulkan::DescriptorSet> set;
  } mDescriptors;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::RasterPipeline> mPipeline;

  uPtr<vulkan::Sampler> mSampler;
};

}  // namespace recore::passes
