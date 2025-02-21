#include "blit.h"

namespace recore::passes {

constexpr auto kFullscreenShader = "recore/shaders/fullscreen.vert.glsl";
constexpr auto kBlitShader = "recore/passes/blit/blit.frag.glsl";

BlitPass::BlitPass(const vulkan::Device& device,
                   const vulkan::RenderPass& renderPass)
    : mDevice{device}, mRenderPass{renderPass} {}

void BlitPass::initialize(vulkan::ShaderLibrary& shaderLibrary) {
  mDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
      .device = mDevice,
      .poolSizes = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}},
  });

  mDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
      .device = mDevice,
      .bindings = {{
          .binding = {.binding = 0,
                      .descriptorType =
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      .descriptorCount = 1,
                      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
      }},
  });

  mDescriptors.set = makeUnique<vulkan::DescriptorSet>({
      .device = mDevice,
      .pool = *mDescriptors.pool,
      .layout = *mDescriptors.layout,
  });

  mSampler = makeUnique<vulkan::Sampler>({.device = mDevice});

  reloadPipeline(shaderLibrary);
}

void BlitPass::reloadPipeline(vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
      .device = mDevice,
      .descriptorSetLayouts = {&*mDescriptors.layout},
      .pushConstants = {},
  });

  mPipeline = makeUnique<vulkan::RasterPipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .renderPass = mRenderPass,
      .state = {},
      .shaders = {shaderLibrary.loadShader(kFullscreenShader),
                  shaderLibrary.loadShader(kBlitShader)},
  });
}

void BlitPass::setInput(const vulkan::ImageView& image, VkImageLayout layout) {
  vulkan::DescriptorSet::Resources resources;
  resources.images[0][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = image.vkHandle(),
      .imageLayout = layout,
  };

  mDescriptors.set->update(resources);
}

void BlitPass::draw(const vulkan::CommandBuffer& commandBuffer) {
  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindDescriptorSet(*mPipelineLayout, *mDescriptors.set, 0);
  commandBuffer.draw(3);
}

}  // namespace recore::passes
