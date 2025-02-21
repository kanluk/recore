#include "taa.h"

namespace recore::passes {

constexpr auto kTAAShader = "recore/passes/taa/taa.comp.glsl";

TAAPass::TAAPass(const vulkan::Device& device) : Pass{device} {
  {  // Initialize descriptor set layout
    mDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
        .device = mDevice,
        .poolSizes =
            {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            },
    });

    mDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
        .device = mDevice,
        .bindings =
            {
                {
                    {.binding = 0,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 1,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 2,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 3,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
            },
    });

    mDescriptors.set = makeUnique<vulkan::DescriptorSet>({
        .device = mDevice,
        .pool = *mDescriptors.pool,
        .layout = *mDescriptors.layout,
    });
  }

  mSampler = makeUnique<vulkan::Sampler>({.device = mDevice});
}

void TAAPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
      .device = mDevice,
      .descriptorSetLayouts = {&*mDescriptors.layout},
      .pushConstants = {},
  });

  const auto& shaderData = shaderLibrary.loadShader(kTAAShader);
  mPipeline = makeUnique<vulkan::ComputePipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .shader = *shaderData.shader,
  });
  mWorkgroupSize = shaderData.reflection.workgroupSize.value();
}

void TAAPass::resize(uint32_t width, uint32_t height) {
  mOutputImage = makeUnique<vulkan::Image>({
      .device = mDevice,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .extent = {width, height, 1},
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mPrevFrame = makeUnique<vulkan::Image>({
      .device = mDevice,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .extent = {width, height, 1},
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mDevice.submitAndWait([&](const auto& commandBuffer) {
    commandBuffer.transitionImageLayout(*mOutputImage,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.transitionImageLayout(
        *mPrevFrame,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  });
}

void TAAPass::execute(const vulkan::CommandBuffer& commandBuffer,
                      vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "TAA::execute");

  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindDescriptorSet(*mPipeline, *mDescriptors.set, 0);

  vulkan::CommandBuffer::DispatchDim dispatchDim = {
      vulkan::dispatchSize(mWorkgroupSize.x, mOutputImage->getWidth()),
      vulkan::dispatchSize(mWorkgroupSize.y, mOutputImage->getHeight()),
      1,
  };
  commandBuffer.dispatch(dispatchDim);

  {  // Blit to save previous frame
    commandBuffer.transitionImageLayout(*mOutputImage,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.transitionImageLayout(
        *mPrevFrame,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.blitImage(*mOutputImage, *mPrevFrame);

    commandBuffer.transitionImageLayout(*mOutputImage,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.transitionImageLayout(
        *mPrevFrame,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }
}

void TAAPass::setInput(const Input& input) {
  vulkan::DescriptorSet::Resources resources;

  resources.images[0][0] = {
      .sampler = VK_NULL_HANDLE,
      .imageView = mOutputImage->getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  resources.images[1][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = input.gMotion.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  resources.images[2][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = mPrevFrame->getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  resources.images[3][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = input.image.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  mDescriptors.set->update(resources);
}

}  // namespace recore::passes
