#include "command.h"

#include <array>

namespace recore::vulkan {

CommandPool::CommandPool(const Desc& desc)
    : Object{desc.device}, mQueueFamilyIndex{desc.queueFamilyIndex} {
  VkCommandPoolCreateInfo commandPoolInfo{};
  commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                          VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolInfo.queueFamilyIndex = mQueueFamilyIndex;

  checkResult(vkCreateCommandPool(
      mDevice.vkHandle(), &commandPoolInfo, nullptr, &mHandle));
}

CommandPool::~CommandPool() {
  vkDestroyCommandPool(mDevice.vkHandle(), mHandle, nullptr);
}

CommandBuffer::CommandBuffer(const Desc& desc)
    : Object{desc.device}, mCommandPool{desc.commandPool} {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = mCommandPool.vkHandle();
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  checkResult(
      vkAllocateCommandBuffers(mDevice.vkHandle(), &allocInfo, &mHandle));
}

CommandBuffer::~CommandBuffer() {
  vkFreeCommandBuffers(
      mDevice.vkHandle(), mCommandPool.vkHandle(), 1, &mHandle);
}

void CommandBuffer::begin(VkCommandBufferUsageFlags usage) const {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = usage;
  beginInfo.pInheritanceInfo = nullptr;

  checkResult(vkBeginCommandBuffer(mHandle, &beginInfo));
}

void CommandBuffer::end() const {
  checkResult(vkEndCommandBuffer(mHandle));
}

void CommandBuffer::beginRenderPass(
    const RenderPass& renderPass,
    const Framebuffer& framebuffer,
    const std::vector<VkClearValue>& clearValues) const {
  VkRenderPassBeginInfo renderPassBeginInfo{};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.renderPass = renderPass.vkHandle();
  renderPassBeginInfo.framebuffer = framebuffer.vkHandle();
  renderPassBeginInfo.renderArea.offset = {0, 0};
  renderPassBeginInfo.renderArea.extent = {framebuffer.getWidth(),
                                           framebuffer.getHeight()};
  renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(
      clearValues.size());
  renderPassBeginInfo.pClearValues = clearValues.data();

  vkCmdBeginRenderPass(
      mHandle, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandBuffer::endRenderPass() const {
  vkCmdEndRenderPass(mHandle);
}

void CommandBuffer::bindPipeline(const Pipeline& pipeline) const {
  vkCmdBindPipeline(mHandle, pipeline.getBindPoint(), pipeline.vkHandle());
}

void CommandBuffer::setViewport(const VkViewport& viewport) const {
  vkCmdSetViewport(mHandle, 0, 1, &viewport);
}

void CommandBuffer::setScissor(const VkRect2D& scissor) const {
  vkCmdSetScissor(mHandle, 0, 1, &scissor);
}

void CommandBuffer::setViewportAndScissor(uint32_t width,
                                          uint32_t height) const {
  setViewport({0.f,
               0.f,
               static_cast<float>(width),
               static_cast<float>(height),
               0.f,
               1.f});
  setScissor({0, 0, width, height});
}

void CommandBuffer::setFramebufferSize(uint32_t width, uint32_t height) const {
  setViewportAndScissor(width, height);
}

void CommandBuffer::bindVertexBuffer(const Buffer& buffer,
                                     uint32_t binding,
                                     uint32_t count) const {
  std::array<VkDeviceSize, 1> offsets = {0};
  vkCmdBindVertexBuffers(
      mHandle, binding, count, buffer.vkPtr(), offsets.data());
}

void CommandBuffer::bindIndexBuffer(const Buffer& buffer,
                                    VkDeviceSize offset,
                                    VkIndexType indexType) const {
  vkCmdBindIndexBuffer(mHandle, buffer.vkHandle(), offset, indexType);
}

void CommandBuffer::pushConstants(const PipelineLayout& layout,
                                  uint32_t size,
                                  void* data,
                                  VkShaderStageFlags stage) const {
  vkCmdPushConstants(mHandle, layout.vkHandle(), stage, 0, size, data);
}

void CommandBuffer::bindDescriptorSet(const PipelineLayout& layout,
                                      const DescriptorSet& descriptorSet,
                                      uint32_t set,
                                      VkPipelineBindPoint bindPoint) const {
  vkCmdBindDescriptorSets(mHandle,
                          bindPoint,
                          layout.vkHandle(),
                          set,
                          1,
                          descriptorSet.vkPtr(),
                          0,
                          nullptr);
}

void CommandBuffer::bindDescriptorSet(const Pipeline& pipeline,
                                      const DescriptorSet& descriptorSet,
                                      uint32_t set) const {
  vkCmdBindDescriptorSets(mHandle,
                          pipeline.getBindPoint(),
                          pipeline.getLayout().vkHandle(),
                          set,
                          1,
                          descriptorSet.vkPtr(),
                          0,
                          nullptr);
}

void CommandBuffer::draw(uint32_t vertexCount,
                         uint32_t instanceCount,
                         uint32_t firstVertex,
                         uint32_t firstInstance) const {
  vkCmdDraw(mHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::drawIndexed(uint32_t indexCount,
                                uint32_t instanceCount,
                                uint32_t firstIndex,
                                int32_t vertexOffset,
                                uint32_t firstInstance) const {
  vkCmdDrawIndexed(mHandle,
                   indexCount,
                   instanceCount,
                   firstIndex,
                   vertexOffset,
                   firstInstance);
}

void CommandBuffer::dispatch(DispatchDim dimensions) const {
  vkCmdDispatch(mHandle, dimensions.x, dimensions.y, dimensions.z);
}

void CommandBuffer::copyBufferToBuffer(const Buffer& src,
                                       const Buffer& dst) const {
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = src.getSize();

  vkCmdCopyBuffer(mHandle, src.vkHandle(), dst.vkHandle(), 1, &copyRegion);
}

void CommandBuffer::copyBufferToBuffer(const Buffer& src,
                                       const Buffer& dst,
                                       BufferOffsets offsets) const {
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = offsets.srcOffset;
  copyRegion.dstOffset = offsets.dstOffset;
  copyRegion.size = src.getSize();

  vkCmdCopyBuffer(mHandle, src.vkHandle(), dst.vkHandle(), 1, &copyRegion);
}

void CommandBuffer::copyBufferToImage(const Buffer& src,
                                      const Image& dst) const {
  VkBufferImageCopy copyRegion{};
  copyRegion.bufferOffset = 0;
  copyRegion.bufferRowLength = 0;
  copyRegion.bufferImageHeight = 0;

  copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.imageSubresource.mipLevel = 0;
  copyRegion.imageSubresource.baseArrayLayer = 0;
  copyRegion.imageSubresource.layerCount = 1;

  copyRegion.imageOffset = {0, 0, 0};
  copyRegion.imageExtent = dst.getExtent();

  vkCmdCopyBufferToImage(mHandle,
                         src.vkHandle(),
                         dst.vkHandle(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &copyRegion);
}

void CommandBuffer::copyImageToBuffer(const Image& src,
                                      const Buffer& dst) const {
  VkBufferImageCopy copyRegion{};
  copyRegion.bufferOffset = 0;
  copyRegion.bufferRowLength = 0;
  copyRegion.bufferImageHeight = 0;

  copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.imageSubresource.mipLevel = 0;
  copyRegion.imageSubresource.baseArrayLayer = 0;
  copyRegion.imageSubresource.layerCount = 1;

  copyRegion.imageOffset = {0, 0, 0};
  copyRegion.imageExtent = src.getExtent();

  vkCmdCopyImageToBuffer(mHandle,
                         src.vkHandle(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         dst.vkHandle(),
                         1,
                         &copyRegion);
}

void CommandBuffer::copyImageToImage(const Image& src, const Image& dst) const {
  VkImageCopy copyRegion{};
  copyRegion.srcSubresource.aspectMask = src.getAspect();
  copyRegion.srcSubresource.mipLevel = 0;
  copyRegion.srcSubresource.baseArrayLayer = 0;
  copyRegion.srcSubresource.layerCount = 1;
  copyRegion.srcOffset = {0, 0, 0};

  copyRegion.dstSubresource.aspectMask = dst.getAspect();
  copyRegion.dstSubresource.mipLevel = 0;
  copyRegion.dstSubresource.baseArrayLayer = 0;
  copyRegion.dstSubresource.layerCount = 1;
  copyRegion.dstOffset = {0, 0, 0};

  copyRegion.extent = src.getExtent();

  vkCmdCopyImage(mHandle,
                 src.vkHandle(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dst.vkHandle(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 1,
                 &copyRegion);
}

void CommandBuffer::fillBuffer(const Buffer& buffer,
                               VkDeviceSize offset,
                               VkDeviceSize size,
                               uint32_t data) const {
  vkCmdFillBuffer(mHandle, buffer.vkHandle(), offset, size, data);
}

void CommandBuffer::blitImage(const Image& src, const Image& dst) const {
  VkImageBlit region{};

  auto srcExtent = src.getExtent();

  region.srcSubresource.aspectMask = src.getAspect();
  region.srcSubresource.mipLevel = 0;
  region.srcSubresource.baseArrayLayer = 0;
  region.srcSubresource.layerCount = 1;
  region.srcOffsets[0] = VkOffset3D{0, 0, 0};
  region.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(srcExtent.width),
                                    static_cast<int32_t>(srcExtent.height),
                                    1};

  region.dstSubresource.aspectMask = src.getAspect();
  region.dstSubresource.mipLevel = 0;
  region.dstSubresource.baseArrayLayer = 0;
  region.dstSubresource.layerCount = 1;
  region.dstOffsets[0] = VkOffset3D{0, 0, 0};
  region.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(srcExtent.width),
                                    static_cast<int32_t>(srcExtent.height),
                                    1};

  vkCmdBlitImage(mHandle,
                 src.vkHandle(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dst.vkHandle(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 1,
                 &region,
                 VK_FILTER_NEAREST);
}

void CommandBuffer::clearColorImage(const Image& image,
                                    VkImageLayout layout,
                                    const VkClearColorValue& color) const {
  VkImageSubresourceRange range = image.getView().getSubresourceRange();
  vkCmdClearColorImage(mHandle, image.vkHandle(), layout, &color, 1, &range);
}

void CommandBuffer::imageMemoryBarrier(
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage,
    VkImageMemoryBarrier imageMemoryBarrier) const {
  vkCmdPipelineBarrier(mHandle,
                       srcStage,
                       dstStage,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &imageMemoryBarrier);
}

void CommandBuffer::transitionImageLayout(const Image& image,
                                          VkImageLayout srcLayout,
                                          VkImageLayout dstLayout,
                                          VkPipelineStageFlags srcStage,
                                          VkPipelineStageFlags dstStage) const {
  // Setup general memory barrier.
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = srcLayout;
  barrier.newLayout = dstLayout;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = image.vkHandle();

  barrier.subresourceRange.aspectMask = image.getAspect();
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = image.getMipLevel();
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  // Adapted from github.com/KhronosGroup/Vulkan-Samples -> vk_common.cpp
  switch (srcLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      // Image layout is undefined (or does not matter)
      // Only valid as initial layout
      // No flags required, listed only for completeness
      barrier.srcAccessMask = 0;
      break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      // Image is preinitialized
      // Only valid as initial layout for linear images, preserves memory
      // contents Make sure host writes have been finished
      barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      // Image is a color attachment
      // Make sure any writes to the color buffer have been finished
      barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      // Image is a depth/stencil attachment
      // Make sure any writes to the depth/stencil buffer have been finished
      barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      // Image is a transfer source
      // Make sure any reads from the image have been finished
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      // Image is a transfer destination
      // Make sure any writes to the image have been finished
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      // Image is read by a shader
      // Make sure any shader reads from the image have been finished
      barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_GENERAL:
      // Works to sync Compute to Graphics.
      barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      break;

    default:
      throw VulkanException(
          VK_ERROR_UNKNOWN,
          "Image::transitionLayout: Unsupported srcLayout transition!");
  }

  switch (dstLayout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      // Image will be used as a transfer destination
      // Make sure any writes to the image have been finished
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      // Image will be used as a transfer source
      // Make sure any reads from the image have been finished
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      // Image will be used as a color attachment
      // Make sure any writes to the color buffer have been finished
      barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      // Image layout will be used as a depth/stencil attachment
      // Make sure any writes to depth/stencil buffer have been finished
      barrier.dstAccessMask = barrier.dstAccessMask |
                              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      // Image will be read in a shader (sampler, input attachment)
      // Make sure any writes to the image have been finished
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      barrier.dstAccessMask = 0;
      break;

    case VK_IMAGE_LAYOUT_GENERAL:
      // Works to sync Graphics to Compute.
      barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      break;

    default:
      throw VulkanException(
          VK_ERROR_UNKNOWN,
          "Image::transitionLayout: Unsupported dstLayout transition!");
  }

  imageMemoryBarrier(srcStage, dstStage, barrier);
}

void CommandBuffer::transitionImageLayout(
    const std::vector<const Image*>& images,
    VkImageLayout srcLayout,
    VkImageLayout dstLayout,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage) const {
  for (const auto& image : images) {
    transitionImageLayout(*image, srcLayout, dstLayout, srcStage, dstStage);
  }
}

void CommandBuffer::memoryBarrier(VkAccessFlags srcAccessMask,
                                  VkAccessFlags dstAccessMask,
                                  VkPipelineStageFlagBits srcStage,
                                  VkPipelineStageFlagBits dstStage) const {
  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = srcAccessMask;
  barrier.dstAccessMask = dstAccessMask;
  vkCmdPipelineBarrier(
      mHandle, srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

};  // namespace recore::vulkan
