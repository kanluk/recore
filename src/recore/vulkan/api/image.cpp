#include "image.h"

#include <cmath>

namespace recore::vulkan {

static uint32_t getMaxMipLevel(uint32_t width, uint32_t height) {
  return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) +
         1;
}

static VkImageType getImageTypeFromExtent(const VkExtent3D& extent) {
  uint32_t dimensions = 0;
  if (extent.width > 0) {
    dimensions++;
  }
  if (extent.height > 0) {
    dimensions++;
  }
  if (extent.depth > 1) {
    dimensions++;
  }

  switch (dimensions) {
    case 1:
      return VK_IMAGE_TYPE_1D;
    case 2:
      return VK_IMAGE_TYPE_2D;
    case 3:
      return VK_IMAGE_TYPE_3D;
    default:
      throw VulkanException(VK_INCOMPLETE, "getImageType invalid");
  }
}

static VkImageAspectFlags formatToAspect(VkFormat format) {
  return isDepthStencilFormat(format) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                      : VK_IMAGE_ASPECT_COLOR_BIT;
}

Image::Image(const Desc& desc)
    : Object{desc.device},
      mType{desc.type},
      mFormat{desc.format},
      mExtent{desc.extent},
      mUsage{desc.usage},
      mMemoryUsage{desc.memoryUsage},
      mSampleCount{desc.sampleCount} {
  mSubresource.aspectMask = formatToAspect(mFormat);
  mSubresource.mipLevel = std::min(
      desc.mipLevel, getMaxMipLevel(mExtent.width, mExtent.height));

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.flags = 0;
  imageInfo.imageType = mType;
  imageInfo.format = mFormat;
  imageInfo.extent = mExtent;
  imageInfo.mipLevels = mSubresource.mipLevel;
  imageInfo.arrayLayers = mSubresource.arrayLayer;
  imageInfo.samples = mSampleCount;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = mUsage;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.queueFamilyIndexCount = 0;
  imageInfo.pQueueFamilyIndices = nullptr;

  VmaAllocationCreateInfo memoryInfo{};
  memoryInfo.flags = 0;
  memoryInfo.usage = mMemoryUsage;

  checkResult(vmaCreateImage(mDevice.getMemoryAllocator(),
                             &imageInfo,
                             &memoryInfo,
                             &mHandle,
                             &mMemory,
                             nullptr));

  // Create default image view for ease of use
  mView = std::make_unique<ImageView>(
      ImageView::Desc{.device = mDevice, .image = *this});
}

Image::Image(const Desc& desc, VkImage handle)
    : Object{desc.device},
      mType{desc.type},
      mFormat{desc.format},
      mExtent{desc.extent},
      mUsage{desc.usage},
      mMemoryUsage{desc.memoryUsage},
      mSampleCount{desc.sampleCount} {
  mHandle = handle;
  mSubresource.aspectMask = formatToAspect(mFormat);
}

Image::~Image() {
  // Only delete if image is actually owned
  if (mHandle != VK_NULL_HANDLE && mMemory != VK_NULL_HANDLE) {
    vmaDestroyImage(mDevice.getMemoryAllocator(), mHandle, mMemory);
  }
}

static VkImageViewType getImageViewType(VkImageType type) {
  switch (type) {
    case VK_IMAGE_TYPE_1D:
      return VK_IMAGE_VIEW_TYPE_1D;
    case VK_IMAGE_TYPE_2D:
      return VK_IMAGE_VIEW_TYPE_2D;
    case VK_IMAGE_TYPE_3D:
      return VK_IMAGE_VIEW_TYPE_3D;
    default:
      throw VulkanException(VK_INCOMPLETE, "getImageViewType invalid");
  }
}

ImageView::ImageView(const Desc& desc)
    : Object{desc.device}, mImage{desc.image} {
  auto subresource = mImage.getSubresource();

  mSubresourceRange.aspectMask = subresource.aspectMask;
  mSubresourceRange.baseMipLevel = 0;
  mSubresourceRange.baseArrayLayer = 0;
  mSubresourceRange.levelCount = subresource.mipLevel;
  mSubresourceRange.layerCount = subresource.arrayLayer;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.flags = 0;
  viewInfo.image = mImage.vkHandle();
  viewInfo.viewType = getImageViewType(mImage.getType());
  viewInfo.format = mImage.getFormat();
  viewInfo.subresourceRange = mSubresourceRange;

  checkResult(
      vkCreateImageView(mDevice.vkHandle(), &viewInfo, nullptr, &mHandle));
}

ImageView::~ImageView() {
  vkDestroyImageView(mDevice.vkHandle(), mHandle, nullptr);
}

Sampler::Sampler(const Desc& desc) : Object{desc.device} {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = desc.magFilter;
  samplerInfo.minFilter = desc.minFilter;
  samplerInfo.mipmapMode = desc.mipmapMode;

  samplerInfo.addressModeU = desc.addressMode;
  samplerInfo.addressModeV = desc.addressMode;
  samplerInfo.addressModeW = desc.addressMode;

  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1;

  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

  checkResult(
      vkCreateSampler(mDevice.vkHandle(), &samplerInfo, nullptr, &mHandle));
}

Sampler::~Sampler() {
  vkDestroySampler(mDevice.vkHandle(), mHandle, nullptr);
}

}  // namespace recore::vulkan
