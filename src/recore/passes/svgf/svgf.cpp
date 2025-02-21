#include "svgf.h"

#include "atrous.glslh"

#include <recore/vulkan/debug.h>

namespace recore::passes {

constexpr auto kReprojectionShader =
    "recore/passes/svgf/reprojection.comp.glsl";
constexpr auto kATrousShader = "recore/passes/svgf/atrous.comp.glsl";
constexpr auto kFinalizeShader = "recore/passes/svgf/finalize.comp.glsl";

SVGFPass::SVGFPass(const vulkan::Device& device) : Pass{device} {
  {  // Initialize reprojection descriptors
    mReprojectionDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
        .device = mDevice,
        .poolSizes =
            {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
            },
    });

    mReprojectionDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
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
                {
                    {.binding = 10,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 11,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 12,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 13,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 14,
                     .descriptorType =
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 20,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
                {
                    {.binding = 21,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
            },
    });

    mReprojectionDescriptors.set = makeUnique<vulkan::DescriptorSet>({
        .device = mDevice,
        .pool = *mReprojectionDescriptors.pool,
        .layout = *mReprojectionDescriptors.layout,
    });
  }

  {  // Initialize ATrous descriptors
    const uint32_t numATrousSets = mATrousDescriptors.sets.size();
    mATrousDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
        .device = mDevice,
        .poolSizes =
            {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            },
        .maxSets = numATrousSets,
        .multiplier = numATrousSets,
        .perSet = true,
    });

    mATrousDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
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

    for (auto& set : mATrousDescriptors.sets) {
      set = makeUnique<vulkan::DescriptorSet>({
          .device = mDevice,
          .pool = *mATrousDescriptors.pool,
          .layout = *mATrousDescriptors.layout,
      });
    }
  }

  {  // Initialize finalize descriptors
    mFinalizeDescriptors.pool = makeUnique<vulkan::DescriptorPool>({
        .device = mDevice,
        .poolSizes =
            {
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
            },
    });

    mFinalizeDescriptors.layout = makeUnique<vulkan::DescriptorSetLayout>({
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
                {
                    {.binding = 2,
                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     .descriptorCount = 1,
                     .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
                },
            },
    });

    mFinalizeDescriptors.set = makeUnique<vulkan::DescriptorSet>({
        .device = mDevice,
        .pool = *mFinalizeDescriptors.pool,
        .layout = *mFinalizeDescriptors.layout,
    });
  }

  mSampler = makeUnique<vulkan::Sampler>({.device = mDevice});
}

void SVGFPass::reloadShaders(vulkan::ShaderLibrary& shaderLibrary) {
  {  // Reprojection pipeline
    mReprojectionPipeline.layout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {&*mReprojectionDescriptors.layout},
        .pushConstants = {},
    });

    const auto& shaderData = shaderLibrary.loadShader(kReprojectionShader);
    mReprojectionPipeline.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mReprojectionPipeline.layout,
        .shader = *shaderData.shader,
    });
    mReprojectionPipeline.workgroupSize =
        shaderData.reflection.workgroupSize.value();
  }

  {  // ATrous pipeline
    mATrousPipeline.layout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {&*mATrousDescriptors.layout},
        .pushConstants = {{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .size = sizeof(ATrousPush),
        }},
    });

    const auto& shaderData = shaderLibrary.loadShader(kATrousShader);
    mATrousPipeline.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mATrousPipeline.layout,
        .shader = *shaderData.shader,
    });
    mATrousPipeline.workgroupSize = shaderData.reflection.workgroupSize.value();
  }

  {  // Finalize pipeline
    mFinalizePipeline.layout = makeUnique<vulkan::PipelineLayout>({
        .device = mDevice,
        .descriptorSetLayouts = {&*mFinalizeDescriptors.layout},
        .pushConstants = {},
    });

    const auto& shaderData = shaderLibrary.loadShader(kFinalizeShader);
    mFinalizePipeline.pipeline = makeUnique<vulkan::ComputePipeline>({
        .device = mDevice,
        .layout = *mFinalizePipeline.layout,
        .shader = *shaderData.shader,
    });
    mFinalizePipeline.workgroupSize =
        shaderData.reflection.workgroupSize.value();
  }
}

