#include "guided_pathtracer.h"

#include <recore/shaders/vmm.glslh>

#include "guided_pathtracer.glslh"
#include "guiding.glslh"

namespace recore::passes {

constexpr auto kPathTracerShader =
    "recore/passes/guided_pathtracer/guided_pathtracer.comp.glsl";

constexpr auto kGuidingIndicesShader =
    "recore/passes/guided_pathtracer/guiding_indices.comp.glsl";

constexpr auto kGuidingEMShader =
    "recore/passes/guided_pathtracer/guiding_em.comp.glsl";

GuidedPathTracerPass::GuidedPathTracerPass(const vulkan::Device& device,
                                           const scene::GPUScene& scene)
    : Pass{device}, mScene{scene} {
  // Initialize descriptors
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
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
              },
              {
                  {.binding = 2,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
              },
              {
                  {.binding = 3,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
              },
              {
                  {.binding = 4,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   .descriptorCount = 1,
                   .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
              },
          },
  });

  // Initialize buffers
  mDescriptors.set = makeUnique<vulkan::DescriptorSet>({
      .device = mDevice,
      .pool = *mDescriptors.pool,
      .layout = *mDescriptors.layout,
  });

  mBuffers.hashGrid = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mHashGrid.size * sizeof(HashGridCell),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
  vulkan::debug::setName(*mBuffers.hashGrid, "HashGrid");

  mBuffers.vmms = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mHashGrid.size * sizeof(VMM),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
  vulkan::debug::setName(*mBuffers.vmms, "VMMs");

  mBuffers.cellCounters = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mHashGrid.size * sizeof(uint32_t),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
  vulkan::debug::setName(*mBuffers.cellCounters, "CellCounters");

  mBuffers.cellCountersPrefix = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mHashGrid.size * sizeof(uint32_t),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
  vulkan::debug::setName(*mBuffers.cellCountersPrefix, "CellCountersPrefix");

  mBuffers.cellPrefixSums = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mHashGrid.size * sizeof(uint32_t),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
  vulkan::debug::setName(*mBuffers.cellPrefixSums, "CellPrefixSums");

  mHashGrid.cells = mBuffers.hashGrid->getDeviceAddress();

  mSampler = makeUnique<vulkan::Sampler>({.device = mDevice});

  mPrefixSumPass = makeUnique<PrefixSumPass>(mDevice, *mBuffers.cellPrefixSums);
}

void GuidedPathTracerPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  {
    mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {&mScene.getDescriptorSetLayout(),
                                 &*mDescriptors.layout},
        .pushConstants = {{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .size = sizeof(GuidedPathTracerPush),
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

  mPrefixSumPass->reloadShaders(shaderLibrary);

  {
    mGuidingIndicesPass.pipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {},
        .pushConstants = {{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .size = sizeof(GuidingIndicesPush),
        }},
    });

    const auto& shaderData = shaderLibrary.loadShader(kGuidingIndicesShader);
    mGuidingIndicesPass.workgroupSize =
        shaderData.reflection.workgroupSize.value();

    mGuidingIndicesPass.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mGuidingIndicesPass.pipelineLayout,
        .shader = *shaderData.shader,
    });
  }

  {
    mGuidingEMPass.pipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {},
        .pushConstants = {{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .size = sizeof(GuidingEMPush),
        }},
    });

    const auto& shaderData = shaderLibrary.loadShader(kGuidingEMShader);
    mGuidingEMPass.workgroupSize = shaderData.reflection.workgroupSize.value();

    mGuidingEMPass.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mGuidingEMPass.pipelineLayout,
        .shader = *shaderData.shader,
    });
  }
}

