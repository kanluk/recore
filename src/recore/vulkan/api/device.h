#pragma once

#include "instance.h"
#include "object.h"

#include <recore/core/base.h>

// #define VK_NO_PROTOTYPES
#include <vk_mem_alloc.h>

#include <map>
#include <optional>

namespace recore::vulkan {

class CommandBuffer;
class Queue;

// Cursed feature map inspired by:
// https://github.com/KhronosGroup/Vulkan-Samples/blob/main/framework/core/physical_device.h
class FeatureMap {
 public:
  template <typename FeatureType, VkStructureType sType>
  void addFeature(VkBool32 FeatureType::*flag) {
    addFeatures<FeatureType, sType>({flag});
  }

  template <typename FeatureType, VkStructureType sType>
  void addFeatures(std::vector<VkBool32 FeatureType::*> flag) {
    auto [it, inserted] = mFeatures.insert(
        {sType, makeShared<FeatureType>(FeatureType{.sType = sType})});
    if (inserted) {
      if (mLastInserted) {
        static_cast<FeatureType*>(it->second.get())->pNext = mLastInserted;
      }
      mLastInserted = it->second.get();
    }
    auto& feature = *static_cast<FeatureType*>(it->second.get());
    for (auto& f : flag) {
      feature.*f = VK_TRUE;
    }
  }

  [[nodiscard]] void* pNextChain() const { return mLastInserted; }

 private:
  std::map<VkStructureType, sPtr<void>> mFeatures;
  void* mLastInserted{nullptr};
};

class Device : public Object<VkDevice> {
 public:
  struct Features {
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceVulkan11Features features11{};
    VkPhysicalDeviceVulkan12Features features12{};
    VkPhysicalDeviceVulkan13Features features13{};
    FeatureMap featureMap;

    void* pNextChain{nullptr};

    void finalize() {
      features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
      features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
      features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

      pNextChain = &features11;
      features11.pNext = &features12;
      features12.pNext = &features13;
      features13.pNext = featureMap.pNextChain();
    }
  };

  struct Desc {
    const Instance& instance;
    const PhysicalDevice& physicalDevice;
    std::optional<VkSurfaceKHR> surface;
    std::vector<std::string> extensions;
    Features features;
    // TODO: debug utils
  };

  explicit Device(const Desc& desc);
  ~Device() override;

  [[nodiscard]] const Instance& getInstance() const { return mInstance; }

  [[nodiscard]] const PhysicalDevice& getPhysicalDevice() const {
    return mPhysicalDevice;
  }

  [[nodiscard]] VmaAllocator getMemoryAllocator() const {
    return mMemoryAllocator;
  }

  [[nodiscard]] Queue& getGraphicsQueue() const;
  [[nodiscard]] Queue& getComputeQueue() const;

  [[nodiscard]] VkResult waitIdle() const;

  using CommandRecorder =
      std::function<void(const CommandBuffer& commandBuffer)>;
  void submitAndWait(const CommandRecorder& recorder) const;

 private:
  const Instance& mInstance;
  const PhysicalDevice& mPhysicalDevice;

  VmaAllocator mMemoryAllocator{VK_NULL_HANDLE};

  std::vector<std::vector<uPtr<Queue>>> queues;
};

class Queue : public Object<VkQueue> {
 public:
  struct Desc {
    const Device& device;
    uint32_t familyIndex;
    VkQueueFamilyProperties properties;
    uint32_t queueIndex;
    bool canPresent;
  };

  explicit Queue(const Desc& desc);
  ~Queue() override = default;

  [[nodiscard]] VkQueueFamilyProperties getProperties() const {
    return mProperties;
  }

  [[nodiscard]] bool canPresent() const { return mCanPresent; }

  [[nodiscard]] uint32_t getFamilyIndex() const { return mFamilyIndex; }

  [[nodiscard]] VkResult submit(const CommandBuffer& commandBuffer,
                                VkFence fence = VK_NULL_HANDLE) const;
  [[nodiscard]] VkResult submit(const std::vector<VkSubmitInfo>& submits,
                                VkFence fence = VK_NULL_HANDLE) const;
  [[nodiscard]] VkResult present(const VkPresentInfoKHR& presentInfo) const;

  [[nodiscard]] VkResult waitIdle() const;

 private:
  uint32_t mFamilyIndex{0};
  VkQueueFamilyProperties mProperties{};
  uint32_t mQueueIndex{0};
  bool mCanPresent{false};
};

}  // namespace recore::vulkan
