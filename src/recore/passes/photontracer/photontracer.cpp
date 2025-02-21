#include "photontracer.h"

#include "photontracer.glslh"

namespace recore::passes {

constexpr auto kPhotonTracerShader =
    "recore/passes/photontracer/photontracer.comp.glsl";

PhotonTracerPass::PhotonTracerPass(const vulkan::Device& device,
                                   const scene::GPUScene& scene)
    : Pass{device}, mScene{scene} {
  mBuffers.hashGrid = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mHashGrid.size * sizeof(HashGridCell),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
  vulkan::debug::setName(*mBuffers.hashGrid, "HashGrid");
  mHashGrid.cells = mBuffers.hashGrid->getDeviceAddress();

  mBuffers.photons = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = mHashGrid.size * sizeof(PhotonCell),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });
  vulkan::debug::setName(*mBuffers.photons, "Photons");
}

void PhotonTracerPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  mPipelineLayout = makeUnique<vulkan::PipelineLayout>({
      .device = mDevice,
      .descriptorSetLayouts = {&mScene.getDescriptorSetLayout()},
      .pushConstants = {{
          .stageFlags = VK_SHADER_STAGE_ALL,
          .size = sizeof(PhotonTracerPush),
      }},
  });

  const auto& shaderData = shaderLibrary.loadShader(kPhotonTracerShader);
  mPipeline = makeUnique<vulkan::ComputePipeline>({
      .device = mDevice,
      .layout = *mPipelineLayout,
      .shader = *shaderData.shader,
  });
  mWorkgroupSize = shaderData.reflection.workgroupSize.value();
}

void PhotonTracerPass::execute(const vulkan::CommandBuffer& commandBuffer,
                               vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(
      currentFrame, commandBuffer, "PhotonTracer::execute");

  PhotonTracerPush p{
      .scene = mScene.getSceneDataDeviceAddress(),
      .photons = mBuffers.photons->getDeviceAddress(),
      .hashGrid = mHashGrid,
  };

  commandBuffer.bindPipeline(*mPipeline);
  commandBuffer.bindDescriptorSet(*mPipeline, mScene.getDescriptorSet(), 0);

  commandBuffer.pushConstants(*mPipelineLayout, p);

  vulkan::CommandBuffer::DispatchDim dispatchDim = {
      vulkan::dispatchSize(mWorkgroupSize.x, 4096),
      1,
      1,
  };
  commandBuffer.dispatch(dispatchDim);
}

}  // namespace recore::passes
