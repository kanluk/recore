#include "renderpass.h"

#include <array>

namespace recore::vulkan {

RenderPass::RenderPass(const Desc& desc) : Object{desc.device} {
  const auto& attachments = desc.attachments;
  std::vector<VkAttachmentDescription> attachmentsDescriptions{};

  // NOLINTNEXTLINE use explicit index operator just to be sure
  for (size_t i = 0; i < attachments.size(); i++) {
    VkAttachmentDescription outputDescription{};
    outputDescription.format = attachments[i].format;
    outputDescription.samples = attachments[i].samples;
    outputDescription.loadOp = attachments[i].loadOp;
    outputDescription.storeOp = attachments[i].storeOp;
    outputDescription.stencilLoadOp = attachments[i].loadOp;
    outputDescription.stencilStoreOp = attachments[i].storeOp;
    outputDescription.initialLayout = attachments[i].initialLayout;
    outputDescription.finalLayout =
        isDepthStencilFormat(attachments[i].format)
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachmentsDescriptions.push_back(outputDescription);
  }

  std::vector<VkAttachmentReference> outputAttachmentReferences{};
  VkAttachmentReference depthStencilAttachmentReference{};

  // TODO: only a single subpass for now
  VkSubpassDescription subpassDescription{};
  {
    bool foundDepth = false;
    for (size_t i = 0; i < attachments.size(); i++) {
      auto attachment = attachments[i];
      if (!isDepthStencilFormat(attachment.format)) {
        VkAttachmentReference outputReference{};
        outputReference.attachment = i;
        outputReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        outputAttachmentReferences.push_back(outputReference);
      } else {
        depthStencilAttachmentReference.attachment = i;
        depthStencilAttachmentReference.layout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        foundDepth = true;
      }
    }

    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = static_cast<uint32_t>(
        outputAttachmentReferences.size());
    subpassDescription.pColorAttachments = outputAttachmentReferences.data();

    subpassDescription.pDepthStencilAttachment =
        foundDepth ? &depthStencilAttachmentReference : nullptr;

    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
  }

  std::array<VkSubpassDependency, 2> dependencies{};

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_NONE_KHR;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(
      attachmentsDescriptions.size());
  renderPassInfo.pAttachments = attachmentsDescriptions.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpassDescription;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  checkResult(vkCreateRenderPass(
      mDevice.vkHandle(), &renderPassInfo, nullptr, &mHandle));
}

RenderPass::~RenderPass() {
  vkDestroyRenderPass(mDevice.vkHandle(), mHandle, nullptr);
}

Framebuffer::Framebuffer(const Desc& desc)
    : Object{desc.device},
      mRenderPass{desc.renderPass},
      mAttachments{desc.attachments},
      mWidth{desc.width},
      mHeight{desc.height} {
  std::vector<VkImageView> vkAttachments{mAttachments.size()};
  std::transform(
      mAttachments.begin(),
      mAttachments.end(),
      vkAttachments.begin(),
      [](const ImageView* view) -> VkImageView { return view->vkHandle(); });

  uint32_t maxLayers = 0;
  for (const auto& attachment : mAttachments) {
    if (attachment->getSubresourceRange().layerCount > maxLayers) {
      maxLayers = attachment->getSubresourceRange().layerCount;
    }
  }

  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = mRenderPass.vkHandle();
  framebufferInfo.attachmentCount = static_cast<uint32_t>(vkAttachments.size());
  framebufferInfo.pAttachments = vkAttachments.data();
  framebufferInfo.width = mWidth;
  framebufferInfo.height = mHeight;
  framebufferInfo.layers = maxLayers;

  checkResult(vkCreateFramebuffer(
      mDevice.vkHandle(), &framebufferInfo, nullptr, &mHandle));
}

Framebuffer::~Framebuffer() {
  vkDestroyFramebuffer(mDevice.vkHandle(), mHandle, nullptr);
}

}  // namespace recore::vulkan
