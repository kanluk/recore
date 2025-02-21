#pragma once

#include "device.h"

namespace recore::vulkan {

class Buffer : public Object<VkBuffer> {
 public:
  struct Desc {
    const Device& device;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage{};
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    VmaAllocationCreateFlags memoryFlags = 0;
    VkDeviceSize minAlignment = 0;
  };

  explicit Buffer(const Desc& desc);
  ~Buffer() override;

  [[nodiscard]] VkDeviceSize getSize() const { return mSize; }

  [[nodiscard]] VkDeviceAddress getDeviceAddress() const;

  void upload(const void* data);
  void download(void* data);

  void map();
  void unmap();
  void flush();

  [[nodiscard]] uint8_t* getMappedData() { return mMappedData; }

 private:
  VmaAllocation mAllocation{VK_NULL_HANDLE};
  VkDeviceMemory mMemory{VK_NULL_HANDLE};
  VkDeviceSize mSize{0};

  uint8_t* mMappedData{nullptr};
};

}  // namespace recore::vulkan