void GuidedPathTracerPass::setInput(const Input& input) {
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

void GuidedPathTracerPass::resize(uint32_t width, uint32_t height) {
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

  // Buffers that depend on the resolution

  mNumGuidingSamples = width * height * PATH_TRACER_MAX_BOUNCES;

  mBuffers.guidingSamples = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mNumGuidingSamples * sizeof(GuidingSample),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mBuffers.cellIndices = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mNumGuidingSamples * sizeof(uint32_t),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
}

void GuidedPathTracerPass::execute(const vulkan::CommandBuffer& commandBuffer,
                                   vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(
      currentFrame, commandBuffer, "GuidedPathTracer::execute");
  // Perpare buffers, zero initialize per frame
  prepareBuffers(commandBuffer, currentFrame);

  commandBuffer.memoryBarrier(
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

  commandBuffer.transitionImageLayout(*mOutputImage,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_GENERAL,
                                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  // Run path tracer
  executePathTracer(commandBuffer, currentFrame);

  if (mSettings.train) {
    RECORE_GPU_PROFILE_SCOPE(
        currentFrame, commandBuffer, "GuidedPathTracer::Guiding");
    commandBuffer.memoryBarrier(
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    computeCellCounterPrefixSum(commandBuffer, currentFrame);
    prepareGuidingIndices(commandBuffer, currentFrame);

    commandBuffer.memoryBarrier(
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    updateGuidingMixture(commandBuffer, currentFrame);
  }
}

void GuidedPathTracerPass::resetGrid() {
  mDevice.submitAndWait([&](const auto& commandBuffer) {
    commandBuffer.fillBuffer(*mBuffers.hashGrid);
    commandBuffer.fillBuffer(*mBuffers.vmms);
  });
}

void GuidedPathTracerPass::prepareBuffers(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) const {
  RECORE_GPU_PROFILE_SCOPE(
      currentFrame, commandBuffer, "GuidedPathTracerPass::prepareBuffers");

  commandBuffer.memoryBarrier(
      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT);

  commandBuffer.fillBuffer(*mBuffers.guidingSamples);
  commandBuffer.fillBuffer(*mBuffers.cellCounters);
  commandBuffer.fillBuffer(*mBuffers.cellCountersPrefix);
  commandBuffer.fillBuffer(*mBuffers.cellPrefixSums);
  commandBuffer.fillBuffer(*mBuffers.cellIndices);

  commandBuffer.memoryBarrier(
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void GuidedPathTracerPass::executePathTracer(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(
      currentFrame, commandBuffer, "GuidedPathTracerPass::executePathTracer");

  // Generate a random number based on time value
  uint32_t rngSeed = static_cast<uint32_t>(
      std::chrono::system_clock::now().time_since_epoch().count());

  GuidedPathTracerPush p{
      .scene = mScene.getSceneDataDeviceAddress(),
      .hashGrid = mHashGrid,
      .vmms = mBuffers.vmms->getDeviceAddress(),
      .guidingSamples = mBuffers.guidingSamples->getDeviceAddress(),
      .cellCounters = mBuffers.cellCounters->getDeviceAddress(),
      .rngSeed = rngSeed,
      .train = static_cast<bool32>(mSettings.train),
      .guide = static_cast<bool32>(mSettings.guide),
      .selectGuidingProbability = mSettings.selectGuidingProbability,
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

void GuidedPathTracerPass::computeCellCounterPrefixSum(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) const {
  RECORE_GPU_PROFILE_SCOPE(currentFrame,
                           commandBuffer,
                           "GuidedPathTracerPass::computeCellCounterPrefixSum");

  commandBuffer.copyBufferToBuffer(*mBuffers.cellCounters,
                                   *mBuffers.cellPrefixSums);

  commandBuffer.memoryBarrier(
      VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  commandBuffer.memoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  mPrefixSumPass->execute(commandBuffer, currentFrame);

  commandBuffer.memoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void GuidedPathTracerPass::prepareGuidingIndices(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame,
                           commandBuffer,
                           "GuidedPathTracerPass::prepareGuidingIndices");

  GuidingIndicesPush p{
      .guidingSamples = mBuffers.guidingSamples->getDeviceAddress(),
      .cellCounters = mBuffers.cellCounters->getDeviceAddress(),
      .cellPrefixSums = mBuffers.cellPrefixSums->getDeviceAddress(),
      .cellIndices = mBuffers.cellIndices->getDeviceAddress(),
      .numGuidingSamples = mNumGuidingSamples,
  };

  commandBuffer.bindPipeline(*mGuidingIndicesPass.pipeline);
  commandBuffer.pushConstants(*mGuidingIndicesPass.pipelineLayout, p);

  vulkan::CommandBuffer::DispatchDim dispatchDim = {
      vulkan::dispatchSize(mGuidingIndicesPass.workgroupSize.x,
                           mNumGuidingSamples),
      1,
      1,
  };
  commandBuffer.dispatch(dispatchDim);
}

void GuidedPathTracerPass::updateGuidingMixture(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) const {
  RECORE_GPU_PROFILE_SCOPE(currentFrame,
                           commandBuffer,
                           "GuidedPathTracerPass::updateGuidingMixture");

  commandBuffer.memoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  GuidingEMPush p{
      .hashGrid = mHashGrid,
      .vmms = mBuffers.vmms->getDeviceAddress(),
      .guidingSamples = mBuffers.guidingSamples->getDeviceAddress(),
      .cellCounters = mBuffers.cellCounters->getDeviceAddress(),
      .cellPrefixSums = mBuffers.cellPrefixSums->getDeviceAddress(),
      .cellIndices = mBuffers.cellIndices->getDeviceAddress(),
  };

  commandBuffer.bindPipeline(*mGuidingEMPass.pipeline);
  commandBuffer.pushConstants(*mGuidingEMPass.pipelineLayout, p);

  vulkan::CommandBuffer::DispatchDim dispatchDim = {
      vulkan::dispatchSize(mGuidingEMPass.workgroupSize.x, mHashGrid.size),
      1,
      1,
  };
  commandBuffer.dispatch(dispatchDim);
}

}  // namespace recore::passes
