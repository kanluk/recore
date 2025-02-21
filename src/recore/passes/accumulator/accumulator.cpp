#include "accumulator.h"

#include "accumulator.glslh"

namespace recore::passes {

constexpr auto kAccumulatorShader =
    "recore/passes/accumulator/accumulator.comp.glsl";

AccumulatorPass::AccumulatorPass(const vulkan::Device& device,
                                 const scene::Scene& scene)
    : Pass{device}, mScene{scene} {
  mDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
      .device = mDevice,
      .poolSizes =
          {
              {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
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
                   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
              },
              {
                  {.binding = 2,
                   .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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

void AccumulatorPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
      .device = mDevice,
      .descriptorSetLayouts = {&*mDescriptors.layout},
      .pushConstants = {{
          .stageFlags = VK_SHADER_STAGE_ALL,
          .size = sizeof(AccumulatorPush),
      }},
  });

  const auto& shaderData = shaderLibrary.loadShader(kAccumulatorShader);
  mPipeline = makeUnique<vulkan::ComputePipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .shader = *shaderData.shader,
  });
  mWorkgroupSize = shaderData.reflection.workgroupSize.value();
}

void AccumulatorPass::resize(uint32_t width, uint32_t height) {
  mAccumulatorImage = makeUnique<vulkan::Image>({
      .device = mDevice,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .extent = {width, height, 1},
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mMomentImage = makeUnique<vulkan::Image>({
      .device = mDevice,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .extent = {width, height, 1},
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mDevice.submitAndWait([&](const auto& commandBuffer) {
    commandBuffer.transitionImageLayout({&*mAccumulatorImage, &*mMomentImage},
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  });
}

void AccumulatorPass::execute(const vulkan::CommandBuffer& commandBuffer,
                              vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "Accumulator::execute");

  if (!mSettings.enabled ||
      is_set(mScene.getUpdates(), scene::Scene::UpdateFlags::Camera)) {
    reset();
  }

  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindDescriptorSet(*mPipeline, *mDescriptors.set, 0);

  AccumulatorPush p{
      // .frameCount = mSettings.enabled ? mScene.getStaticFrameCount() : 0,
      .frameCount = mFrameCount,
      .maxSamples = mSettings.maxSamples,
  };
  commandBuffer.pushConstants(*mPipelineLayout, p);

  vulkan::CommandBuffer::DispatchDim dispatchDim = {
      vulkan::dispatchSize(mWorkgroupSize.x, mAccumulatorImage->getWidth()),
      vulkan::dispatchSize(mWorkgroupSize.y, mAccumulatorImage->getHeight()),
      1,
  };
  commandBuffer.dispatch(dispatchDim);

  if (mSettings.enabled &&
      (mSettings.maxSamples == 0 || mFrameCount <= mSettings.maxSamples)) {
    mFrameCount++;
  }
}

void AccumulatorPass::setInput(const vulkan::Image& inputImage) {
  vulkan::DescriptorSet::Resources resources;

  resources.images[0][0] = {
      .sampler = VK_NULL_HANDLE,
      .imageView = inputImage.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  resources.images[1][0] = {
      .sampler = VK_NULL_HANDLE,
      .imageView = mAccumulatorImage->getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  resources.images[2][0] = {
      .sampler = VK_NULL_HANDLE,
      .imageView = mMomentImage->getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  mDescriptors.set->update(resources);
}

}  // namespace recore::passes
