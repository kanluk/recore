#include "prefixsum.h"

#include "prefixsum.glslh"

#include <recore/vulkan/utils.h>

#include <recore/vulkan/debug.h>

namespace recore::passes {

constexpr auto kPrefixSumShader = "recore/passes/prefixsum/prefixsum.comp.glsl";

constexpr auto kPrefixSumAddShader =
    "recore/passes/prefixsum/prefixsum_add.comp.glsl";

PrefixSumPass::PrefixSumPass(const vulkan::Device& device,
                             const vulkan::Buffer& dataBuffer)
    : Pass{device}, mDataBuffer{dataBuffer} {
  mWorkgroupPrefixSumsBuffer = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = 1024 * 1024 * sizeof(uint32_t),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mTotalSumBuffer = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = sizeof(uint32_t),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mPrevTotalSumBuffer = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = sizeof(uint32_t),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
}

void PrefixSumPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  {
    mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {},
        .pushConstants = {{.stageFlags = VK_SHADER_STAGE_ALL,
                           .size = sizeof(PrefixSumPush)}},
    });

    const auto& shaderData = shaderLibrary.loadShader(kPrefixSumShader);
    mPipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mPipelineLayout,
        .shader = *shaderData.shader,
    });
    mWorkgroupSize = shaderData.reflection.workgroupSize.value();
  }

  {
    mAddPass.pipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {},
        .pushConstants = {{.stageFlags = VK_SHADER_STAGE_ALL,
                           .size = sizeof(PrefixSumPush)}},
    });

    const auto& shaderData = shaderLibrary.loadShader(kPrefixSumAddShader);
    mAddPass.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mAddPass.pipelineLayout,
        .shader = *shaderData.shader,
    });
    mAddPass.workgroupSize = shaderData.reflection.workgroupSize.value();
  }
}

void PrefixSumPass::execute(const vulkan::CommandBuffer& commandBuffer,
                            vulkan::RenderFrame& currentFrame) {
  RECORE_DEBUG_SCOPE(commandBuffer, "PrefixSumPass::execute");
  auto inputDeviceAddress = mDataBuffer.getDeviceAddress();

  auto numElements = static_cast<uint32_t>(mDataBuffer.getSize() /
                                           sizeof(uint32_t));

  PrefixSumPush p{
      .data = inputDeviceAddress,
      .workgroupPrefixSums = mWorkgroupPrefixSumsBuffer->getDeviceAddress(),
      .totalSum = mTotalSumBuffer->getDeviceAddress(),
      .prevTotalSum = mPrevTotalSumBuffer->getDeviceAddress(),
      .size = numElements,
      .iteration = 0,
  };

  uint32_t maxNumElementsPerIteration = mWorkgroupSize.x * mWorkgroupSize.x * 2;
  uint32_t numIterations = vulkan::dispatchSize(maxNumElementsPerIteration,
                                                numElements);

  commandBuffer.fillBuffer(*mTotalSumBuffer);

  for (uint32_t i = 0; i < numIterations; i++) {
    RECORE_DEBUG_SCOPE(commandBuffer, std::format("PrefixSumPass: i = {}", i));

    uint32_t xSize = std::max(
        1u,
        vulkan::dispatchSize(
            mWorkgroupSize.x * 2,
            std::min(maxNumElementsPerIteration, numElements)));
    p.iteration = i,

    commandBuffer.memoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.copyBufferToBuffer(*mTotalSumBuffer, *mPrevTotalSumBuffer);
    commandBuffer.fillBuffer(*mWorkgroupPrefixSumsBuffer);

    commandBuffer.memoryBarrier(
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    {
      RECORE_DEBUG_SCOPE(commandBuffer, "PrefixSumPass::LocalPrefixSum");
      commandBuffer.bindPipeline(*mPipeline);
      commandBuffer.pushConstants(*mPipelineLayout, p);
      commandBuffer.dispatch({xSize, 1, 1});
    }

    commandBuffer.memoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    if (xSize > 1) {
      RECORE_DEBUG_SCOPE(commandBuffer, "PrefixSumPass::Add");
      commandBuffer.bindPipeline(*mAddPass.pipeline);
      commandBuffer.pushConstants(*mAddPass.pipelineLayout, p);
      commandBuffer.dispatch({(xSize - 1) * 2, 1, 1});
    }

    commandBuffer.memoryBarrier(VK_ACCESS_SHADER_WRITE_BIT,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    numElements -= maxNumElementsPerIteration;
  }
}

}  // namespace recore::passes
