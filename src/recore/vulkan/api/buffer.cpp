#include "buffer.h"

#include <cstring>

namespace recore::vulkan {

Buffer::Buffer(const Desc& desc) : Object{desc.device}, mSize{desc.size} {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.flags = 0;
  bufferInfo.size = mSize;
  bufferInfo.usage = desc.usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  bufferInfo.queueFamilyIndexCount = 0;
  bufferInfo.pQueueFamilyIndices = nullptr;

  VmaAllocationCreateInfo memoryInfo{};
  memoryInfo.flags = desc.memoryFlags;
  memoryInfo.usage = desc.memoryUsage;

  VmaAllocationInfo allocationInfo{};
  if (desc.minAlignment == 0) {
    checkResult(vmaCreateBuffer(mDevice.getMemoryAllocator(),
                                &bufferInfo,
                                &memoryInfo,
                                &mHandle,
                                &mAllocation,
                                &allocationInfo));
  } else {
    checkResult(vmaCreateBufferWithAlignment(mDevice.getMemoryAllocator(),
                                             &bufferInfo,
                                             &memoryInfo,
                                             desc.minAlignment,
                                             &mHandle,
                                             &mAllocation,
                                             &allocationInfo));
  }

  mMemory = allocationInfo.deviceMemory;
}

Buffer::~Buffer() {
  unmap();
  vmaDestroyBuffer(mDevice.getMemoryAllocator(), mHandle, mAllocation);
}

void Buffer::upload(const void* data) {
  map();
  std::memcpy(mMappedData, data, mSize);
  flush();
  unmap();
}

void Buffer::download(void* data) {
  map();
  std::memcpy(data, mMappedData, mSize);
  unmap();
}

VkDeviceAddress Buffer::getDeviceAddress() const {
  VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
  bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  bufferDeviceAddressInfo.buffer = mHandle;
  return vkGetBufferDeviceAddress(mDevice.vkHandle(), &bufferDeviceAddressInfo);
}

void Buffer::map() {
  if (mMappedData != nullptr) {
    return;  // Already mapped
  }

  checkResult(vmaMapMemory(mDevice.getMemoryAllocator(),
                           mAllocation,
                           reinterpret_cast<void**>(&mMappedData)));
}

void Buffer::unmap() {
  if (mMappedData == nullptr) {
    return;  // Not mapped
  }

  vmaUnmapMemory(mDevice.getMemoryAllocator(), mAllocation);
  mMappedData = nullptr;
}

void Buffer::flush() {
  vmaFlushAllocation(mDevice.getMemoryAllocator(), mAllocation, 0, mSize);
}

}  // namespace recore::vulkan
