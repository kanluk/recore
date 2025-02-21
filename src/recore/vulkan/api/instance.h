#pragma once

// #define VK_NO_PROTOTYPES
// #include <vk_mem_alloc.h>
#include <volk.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <recore/vulkan/utils.h>

namespace recore::vulkan {

class PhysicalDevice;

class Instance {
 public:
  using DebugCallback = std::function<void(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageType,
      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData)>;

  struct Desc {
    std::string appName = "ReCoRe Vulkan App";
    uint32_t appVersion = VK_MAKE_VERSION(0, 0, 1);
    std::string engineName = "ReCoRe Vulkan Engine";
    uint32_t engineVersion = VK_MAKE_VERSION(0, 0, 1);

    uint32_t vulkanApiVersion = VK_API_VERSION_1_3;
    bool enableValidation = true;

    std::vector<std::string> extensions;

    DebugCallback debugCallback = nullptr;
  };

  explicit Instance(const Desc& desc);
  ~Instance();

  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;
  Instance(Instance&&) = delete;
  Instance& operator=(Instance&&) = delete;

  [[nodiscard]] VkInstance vkHandle() const { return mInstance; }

  [[nodiscard]] const VkInstance* vkPtr() const { return &mInstance; }

  [[nodiscard]] const std::vector<VkExtensionProperties>&
  getAvailableExtensions() const {
    return mAvailableExtensions;
  }

  [[nodiscard]] const std::vector<VkLayerProperties>& getAvailableLayers()
      const {
    return mAvailableLayers;
  }

  [[nodiscard]] const std::vector<PhysicalDevice>& getPhysicalDevices() const {
    return mPhysicalDevices;
  }

  [[nodiscard]] uint32_t getApiVersion() const { return mApiVersion; }

  [[nodiscard]] bool isExtensionEnabled(const char* extensionName) const {
    return std::ranges::contains(mEnabledExtensions, extensionName);
  }

  [[nodiscard]] bool isValidationEnabled() const { return mEnableValidation; }

 private:
  VkInstance mInstance{VK_NULL_HANDLE};

  bool mEnableValidation{false};
  VkDebugUtilsMessengerEXT mDebugMessenger{VK_NULL_HANDLE};
  DebugCallback mDebugCallback;

  uint32_t mApiVersion{VK_API_VERSION_1_0};

  std::vector<VkExtensionProperties> mAvailableExtensions;
  std::vector<VkLayerProperties> mAvailableLayers;
  std::vector<PhysicalDevice> mPhysicalDevices;

  std::vector<std::string> mEnabledExtensions;
};

class PhysicalDevice {
 public:
  explicit PhysicalDevice(VkPhysicalDevice device);

  // ~PhysicalDevice() = default;

  [[nodiscard]] VkPhysicalDevice vkHandle() const { return mDevice; }

  [[nodiscard]] const VkPhysicalDevice* vkPtr() const { return &mDevice; }

  [[nodiscard]] std::string getName() const { return {mProperties.deviceName}; }

  [[nodiscard]] Vendor getVendor() const {
    return static_cast<Vendor>(mProperties.vendorID);
  }

  [[nodiscard]] Version getDriverVersion() const { return mDriverVersion; }

  [[nodiscard]] std::vector<VkQueueFamilyProperties> getQueueFamilies() const {
    return mQueueFamilies;
  }

  [[nodiscard]] bool isExtensionSupported(const std::string& name) const;

  [[nodiscard]] bool isPresentSupported(VkSurfaceKHR surface,
                                        uint32_t queueFamilyIndex) const;
  [[nodiscard]] VkSurfaceCapabilitiesKHR getSurfaceCapabilities(
      VkSurfaceKHR surface) const;

  [[nodiscard]] const VkPhysicalDeviceProperties& getProperties() const {
    return mProperties;
  }

 private:
  VkPhysicalDevice mDevice{VK_NULL_HANDLE};
  VkPhysicalDeviceProperties mProperties{};
  VkPhysicalDeviceFeatures mFeatures{};
  VkPhysicalDeviceMemoryProperties mMemoryProperties{};
  std::vector<VkQueueFamilyProperties> mQueueFamilies;
  std::vector<VkExtensionProperties> mExtensions;

  Version mDriverVersion;
};

class Surface {
 public:
  Surface(const Instance& instance, VkSurfaceKHR surface);

  ~Surface();

  Surface(const Surface&) = delete;
  Surface& operator=(const Surface&) = delete;
  Surface(Surface&&) = delete;
  Surface& operator=(Surface&&) = delete;

  [[nodiscard]] VkSurfaceKHR vkHandle() const { return mSurface; }

  [[nodiscard]] const VkSurfaceKHR* vkPtr() const { return &mSurface; }

 private:
  const Instance& mInstance;
  VkSurfaceKHR mSurface{VK_NULL_HANDLE};
};

}  // namespace recore::vulkan
