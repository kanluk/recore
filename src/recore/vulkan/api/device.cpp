// #include <vulkan/vulkan.h>
#define VMA_IMPLEMENTATION

#include "device.h"
#include "command.h"

namespace recore::vulkan {

Device::Device(const Desc& desc)
    : Object{*this},
      mInstance{desc.instance},
      mPhysicalDevice{desc.physicalDevice} {
  const auto& queueFamilies = desc.physicalDevice.getQueueFamilies();

  // Give all queues same priority for now
  std::vector<VkDeviceQueueCreateInfo> queueInfos{queueFamilies.size()};
  std::vector<std::vector<float>> priorities{queueInfos.size()};

  for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilies.size();
       queueFamilyIndex++) {
    queueInfos[queueFamilyIndex].sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfos[queueFamilyIndex].queueFamilyIndex = queueFamilyIndex;
    queueInfos[queueFamilyIndex].queueCount =
        queueFamilies[queueFamilyIndex].queueCount;

    priorities[queueFamilyIndex].resize(
        queueFamilies[queueFamilyIndex].queueCount, 1.f);
    queueInfos[queueFamilyIndex].pQueuePriorities =
        priorities[queueFamilyIndex].data();
  }

  VkDeviceCreateInfo deviceInfo{};
  deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
  deviceInfo.pQueueCreateInfos = queueInfos.data();

  auto features = desc.features;
  features.finalize();
  VkPhysicalDeviceFeatures2 features2{};
  if (mInstance.isExtensionEnabled(
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features = features.features;
    features2.pNext = features.pNextChain;
    deviceInfo.pEnabledFeatures = nullptr;
    deviceInfo.pNext = &features2;
  } else {
    deviceInfo.pEnabledFeatures = &features.features;
  }

  auto extensionNames = strvec_to_cchar(desc.extensions);
  deviceInfo.enabledExtensionCount = static_cast<uint32_t>(
      extensionNames.size());
  deviceInfo.ppEnabledExtensionNames = extensionNames.data();

  checkResult(vkCreateDevice(
      mPhysicalDevice.vkHandle(), &deviceInfo, nullptr, &mHandle));

  // Load queues
  queues.resize(queueInfos.size());
  for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilies.size();
       queueFamilyIndex++) {
    const auto& queueFamily = queueFamilies[queueFamilyIndex];
    // Make canPresent true if no surface is provided
    bool canPresent = !desc.surface.has_value() ||
                      mPhysicalDevice.isPresentSupported(desc.surface.value(),
                                                         queueFamilyIndex);

    for (uint32_t queueIndex = 0; queueIndex < queueFamily.queueCount;
         queueIndex++) {
      queues[queueFamilyIndex].emplace_back(
          makeUnique<Queue>({.device = *this,
                             .familyIndex = queueFamilyIndex,
                             .properties = queueFamily,
                             .queueIndex = queueIndex,
                             .canPresent = canPresent}));
    }
  }

  // Setup vulkan memory allocator
  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = mPhysicalDevice.vkHandle();
  allocatorInfo.device = mDevice.vkHandle();
  allocatorInfo.instance = mInstance.vkHandle();
  allocatorInfo.vulkanApiVersion = mInstance.getApiVersion();

  VmaVulkanFunctions vulkanFunctions{};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  if (mPhysicalDevice.isExtensionSupported(
          VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) &&
      mPhysicalDevice.isExtensionSupported(
          VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME)) {
    allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR =
        vkGetBufferMemoryRequirements2;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR =
        vkGetImageMemoryRequirements2;
  }

  if (features.features12.bufferDeviceAddress == VK_TRUE) {
    allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  }

  checkResult(vmaCreateAllocator(&allocatorInfo, &mMemoryAllocator));
}

Device::~Device() {
  vmaDestroyAllocator(mMemoryAllocator);
  vkDestroyDevice(mHandle, nullptr);
}

Queue& Device::getGraphicsQueue() const {
  for (const auto& queue : queues) {
    const auto& first = queue[0];
    if (first->getProperties().queueCount > 0 && first->canPresent()) {
      return *first;
    }
  }
  throw VulkanException(VK_INCOMPLETE, "No graphics queue found.");
}

Queue& Device::getComputeQueue() const {
  for (const auto& queue : queues) {
    const auto& first = queue[0];
    if (first->getProperties().queueCount > 0 &&
        first->getProperties().queueFlags & VK_QUEUE_COMPUTE_BIT) {
      return *first;
    }
  }
  throw VulkanException(VK_INCOMPLETE, "No compute queue found.");
}

VkResult Device::waitIdle() const {
  return vkDeviceWaitIdle(mHandle);
}

void Device::submitAndWait(const CommandRecorder& recorder) const {
  const auto& queue = getGraphicsQueue();
  CommandPool commandPool{
      {.device = *this, .queueFamilyIndex = queue.getFamilyIndex()}};
  CommandBuffer commandBuffer{{.device = *this, .commandPool = commandPool}};
  commandBuffer.begin();
  recorder(commandBuffer);
  commandBuffer.end();
  vulkan::checkResult(queue.submit(commandBuffer));
  vulkan::checkResult(queue.waitIdle());
}

Queue::Queue(const Desc& desc)
    : Object{desc.device},
      mFamilyIndex{desc.familyIndex},
      mProperties{desc.properties},
      mQueueIndex{desc.queueIndex},
      mCanPresent{desc.canPresent} {
  vkGetDeviceQueue(mDevice.vkHandle(), mFamilyIndex, mQueueIndex, &mHandle);
}

VkResult Queue::submit(const CommandBuffer& commandBuffer,
                       VkFence fence) const {
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = commandBuffer.vkPtr();

  return submit({submitInfo}, fence);
}

VkResult Queue::submit(const std::vector<VkSubmitInfo>& submits,
                       VkFence fence) const {
  return vkQueueSubmit(
      mHandle, static_cast<uint32_t>(submits.size()), submits.data(), fence);
}

VkResult Queue::present(const VkPresentInfoKHR& presentInfo) const {
  return vkQueuePresentKHR(mHandle, &presentInfo);
}

VkResult Queue::waitIdle() const {
  return vkQueueWaitIdle(mHandle);
}

}  // namespace recore::vulkan
