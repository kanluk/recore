#include "tone_mapping.h"

#include "tone_mapping.glslh"

namespace recore::passes {

constexpr auto kFullscreenShader = "recore/shaders/fullscreen.vert.glsl";

constexpr auto kToneMappingShader =
    "recore/passes/tone_mapping/tone_mapping.frag.glsl";

ToneMappingPass::ToneMappingPass(const vulkan::Device& device) : Pass{device} {
  mRenderPass = makeUnique<vulkan::RenderPass>(
      {.device = mDevice,
       .attachments = {
           {VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
       }});

  mDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
      .device = mDevice,
      .poolSizes =
          {
              {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
          },
  });

  mDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
      .device = mDevice,
      .bindings =
          {
              {
                  {.binding = 0,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
              },
          },
  });

  mDescriptors.set = makeUnique<vulkan::DescriptorSet>({
      .device = mDevice,
      .pool = *mDescriptors.pool,
      .layout = *mDescriptors.layout,
  });

  mSampler = makeUnique<vulkan::Sampler>({.device = mDevice});
}

void ToneMappingPass::resize(uint32_t width, uint32_t height) {
  mOutputImage = makeUnique<vulkan::Image>({
      .device = mDevice,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {width, height, 1},
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mFramebuffer = makeUnique<vulkan::Framebuffer>(
      {.device = mDevice,
       .renderPass = *mRenderPass,
       .attachments = {&mOutputImage->getView()},
       .width = width,
       .height = height});
}

void ToneMappingPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>(
      {.device = mDevice,
       .descriptorSetLayouts = {&*mDescriptors.layout},
       .pushConstants = {
           {.stageFlags = VK_SHADER_STAGE_ALL, .size = sizeof(ToneMappingPush)},
       }});

  const auto& vertexShader = shaderLibrary.loadShader(kFullscreenShader);
  const auto& fragmentShader = shaderLibrary.loadShader(kToneMappingShader);

  mPipeline = makeUnique<vulkan::RasterPipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .renderPass = *mRenderPass,
      .state = {},
      .shaders = {vertexShader, fragmentShader},
  });
}

void ToneMappingPass::execute(const vulkan::CommandBuffer& commandBuffer,
                              vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(
      currentFrame, commandBuffer, "ToneMappingPass::execute");

  commandBuffer.beginRenderPass(*mRenderPass,
                                *mFramebuffer,
                                {
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                });

  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindDescriptorSet(*mPipeline, *mDescriptors.set, 0);

  ToneMappingPush p{
      .gamma = mSettings.gamma,
      .exposure = mSettings.exposure,
  };

  commandBuffer.pushConstants(*mPipelineLayout, p);

  commandBuffer.draw(3);

  commandBuffer.endRenderPass();
}

void ToneMappingPass::setInput(const vulkan::Image& inputImage) {
  vulkan::DescriptorSet::Resources resources;
  resources.images[0][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = inputImage.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  mDescriptors.set->update(resources);
}

}  // namespace recore::passes
