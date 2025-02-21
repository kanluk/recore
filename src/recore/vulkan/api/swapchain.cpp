#include "swapchain.h"

namespace recore::vulkan {

static std::vector<VkPresentModeKHR> getPresentModes(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
  std::vector<VkPresentModeKHR> presentModes{};
  uint32_t presentCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physicalDevice, surface, &presentCount, nullptr);
  presentModes.resize(presentCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physicalDevice, surface, &presentCount, presentModes.data());
  return presentModes;
}

static std::vector<VkSurfaceFormatKHR> getSurfaceFormats(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
  std::vector<VkSurfaceFormatKHR> formats{};
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physicalDevice, surface, &formatCount, nullptr);
  formats.resize(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physicalDevice, surface, &formatCount, formats.data());
  return formats;
}

static VkPresentModeKHR choosePresentMode(
    const std::vector<VkPresentModeKHR>& modes,  // NOLINT
    const std::vector<VkPresentModeKHR>& priorities) {
  for (VkPresentModeKHR wanted : priorities) {
    if (std::find(modes.begin(), modes.end(), wanted) != modes.end()) {
      return wanted;
    }
  }
  return modes[0];  // Just return first mo de if priorities are not supported
}

static VkSurfaceFormatKHR chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats,  // NOLINT
    const std::vector<VkSurfaceFormatKHR>& priorities) {
  for (VkSurfaceFormatKHR wanted : priorities) {
    if (std::ranges::find_if(formats.begin(),
                             formats.end(),
                             [wanted](VkSurfaceFormatKHR format) {
                               return wanted.format == format.format &&
                                      wanted.colorSpace == format.colorSpace;
                             }) != formats.end()) {
      return wanted;
    }
  }
  return formats[0];
}

static VkExtent2D chooseExtent(const VkExtent2D& windowExtent,
                               const VkSurfaceCapabilitiesKHR& capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    VkExtent2D actualExtent = windowExtent;
    actualExtent.width = std::clamp(actualExtent.width,
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                     capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

Swapchain::Swapchain(const Desc& desc)
    : Object{desc.device},
      mSurface{desc.surface},
      mWindowExtent{desc.width, desc.height} {
  PhysicalDevice physicalDevice = mDevice.getPhysicalDevice();
  VkSurfaceKHR surface = desc.surface.vkHandle();

  mPresentModes = getPresentModes(physicalDevice.vkHandle(), surface);
  mSurfaceFormats = getSurfaceFormats(physicalDevice.vkHandle(), surface);

  VkSurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilities(
      surface);

  mPresentMode = choosePresentMode(mPresentModes, mPresentModePriorities);
  mSurfaceFormat = chooseSurfaceFormat(mSurfaceFormats,
                                       mSurfaceFormatPriorities);

  mImageFormat = mSurfaceFormat.format;
  mSwapchainExtent = chooseExtent(mWindowExtent, capabilities);

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchainInfo{};
  swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainInfo.surface = surface;
  swapchainInfo.minImageCount = imageCount;
  swapchainInfo.imageFormat = mSurfaceFormat.format;
  swapchainInfo.imageColorSpace = mSurfaceFormat.colorSpace;
  swapchainInfo.imageExtent = mSwapchainExtent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.queueFamilyIndexCount = 0;
  swapchainInfo.pQueueFamilyIndices = nullptr;

  swapchainInfo.preTransform = capabilities.currentTransform;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = mPresentMode;
  swapchainInfo.clipped = VK_TRUE;
  swapchainInfo.oldSwapchain = (desc.oldSwapchain != nullptr)
                                   ? desc.oldSwapchain->mHandle
                                   : VK_NULL_HANDLE;

  checkResult(vkCreateSwapchainKHR(
      mDevice.vkHandle(), &swapchainInfo, nullptr, &mHandle));

  // Get images
  std::vector<VkImage> images{};
  vkGetSwapchainImagesKHR(mDevice.vkHandle(), mHandle, &imageCount, nullptr);
  images.resize(imageCount);
  vkGetSwapchainImagesKHR(
      mDevice.vkHandle(), mHandle, &imageCount, images.data());

  mImages.resize(images.size());
  for (size_t i = 0; i < images.size(); i++) {
    mImages[i] = makeUnique<Image>(
        Image::Desc{.device = mDevice,
                    .format = mImageFormat,
                    .extent = {.width = mSwapchainExtent.width,
                               .height = mSwapchainExtent.height},
                    .usage = swapchainInfo.imageUsage},
        images[i]);
  }
}

Swapchain::~Swapchain() {
  vkDestroySwapchainKHR(mDevice.vkHandle(), mHandle, nullptr);
}

VkResult Swapchain::acquireNextImage(uint32_t* pImageIndex,
                                     const Semaphore& imageAvailableSemaphore) {
  VkResult result = vkAcquireNextImageKHR(mDevice.vkHandle(),
                                          mHandle,
                                          std::numeric_limits<uint64_t>::max(),
                                          imageAvailableSemaphore.vkHandle(),
                                          VK_NULL_HANDLE,
                                          pImageIndex);
  return result;
}

}  // namespace recore::vulkan
