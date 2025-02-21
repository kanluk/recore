#include "lpv.h"

namespace recore::passes {

constexpr auto kInjectLightShader = "recore/passes/lpv/inject_light.comp.glsl";

constexpr auto kInjectGeometryShader =
    "recore/passes/lpv/inject_geometry.comp.glsl";

constexpr auto kPropagationShader = "recore/passes/lpv/propagation.comp.glsl";

LightPropagationVolumePass::LightPropagationVolumePass(
    const vulkan::Device& device, const scene::Scene& scene)
    : Pass{device} {
  mSampler = makeUnique<vulkan::Sampler>({.device = mDevice});

  // Initialize buffers for the LPV grid
  constexpr uint32_t GRID_RESOLUTION = 64;
  for (auto& buffer : mBuffers.radianceVolumeBuffers) {
    buffer = makeUnique<vulkan::Buffer>({
        .device = mDevice,
        .size = sizeof(RadianceVolumeCell) * GRID_RESOLUTION * GRID_RESOLUTION *
                GRID_RESOLUTION,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });
  }
  mBuffers.geometryVolume = makeUnique<vulkan::Buffer>({
      .device = mDevice,
      .size = sizeof(GeometryVolumeCell) * GRID_RESOLUTION * GRID_RESOLUTION *
              GRID_RESOLUTION,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
  });

  mLPVGrid.volume = mBuffers.radianceVolumeBuffers[0]->getDeviceAddress();
  mLPVGrid.volumeSwap = mBuffers.radianceVolumeBuffers[1]->getDeviceAddress();
  mLPVGrid.volumeAccu = mBuffers.radianceVolumeBuffers[2]->getDeviceAddress();
  mLPVGrid.geometryVolume = mBuffers.geometryVolume->getDeviceAddress();

  auto aabb = scene.getAABB();
  mLPVGrid.origin = aabb.min;
  mLPVGrid.cellSize = glm::length(aabb.max - aabb.min) / GRID_RESOLUTION;
  mLPVGrid.dimensions = GRID_RESOLUTION * glm::uvec3{1, 1, 1};

  // mLPVGrid.cellSize = 1.f;
  // mLPVGrid.origin = glm::vec3{0.1f, 0.1f, 0.1f} -
  //                   0.5f * glm::vec3{1.f, 1.f, 1.f} *
  //                       static_cast<float>(GRID_RESOLUTION) *
  //                       mLPVGrid.cellSize;

  // Initialize descriptors
  mLightDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
      .device = mDevice,
      .poolSizes =
          {
              {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
          },
  });

  mLightDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
      .device = mDevice,
      .bindings =
          {
              {
                  {.binding = 0,
                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
          },
  });

  mLightDescriptors.set = makeUnique<vulkan::DescriptorSet>({
      .device = mDevice,
      .pool = *mLightDescriptors.pool,
      .layout = *mLightDescriptors.layout,
  });

  // Occluder descriptors
  {
    constexpr uint32_t NUM_OCCLUDER_SETS = 8;
    mOccluderDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
        .device = mDevice,
        .poolSizes =
            {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
            },
        .maxSets = NUM_OCCLUDER_SETS,
        .perSet = true,
    });

    mOccluderDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
        .device = mDevice,
        .bindings =
            {
                {
                    {.binding = 0,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
            },
    });

    mOccluderDescriptors.sets.resize(NUM_OCCLUDER_SETS);
    for (auto& set : mOccluderDescriptors.sets) {
      set = makeUnique<vulkan::DescriptorSet>({
          .device = mDevice,
          .pool = *mOccluderDescriptors.pool,
          .layout = *mOccluderDescriptors.layout,
      });
    }
  }
}

void LightPropagationVolumePass::reloadShaders(
    vulkan::ShaderLibrary& shaderLibrary) {
  {  // Inject light pass
    mInjectLightPass.pipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {&*mLightDescriptors.layout},
        .pushConstants = {{.stageFlags = VK_SHADER_STAGE_ALL,
                           .size = sizeof(InjectPush)}},
    });

    const auto& shaderData = shaderLibrary.loadShader(kInjectLightShader);
    mInjectLightPass.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mInjectLightPass.pipelineLayout,
        .shader = *shaderData.shader,
    });
    mInjectLightPass.workgroupSize =
        shaderData.reflection.workgroupSize.value();
  }

  {  // Inject geometry pass
    mInjectGeometryPass.pipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {&*mOccluderDescriptors.layout},
        .pushConstants = {{.stageFlags = VK_SHADER_STAGE_ALL,
                           .size = sizeof(InjectPush)}},
    });

    const auto& shaderData = shaderLibrary.loadShader(kInjectGeometryShader);
    mInjectGeometryPass.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mInjectGeometryPass.pipelineLayout,
        .shader = *shaderData.shader,
    });
    mInjectGeometryPass.workgroupSize =
        shaderData.reflection.workgroupSize.value();
  }

  {  // Propagation pass
    mPropagationPass.pipelineLayout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {},
        .pushConstants = {{.stageFlags = VK_SHADER_STAGE_ALL,
                           .size = sizeof(PropagationPush)}},
    });

    const auto& shaderData = shaderLibrary.loadShader(kPropagationShader);
    mPropagationPass.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mPropagationPass.pipelineLayout,
        .shader = *shaderData.shader,
    });
    mPropagationPass.workgroupSize =
        shaderData.reflection.workgroupSize.value();
  }
}

