#pragma once

#include "buffer.h"
#include "device.h"
#include "image.h"
#include "pipeline.h"
#include "queries.h"
#include "renderpass.h"

namespace recore::vulkan {

class CommandPool : public Object<VkCommandPool> {
 public:
  struct Desc {
    const Device& device;
    uint32_t queueFamilyIndex;
  };

  explicit CommandPool(const Desc& desc);
  ~CommandPool() override;

 private:
  uint32_t mQueueFamilyIndex{0};
};

class CommandBuffer : public Object<VkCommandBuffer> {
 public:
  struct Desc {
    const Device& device;
    CommandPool& commandPool;
  };

  explicit CommandBuffer(const Desc& desc);
  ~CommandBuffer() override;

  void begin(VkCommandBufferUsageFlags usage = 0) const;
  void end() const;

  // Binding:
  void beginRenderPass(const RenderPass& renderPass,
                       const Framebuffer& framebuffer,
                       const std::vector<VkClearValue>& clearValues) const;
  void endRenderPass() const;

  void bindPipeline(const Pipeline& pipeline) const;

  // Dynamic pipeline states:
  void setViewport(const VkViewport& viewport) const;

  void setScissor(const VkRect2D& scissor) const;

  void setViewportAndScissor(uint32_t width, uint32_t height) const;

  void setFramebufferSize(uint32_t width, uint32_t height) const;

  void bindVertexBuffer(const Buffer& buffer,
                        uint32_t binding = 0,
                        uint32_t count = 1) const;

  void bindIndexBuffer(const Buffer& buffer,
                       VkDeviceSize offset = 0,
                       VkIndexType indexType = VK_INDEX_TYPE_UINT32) const;

  void pushConstants(const PipelineLayout& layout,
                     uint32_t size,
                     void* data,
                     VkShaderStageFlags stage = VK_SHADER_STAGE_ALL) const;

  template <typename T>
  void pushConstants(const PipelineLayout& layout,
                     const T& data,
                     VkShaderStageFlags stage = VK_SHADER_STAGE_ALL) const {
    pushConstants(layout, sizeof(T), const_cast<T*>(&data), stage);
  }

  void bindDescriptorSet(
      const PipelineLayout& layout,
      const DescriptorSet& descriptorSet,
      uint32_t set,
      VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS) const;

  void bindDescriptorSet(const Pipeline& pipeline,
                         const DescriptorSet& descriptorSet,
                         uint32_t set) const;

  // Drawing:
  void draw(uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t firstInstance = 0) const;

  void drawIndexed(uint32_t indexCount,
                   uint32_t instanceCount = 1,
                   uint32_t firstIndex = 0,
                   int32_t vertexOffset = 0,
                   uint32_t firstInstance = 0) const;

  // Dispatch:
  struct DispatchDim {
    uint32_t x = 1;
    uint32_t y = 1;
    uint32_t z = 1;
  };

  void dispatch(DispatchDim dimension) const;

  // Memory:
  void copyBufferToBuffer(const Buffer& src, const Buffer& dst) const;

  struct BufferOffsets {
    VkDeviceSize srcOffset = 0;
    VkDeviceSize dstOffset = 0;
  };

  void copyBufferToBuffer(const Buffer& src,
                          const Buffer& dst,
                          BufferOffsets offsets) const;

  void copyBufferToImage(const Buffer& src, const Image& dst) const;

  void copyImageToBuffer(const Image& src, const Buffer& dst) const;

  void copyImageToImage(const Image& src, const Image& dst) const;

  void fillBuffer(const Buffer& buffer,
                  VkDeviceSize offset = 0,
                  VkDeviceSize size = VK_WHOLE_SIZE,
                  uint32_t data = 0) const;

  void blitImage(const Image& src, const Image& dst) const;

  void clearColorImage(const Image& image,
                       VkImageLayout layout,
                       const VkClearColorValue& color) const;

  // Synchronization:
  void imageMemoryBarrier(VkPipelineStageFlags srcStage,
                          VkPipelineStageFlags dstStage,
                          VkImageMemoryBarrier imageMemoryBarrier) const;

  void transitionImageLayout(const Image& image,
                             VkImageLayout srcLayout,
                             VkImageLayout dstLayout,
                             VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage) const;

  void transitionImageLayout(const std::vector<const Image*>& images,
                             VkImageLayout srcLayout,
                             VkImageLayout dstLayout,
                             VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage) const;

  void memoryBarrier(VkAccessFlags srcAccessMask,
                     VkAccessFlags dstAccessMask,
                     VkPipelineStageFlagBits srcStage,
                     VkPipelineStageFlagBits dstStage) const;

  // Querys:
  void resetTimestampPool(const TimestampQueryPool& queryPool) const {
    vkCmdResetQueryPool(
        mHandle, queryPool.vkHandle(), 0, queryPool.getQueryCount());
  }

  void writeTimestamp(const TimestampQueryPool& queryPool,
                      uint32_t queryIndex,
                      VkPipelineStageFlagBits pipelineStage) const {
    vkCmdWriteTimestamp(
        mHandle, pipelineStage, queryPool.vkHandle(), queryIndex);
  }

  void startTimestamp(const TimestampQueryPool& queryPool,
                      uint32_t queryIndex) const {
    vkCmdWriteTimestamp(mHandle,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        queryPool.vkHandle(),
                        queryIndex);
  }

  void stopTimestamp(const TimestampQueryPool& queryPool,
                     uint32_t queryIndex) const {
    vkCmdWriteTimestamp(mHandle,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        queryPool.vkHandle(),
                        queryIndex);
  }

 private:
  const CommandPool& mCommandPool;
};

}  // namespace recore::vulkan
