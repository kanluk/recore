#include "instance.h"

#include <algorithm>
#include <iostream>
#include <string>

static VKAPI_ATTR VkBool32 VKAPI_CALL
defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                     const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                     void* pUserData) {
  if (auto debugCallback =
          *static_cast<recore::vulkan::Instance::DebugCallback*>(pUserData)) {
    debugCallback(messageSeverity, messageType, pCallbackData);
  } else {
    std::cout << pCallbackData->pMessage << std::endl;
  }

  return VK_FALSE;
}

namespace recore::vulkan {

static constexpr const char* VULKAN_VALIDATION_LAYER_NAME =
    "VK_LAYER_KHRONOS_validation";

static void fillAvailableExtensions(
    std::vector<VkExtensionProperties>& extensions) {
  uint32_t count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  extensions.resize(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
}

static bool isExtensionsAvailable(
    const std::string& name,
    const std::vector<VkExtensionProperties>& extensions) {
  return std::ranges::find_if(extensions.begin(),
                              extensions.end(),
                              [name](const VkExtensionProperties& extension) {
                                return extension.extensionName == name;
                              }) != extensions.end();
}

static void fillAvailableLayers(std::vector<VkLayerProperties>& layers) {
  uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  layers.resize(count);
  vkEnumerateInstanceLayerProperties(&count, layers.data());
}

static bool isLayerAvailable(const std::string& name,
                             const std::vector<VkLayerProperties>& layers) {
  return std::ranges::find_if(layers.begin(),
                              layers.end(),
                              [name](const VkLayerProperties& layer) {
                                return layer.layerName == name;
                              }) != layers.end();
}

Instance::Instance(const Desc& desc)
    : mDebugCallback{desc.debugCallback},
      mApiVersion{desc.vulkanApiVersion},
      mEnableValidation{desc.enableValidation} {
  checkResult(volkInitialize());

  fillAvailableExtensions(mAvailableExtensions);
  fillAvailableLayers(mAvailableLayers);

  // Create instance with requested extensions and layers
  if (mEnableValidation &&
      !isLayerAvailable(VULKAN_VALIDATION_LAYER_NAME, mAvailableLayers)) {
    throw VulkanException(
        VK_ERROR_LAYER_NOT_PRESENT,
        "Tried to enable validation, but layer is not present.");
  }

  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = desc.appName.c_str();
  appInfo.applicationVersion = desc.appVersion;
  appInfo.pEngineName = desc.engineName.c_str();
  appInfo.engineVersion = desc.engineVersion;
  appInfo.apiVersion = desc.vulkanApiVersion;

  VkInstanceCreateInfo instanceInfo{};
  instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceInfo.pApplicationInfo = &appInfo;

  mEnabledExtensions = desc.extensions;
  std::vector<const char*> layerNames{};

  VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
  std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnable = {
      VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};

  // Enable shader debug printf feature
  VkValidationFeaturesEXT validationFeatures{};
  validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
  validationFeatures.enabledValidationFeatureCount = static_cast<uint32_t>(
      validationFeaturesEnable.size());
  validationFeatures.pEnabledValidationFeatures =
      validationFeaturesEnable.data();

  if (mEnableValidation) {
    mEnabledExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    layerNames.push_back(VULKAN_VALIDATION_LAYER_NAME);

    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pUserData = &mDebugCallback;
    debugInfo.pfnUserCallback = defaultDebugCallback;

    validationFeatures.pNext = &debugInfo;
    instanceInfo.pNext = &validationFeatures;
  }

  std::vector<const char*> extensionNamesChar = strvec_to_cchar(
      mEnabledExtensions);
  instanceInfo.enabledExtensionCount = static_cast<uint32_t>(
      extensionNamesChar.size());
  instanceInfo.ppEnabledExtensionNames = extensionNamesChar.data();

  instanceInfo.enabledLayerCount = static_cast<uint32_t>(layerNames.size());
  instanceInfo.ppEnabledLayerNames = layerNames.data();

  checkResult(vkCreateInstance(&instanceInfo, nullptr, &mInstance));

  volkLoadInstance(mInstance);

  if (mEnableValidation) {
    checkResult(vkCreateDebugUtilsMessengerEXT(
        mInstance, &debugInfo, nullptr, &mDebugMessenger));
  }

  // Query gpus
  std::vector<VkPhysicalDevice> devices{};
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
  devices.resize(count);
  vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

  for (VkPhysicalDevice device : devices) {
    mPhysicalDevices.emplace_back(device);
  }
}

Instance::~Instance() {
  if (mDebugMessenger != VK_NULL_HANDLE) {
    vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
  }
  vkDestroyInstance(mInstance, nullptr);
}

PhysicalDevice::PhysicalDevice(VkPhysicalDevice device) : mDevice{device} {
  vkGetPhysicalDeviceProperties(device, &mProperties);
  vkGetPhysicalDeviceFeatures(device, &mFeatures);

  vkGetPhysicalDeviceMemoryProperties(device, &mMemoryProperties);

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  mQueueFamilies.resize(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(
      device, &queueFamilyCount, mQueueFamilies.data());

  uint32_t extensionCount = 0;
  vkEnumerateDeviceExtensionProperties(
      device, nullptr, &extensionCount, nullptr);
  mExtensions.resize(extensionCount);
  vkEnumerateDeviceExtensionProperties(
      device, nullptr, &extensionCount, mExtensions.data());

  mDriverVersion = Version{getVendor(), mProperties.driverVersion};
}

bool PhysicalDevice::isExtensionSupported(const std::string& name) const {
  return std::ranges::find_if(
             mExtensions.begin(),
             mExtensions.end(),
             [&name](const VkExtensionProperties& extension) {
               return std::string_view(extension.extensionName) == name;
             }) != mExtensions.end();
}

bool PhysicalDevice::isPresentSupported(VkSurfaceKHR surface,
                                        uint32_t queueFamilyIndex) const {
  VkBool32 supported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(
      mDevice, queueFamilyIndex, surface, &supported);
  return supported != 0U;
}

VkSurfaceCapabilitiesKHR PhysicalDevice::getSurfaceCapabilities(
    VkSurfaceKHR surface) const {
  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mDevice, surface, &capabilities);
  return capabilities;
}

Surface::Surface(const Instance& instance, VkSurfaceKHR surface)
    : mInstance{instance}, mSurface{surface} {}

Surface::~Surface() {
  vkDestroySurfaceKHR(mInstance.vkHandle(), mSurface, nullptr);
}

}  // namespace recore::vulkan