void SVGFPass::resize(uint32_t width, uint32_t height) {
  auto genImage = [&](VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT) {
    return makeUnique<vulkan::Image>({
        .device = mDevice,
        .format = format,
        .extent = {width, height, 1},
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
    });
  };

  // Ping-pong filtered images for a trous
  for (auto& image : mFilteredImages) {
    image = genImage();
  }

  mOutputImage = genImage();
  mIllumination = genImage();
  mHistoryLength = genImage(VK_FORMAT_R32_SFLOAT);
  mPrevPosition = genImage();
  mPrevNormal = genImage();
  mPrevAlbedo = genImage();
  mPrevIllumination = genImage();
  mPrevHistoryLength = genImage(VK_FORMAT_R32_SFLOAT);

  mDevice.submitAndWait([&](const auto& commandBuffer) {
    commandBuffer.transitionImageLayout({&*mFilteredImages[0],
                                         &*mFilteredImages[1],
                                         &*mOutputImage,
                                         &*mIllumination,
                                         &*mHistoryLength},
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.transitionImageLayout(
        {&*mPrevPosition,
         &*mPrevNormal,
         &*mPrevAlbedo,
         &*mPrevIllumination,
         &*mPrevHistoryLength},
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  });
}

void SVGFPass::execute(const vulkan::CommandBuffer& commandBuffer,
                       vulkan::RenderFrame& currentFrame) {
  RECORE_GPU_PROFILE_SCOPE(currentFrame, commandBuffer, "SVGFPass::execute");

  {  // Reprojection
    commandBuffer.transitionImageLayout(*mIllumination,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.bindPipeline(*mReprojectionPipeline.pipeline);
    commandBuffer.bindDescriptorSet(
        *mReprojectionPipeline.pipeline, *mReprojectionDescriptors.set, 0);

    vulkan::CommandBuffer::DispatchDim dispatchDim = {
        vulkan::dispatchSize(mReprojectionPipeline.workgroupSize.x,
                             mOutputImage->getWidth()),
        vulkan::dispatchSize(mReprojectionPipeline.workgroupSize.y,
                             mOutputImage->getHeight()),
        1,
    };
    commandBuffer.dispatch(dispatchDim);
  }

  {  // A Trous

    commandBuffer.transitionImageLayout(
        *mIllumination,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.transitionImageLayout(*mFilteredImages[0],
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    ATrousPush p{
        .phiColor = 512.f,
        .phiNormal = 0.05f,
        .phiPosition = 0.05f,
        .stepWidth = 1.f,
    };

    const float phiAttenuation = 0.5f;

    commandBuffer.bindPipeline(*mATrousPipeline.pipeline);

    vulkan::CommandBuffer::DispatchDim dispatchDim = {
        vulkan::dispatchSize(mATrousPipeline.workgroupSize.x,
                             mOutputImage->getWidth()),
        vulkan::dispatchSize(mATrousPipeline.workgroupSize.y,
                             mOutputImage->getHeight()),
        1,
    };

    // Only odd number of iterations to ensure that the final iteration goes to
    // mFilteredImages[0]
    const uint32_t numATrousIterations = 5;
    assert(numATrousIterations % 2 != 0);

    const uint32_t outID = ((numATrousIterations % 2) != 0) ? 0 : 1;
    for (uint32_t i = 0; i < numATrousIterations; ++i) {
      if (i == 0) {
        commandBuffer.bindDescriptorSet(
            *mATrousPipeline.pipeline, *mATrousDescriptors.sets.at(0), 0);
      } else {
        commandBuffer.bindDescriptorSet(
            *mATrousPipeline.pipeline,
            *mATrousDescriptors.sets.at(((i + 1) % 2) + 1),
            0);
      }

      commandBuffer.pushConstants(*mATrousPipeline.layout, p);

      commandBuffer.dispatch(dispatchDim);

      p.phiColor *= phiAttenuation;
      p.phiNormal *= phiAttenuation;
      p.phiPosition *= phiAttenuation;
      p.stepWidth *= 2.f;

      commandBuffer.transitionImageLayout(
          *mFilteredImages.at(i % 2),
          VK_IMAGE_LAYOUT_GENERAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

      commandBuffer.transitionImageLayout(*mFilteredImages.at((i + 1) % 2),
                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_GENERAL,
                                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

      if (i == 0) {
        // Use first filtered image as color history for next frame
        commandBuffer.transitionImageLayout(
            *mFilteredImages[0],
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        commandBuffer.transitionImageLayout(
            *mPrevIllumination,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        commandBuffer.blitImage(*mFilteredImages[0], *mPrevIllumination);

        commandBuffer.transitionImageLayout(
            *mFilteredImages[0],
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        commandBuffer.transitionImageLayout(
            *mPrevIllumination,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      }
    }
  }

  {  // Finalize
    commandBuffer.transitionImageLayout(*mOutputImage,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.bindPipeline(*mFinalizePipeline.pipeline);
    commandBuffer.bindDescriptorSet(
        *mFinalizePipeline.pipeline, *mFinalizeDescriptors.set, 0);

    vulkan::CommandBuffer::DispatchDim dispatchDim = {
        vulkan::dispatchSize(mFinalizePipeline.workgroupSize.x,
                             mOutputImage->getWidth()),
        vulkan::dispatchSize(mFinalizePipeline.workgroupSize.y,
                             mOutputImage->getHeight()),
        1,
    };
    commandBuffer.dispatch(dispatchDim);
  }

  {  // Blit to store current frame for next frame reprojection

    commandBuffer.transitionImageLayout(*mHistoryLength,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.transitionImageLayout(
        {mPosition, mNormal, mAlbedo},
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.transitionImageLayout(
        {&*mPrevHistoryLength, &*mPrevPosition, &*mPrevNormal, &*mPrevAlbedo},
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.blitImage(*mHistoryLength, *mPrevHistoryLength);
    commandBuffer.blitImage(*mPosition, *mPrevPosition);
    commandBuffer.blitImage(*mNormal, *mPrevNormal);
    commandBuffer.blitImage(*mAlbedo, *mPrevAlbedo);

    commandBuffer.transitionImageLayout(*mHistoryLength,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.transitionImageLayout(
        {mPosition, mNormal, mAlbedo},
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    commandBuffer.transitionImageLayout(
        {&*mPrevHistoryLength, &*mPrevPosition, &*mPrevNormal, &*mPrevAlbedo},
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  }
}

void SVGFPass::setInput(const Input& input) {
  {  // Reprojection
    vulkan::DescriptorSet::Resources resources;

    resources.images[0][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = input.gPosition.getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[1][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = input.gNormal.getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[2][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = input.gAlbedo.getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[3][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = input.gMotion.getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[4][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = input.noisyImage.getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    resources.images[10][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mPrevPosition->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[11][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mPrevNormal->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[12][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mPrevAlbedo->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[13][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mPrevIllumination->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[14][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mPrevHistoryLength->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    resources.images[20][0] = {
        .sampler = VK_NULL_HANDLE,
        .imageView = mIllumination->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    resources.images[21][0] = {
        .sampler = VK_NULL_HANDLE,
        .imageView = mHistoryLength->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    mReprojectionDescriptors.set->update(resources);
  }
  {  // A Trous
    vulkan::DescriptorSet::Resources resources;
    resources.images[0][0] = {
        .sampler = VK_NULL_HANDLE,
        .imageView = mFilteredImages[0]->getView().vkHandle(),
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
        .imageView = mIllumination->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    // Set for first round: gbuffer / reprojection -> filtered image [0]
    mATrousDescriptors.sets[0]->update(resources);

    resources.images[0][0] = {
        .sampler = VK_NULL_HANDLE,
        .imageView = mFilteredImages[1]->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    resources.images[3][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mFilteredImages[0]->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    // Set for the second round: filtered image [0] -> filtered image [1]
    mATrousDescriptors.sets[1]->update(resources);

    resources.images[0][0] = {
        .sampler = VK_NULL_HANDLE,
        .imageView = mFilteredImages[0]->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    resources.images[3][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mFilteredImages[1]->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    // Set for the third round: filtered image [1] -> filtered image [0]
    mATrousDescriptors.sets[2]->update(resources);
  }
  {  // Finalize
    vulkan::DescriptorSet::Resources resources;
    resources.images[0][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = input.gAlbedo.getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[1][0] = {
        .sampler = mSampler->vkHandle(),
        .imageView = mFilteredImages[0]->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    resources.images[2][0] = {
        .sampler = VK_NULL_HANDLE,
        .imageView = mOutputImage->getView().vkHandle(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    mFinalizeDescriptors.set->update(resources);
  }

  mPosition = &input.gPosition;
  mNormal = &input.gNormal;
  mAlbedo = &input.gAlbedo;
}
}  // namespace recore::passes
