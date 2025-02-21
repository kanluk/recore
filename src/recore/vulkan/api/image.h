#pragma once

#include "device.h"

namespace recore::vulkan {

class ImageView;

class Image : public Object<VkImage> {
 public:
  struct Desc {
    const Device& device;
    VkImageType type = VK_IMAGE_TYPE_2D;
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent3D extent{0, 0, 1};
    VkImageUsageFlags usage{};
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    uint32_t mipLevel = 1;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
  };

  explicit Image(const Desc& desc);
  ~Image() override;

  Image(const Desc& desc, VkImage handle);

  [[nodiscard]] VkFormat getFormat() const { return mFormat; }

  [[nodiscard]] VkImageType getType() const { return mType; }

  [[nodiscard]] VkImageSubresource getSubresource() const {
    return mSubresource;
  }

  [[nodiscard]] VkExtent3D getExtent() const { return mExtent; }

  [[nodiscard]] uint32_t getWidth() const { return mExtent.width; }

  [[nodiscard]] uint32_t getHeight() const { return mExtent.height; }

  [[nodiscard]] VkImageAspectFlags getAspect() const {
    return mSubresource.aspectMask;
  }

  [[nodiscard]] uint32_t getMipLevel() const { return mSubresource.mipLevel; }

  [[nodiscard]] const ImageView& getView() const { return *mView; }

 private:
  VmaAllocation mMemory{VK_NULL_HANDLE};

  VkImageType mType{VK_IMAGE_TYPE_2D};
  VkFormat mFormat{VK_FORMAT_UNDEFINED};
  VkExtent3D mExtent{0, 0, 1};
  VkImageUsageFlags mUsage{};
  VmaMemoryUsage mMemoryUsage{VMA_MEMORY_USAGE_GPU_ONLY};
  VkSampleCountFlagBits mSampleCount{VK_SAMPLE_COUNT_1_BIT};
  VkImageSubresource mSubresource{VK_IMAGE_ASPECT_NONE, 1, 1};

  uPtr<ImageView> mView;
};

class ImageView : public Object<VkImageView> {
 public:
  struct Desc {
    const Device& device;
    const Image& image;
  };

  explicit ImageView(const Desc& desc);
  ~ImageView() override;

  [[nodiscard]] VkImageSubresourceRange getSubresourceRange() const {
    return mSubresourceRange;
  }

 private:
  const Image& mImage;

  VkImageSubresourceRange mSubresourceRange{};
};

class Sampler : public Object<VkSampler> {
 public:
  struct Desc {
    const Device& device;
    VkFilter magFilter{VK_FILTER_NEAREST};
    VkFilter minFilter{VK_FILTER_NEAREST};
    VkSamplerMipmapMode mipmapMode{VK_SAMPLER_MIPMAP_MODE_NEAREST};
    VkSamplerAddressMode addressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};
  };

  explicit Sampler(const Desc& desc);
  ~Sampler() override;
};

}  // namespace recore::vulkan