void LightPropagationVolumePass::setRSM(const RSM& rsm) {
  mRSMResolution = {rsm.position->getExtent().width,
                    rsm.position->getExtent().height};

  // Add RSM for injection
  {
    vulkan::DescriptorSet::Resources resources;
    resources.images[0][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = rsm.position->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[1][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = rsm.normal->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[2][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = rsm.flux->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    mLightDescriptors.set->update(resources);
  }

  // Add RSM for occlusion
  addOccluder(rsm.position->getView(), rsm.normal->getView());
}

void LightPropagationVolumePass::addOccluder(const vulkan::ImageView& position,
                                             const vulkan::ImageView& normal) {
  vulkan::DescriptorSet::Resources resources;
  resources.images[0][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = position.vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  resources.images[1][0] = {
      .sampler = mSampler->vkHandle(),
      .imageView = normal.vkHandle(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  mOccluderDescriptors.sets.at(mNumOccluders)->update(resources);
  // FIXME: conversion to signed int because of imgui
  mNumOccluders++;
}

void LightPropagationVolumePass::execute(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "LPVPass::execute");

  for (const auto& buffer : mBuffers.radianceVolumeBuffers) {
    commandBuffer.fillBuffer(*buffer, 0);
  }
  // TODO: maybe keep geometry volume around for more frames?
  commandBuffer.fillBuffer(*mBuffers.geometryVolume, 0);

  if (mSettings.numIterations == 0) {
    return;
  }

  commandBuffer.memoryBarrier(
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

  injectLight(commandBuffer, currentFrame);
  injectGeometry(commandBuffer, currentFrame);

  commandBuffer.memoryBarrier(
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

  propagate(commandBuffer, currentFrame);
}

void LightPropagationVolumePass::injectLight(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "LPVPass::injectLight");

  commandBuffer.bindPipeline(*mInjectLightPass.pipeline);
  commandBuffer.bindDescriptorSet(
      *mInjectLightPass.pipeline, *mLightDescriptors.set, 0);

  InjectPush p{
      .lpvGrid = mLPVGrid,
  };

  commandBuffer.pushConstants(*mInjectLightPass.pipelineLayout, p);

  commandBuffer.dispatch(
      {vulkan::dispatchSize(mInjectLightPass.workgroupSize.x, mRSMResolution.x),
       vulkan::dispatchSize(mInjectLightPass.workgroupSize.y, mRSMResolution.y),
       1});
}

void LightPropagationVolumePass::injectGeometry(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(
      currentFrame, commandBuffer, "LPVPass::injectGeometry");

  commandBuffer.bindPipeline(*mInjectGeometryPass.pipeline);

  InjectPush p{
      .lpvGrid = mLPVGrid,
  };

  commandBuffer.pushConstants(*mInjectGeometryPass.pipelineLayout, p);

  uint32_t numOccluders = mSettings.onlyRSMOccluder ? 1 : mNumOccluders;
  for (uint32_t i = 0; i < numOccluders; i++) {
    commandBuffer.bindDescriptorSet(
        *mInjectGeometryPass.pipeline, *mOccluderDescriptors.sets[i], 0);
    commandBuffer.dispatch(
        {vulkan::dispatchSize(mInjectGeometryPass.workgroupSize.x,
                              mRSMResolution.x),
         vulkan::dispatchSize(mInjectGeometryPass.workgroupSize.y,
                              mRSMResolution.y),
         1});
  }
}

void LightPropagationVolumePass::propagate(
    const vulkan::CommandBuffer& commandBuffer,
    vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "LPVPass::propagation");

  commandBuffer.bindPipeline(*mPropagationPass.pipeline);

  PropagationPush p{
      .iteration = 0,
      .lpvGrid = mLPVGrid,
  };

  for (uint32_t i = 0; i < mSettings.numIterations; i++) {
    p.iteration = i;

    commandBuffer.memoryBarrier(
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    commandBuffer.pushConstants(*mPropagationPass.pipelineLayout, p);

    commandBuffer.dispatch({
        vulkan::dispatchSize(mPropagationPass.workgroupSize.x,
                             mLPVGrid.dimensions.x),
        vulkan::dispatchSize(mPropagationPass.workgroupSize.y,
                             mLPVGrid.dimensions.y),
        vulkan::dispatchSize(mPropagationPass.workgroupSize.z,
                             mLPVGrid.dimensions.z),
    });

    std::swap(p.lpvGrid.volume, p.lpvGrid.volumeSwap);
  }

  commandBuffer.memoryBarrier(
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

}  // namespace recore::passes
