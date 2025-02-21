#include "diffuse_pathtracer.h"

#include "diffuse_pathtracer.glslh"

namespace recore::passes {

constexpr auto kPathTracerShader =
    "recore/passes/diffuse_pathtracer/diffuse_pathtracer.comp.glsl";

DiffusePathTracerPass::DiffusePathTracerPass(const vulkan::Device& device,
                                             const scene::GPUScene& scene)
    : Pass{device}, mScene{scene} {
  // Initialize descriptor layout
  {
    mDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
        .device = mDevice,
        .poolSizes =
            {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
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
                {
                    {.binding = 4,
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

void DiffusePathTracerPass::reloadShaders(
    vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
      .device = mDevice,
      .descriptorSetLayouts = {&mScene.getDescriptorSetLayout(),
                               &*mDescriptors.layout},
      .pushConstants = {{
          .stageFlags = VK_SHADER_STAGE_ALL,
          .size = sizeof(DiffusePathTracerPush),
      }},
  });

  const auto& shaderData = shaderLibrary.loadShader(kPathTracerShader);
  mPipeline = makeUnique<vulkan::ComputePipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .shader = *shaderData.shader,
  });
  mWorkgroupSize = shaderData.reflection.workgroupSize.value();
}

void DiffusePathTracerPass::resize(uint32_t width, uint32_t height) {
  mOutputImage = makeUnique<vulkan::Image>({
      .device = mDevice,
      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
      .extent = {width, height, 1},
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mDevice.submitAndWait([&](const auto& commandBuffer) {
    commandBuffer.transitionImageLayout(*mOutputImage,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  });
}

void DiffusePathTracerPass::execute(const vulkan::CommandBuffer& commandBuffer,
                                    vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(
      currentFrame, commandBuffer, "DiffusePathTracer::execute");

  commandBuffer.transitionImageLayout(*mOutputImage,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL,
                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  uint32_t rngSeed = static_cast<uint32_t>(
      std::chrono::system_clock::now().time_since_epoch().count());

  DiffusePathTracerPush p{
      .scene = mScene.getSceneDataDeviceAddress(),
      .rngSeed = rngSeed,
  };

  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindDescriptorSet(*mPipeline, mScene.getDescriptorSet(), 0);
  commandBuffer.bindDescriptorSet(*mPipeline, *mDescriptors.set, 1);

  commandBuffer.pushConstants(*mPipelineLayout, p);

  vulkan::CommandBuffer::DispatchDim dispatchDim = {
      vulkan::dispatchSize(mWorkgroupSize.x, mOutputImage->getWidth()),
      vulkan::dispatchSize(mWorkgroupSize.y, mOutputImage->getHeight()),
      1,
  };
  commandBuffer.dispatch(dispatchDim);
}

void DiffusePathTracerPass::setInput(const Input& input) {
  vulkan::DescriptorSet::Resources resources;

  resources.images[0][0] = {
      .sampler = VK_NULL_HANDLE,
      .imageView = mOutputImage->getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  resources.images[1][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = input.gPosition.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  resources.images[2][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = input.gNormal.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  resources.images[3][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = input.gAlbedo.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  resources.images[4][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = input.gEmissive.getView().vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  mDescriptors.set->update(resources);
}

}  // namespace recore::passes
