#include "context.h"

#include <recore/vulkan/debug.h>

namespace recore::vulkan {

RenderFrame::RenderFrame(const Device& device)
    : mDevice{device},
      mCommandPool{
          {.device = mDevice,
           .queueFamilyIndex = mDevice.getGraphicsQueue().getFamilyIndex()}},
      mCommandBuffer{{.device = mDevice, .commandPool = mCommandPool}},
      mSemaphorePool{device},
      mFencePool{device},
      mTimestampQueryPool{{.device = mDevice}} {
  device.submitAndWait([&](const CommandBuffer& commandBuffer) {
    commandBuffer.resetTimestampPool(mTimestampQueryPool);
  });
}

void RenderFrame::reset() {
  mSemaphorePool.reset();

  mFencePool.wait();
  mFencePool.reset();

  mGarbage.buffers.clear();

  mTimestampQueryPool.loadResults();
}

Semaphore& RenderFrame::requestSemaphore() {
  return mSemaphorePool.request();
}

uPtr<Semaphore> RenderFrame::requestSemaphoreWithOwnership() {
  return mSemaphorePool.requestWithOwnership();
}

void RenderFrame::releaseOwnedSemaphore(uPtr<Semaphore> semaphore) {
  mSemaphorePool.releaseOwned(std::move(semaphore));
}

Fence& RenderFrame::requestFence() {
  return mFencePool.request();
}

RenderContext::RenderContext(const Desc& desc)
    : mDevice{desc.device},
      mSurface{desc.surface},
      mSurfaceExtent{desc.width, desc.height},
      mNumFramesInFlight{desc.numFramesInFlight} {
  mSwapchain = makeUnique<Swapchain>({.device = mDevice,
                                      .surface = desc.surface,
                                      .width = desc.width,
                                      .height = desc.height});

  mRenderFrames.resize(mNumFramesInFlight);
  for (auto& frame : mRenderFrames) {
    frame = makeUnique<RenderFrame>(mDevice);
  }
}

RenderFrame& RenderContext::beginFrame() {
  mFrameRecordBuffer.clear();

  mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mRenderFrames.size();
  auto& frame = getCurrentFrame();
  frame.reset();

  return frame;
}

void RenderContext::submit(FrameRecorder&& recorder) {
  mFrameRecordBuffer.push_back(std::move(recorder));
}

void RenderContext::endFrame(const Image& finalImage) {
  RenderFrame& frame = getCurrentFrame();
  const auto& commandBuffer = frame.getCommandBuffer();

  // Record command buffer
  commandBuffer.begin();
  commandBuffer.resetTimestampPool(frame.getTimestampQueryPool());
  for (auto& recorder : mFrameRecordBuffer) {
    recorder(commandBuffer);
  }

  auto& imageAcquireSemaphore = frame.requestSemaphore();
  uint32_t nextSwapchainImageIndex = 0;
  checkResult(mSwapchain->acquireNextImage(&nextSwapchainImageIndex,
                                           imageAcquireSemaphore));

  {  // Blit to swapchain image
    const auto& swapchainImage = mSwapchain->getImage(nextSwapchainImageIndex);

    commandBuffer.transitionImageLayout(
        finalImage,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.transitionImageLayout(swapchainImage,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);

    commandBuffer.blitImage(finalImage, swapchainImage);

    commandBuffer.transitionImageLayout(swapchainImage,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                                        VK_PIPELINE_STAGE_HOST_BIT);
  }

  commandBuffer.end();

  // Submit
  Semaphore& renderFinishedSemaphore = frame.requestSemaphore();
  Fence& inFlightFence = frame.requestFence();

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitInfo.pWaitDstStageMask = &waitStage;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = imageAcquireSemaphore.vkPtr();
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = renderFinishedSemaphore.vkPtr();

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = commandBuffer.vkPtr();

  checkResult(mDevice.getGraphicsQueue().submit({submitInfo},
                                                inFlightFence.vkHandle()));

  // Present

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = renderFinishedSemaphore.vkPtr();
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = mSwapchain->vkPtr();
  presentInfo.pImageIndices = &nextSwapchainImageIndex;
  presentInfo.pResults = nullptr;

  checkResult(mDevice.getGraphicsQueue().present(presentInfo));
}

void RenderContext::resize(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return;
  }
  if (mSurfaceExtent.width == width && mSurfaceExtent.height == height) {
    return;
  }

  mSurfaceExtent = {width, height};
  recreateSwapchain();
}

bool RenderContext::checkSurfaceUpdate() {
  VkSurfaceCapabilitiesKHR capabilities =
      mDevice.getPhysicalDevice().getSurfaceCapabilities(mSurface.vkHandle());

  VkExtent2D currentExtent = capabilities.currentExtent;

  constexpr uint32_t MAX_EXTENT = 0xFFFFFFFF;
  if (currentExtent.width == MAX_EXTENT) {
    return false;
  }

  if (currentExtent.width != mSurfaceExtent.width ||
      currentExtent.height != mSurfaceExtent.height) {
    recreateSwapchain();
    mSurfaceExtent = currentExtent;

    return true;
  }
  return false;
}

void RenderContext::recreateSwapchain() {
  if (mSurfaceExtent.width == 0 && mSurfaceExtent.height == 0) {
    return;
  }

  // Before recreating, make sure the device is not used anymore.
  vulkan::checkResult(mDevice.waitIdle());

  // Recreate swapchain.
  if (mSwapchain != nullptr) {
    mSwapchain = makeUnique<Swapchain>({.device = mDevice,
                                        .surface = mSurface,
                                        .width = mSurfaceExtent.width,
                                        .height = mSurfaceExtent.height,
                                        .oldSwapchain = mSwapchain.get()});
  }
}

HeadlessContext::HeadlessContext(const Desc& desc)
    : mDevice{desc.device}, mNumFramesInFlight{desc.numFramesInFlight} {
  mRenderFrames.resize(mNumFramesInFlight);
  for (auto& frame : mRenderFrames) {
    frame = makeUnique<RenderFrame>(mDevice);
  }
}

RenderFrame& HeadlessContext::beginFrame() {
  mFrameRecordBuffer.clear();

  mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mRenderFrames.size();
  auto& frame = getCurrentFrame();
  frame.reset();

  return frame;
}

void HeadlessContext::submit(FrameRecorder&& recorder) {
  mFrameRecordBuffer.push_back(std::move(recorder));
}

void HeadlessContext::endFrame(const Image& finalImage) {
  RenderFrame& frame = getCurrentFrame();
  const auto& commandBuffer = frame.getCommandBuffer();

  // Record command buffer
  commandBuffer.begin();
  for (auto& recorder : mFrameRecordBuffer) {
    recorder(commandBuffer);
  }
  commandBuffer.end();

  checkResult(mDevice.getGraphicsQueue().submit(
      {commandBuffer}, frame.requestFence().vkHandle()));
}

}  // namespace recore::vulkan
