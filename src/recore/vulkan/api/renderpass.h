#pragma once

#include "device.h"
#include "image.h"

namespace recore::vulkan {

struct Attachment {
  VkFormat format{VK_FORMAT_UNDEFINED};
  VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT};
  VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
  VkImageLayout initialLayout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkAttachmentLoadOp loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR};
  VkAttachmentStoreOp storeOp{VK_ATTACHMENT_STORE_OP_STORE};
};

class RenderPass : public Object<VkRenderPass> {
 public:
  struct Desc {
    const Device& device;
    std::vector<Attachment> attachments;
  };

  explicit RenderPass(const Desc& desc);
  ~RenderPass() override;
};

class Framebuffer : public Object<VkFramebuffer> {
 public:
  struct Desc {
    const Device& device;
    const RenderPass& renderPass;
    std::vector<const ImageView*> attachments;
    uint32_t width;
    uint32_t height;
  };

  explicit Framebuffer(const Desc& desc);
  ~Framebuffer() override;

  [[nodiscard]] uint32_t getWidth() const { return mWidth; }

  [[nodiscard]] uint32_t getHeight() const { return mHeight; }

 private:
  const RenderPass& mRenderPass;
  std::vector<const ImageView*> mAttachments;
  uint32_t mWidth{0};
  uint32_t mHeight{0};
};

}  // namespace recore::vulkan
