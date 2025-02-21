#pragma once

#include "device.h"
#include "image.h"
#include "synchronization.h"

namespace recore::vulkan {

class Swapchain : public Object<VkSwapchainKHR> {
 public:
  struct Desc {
    const Device& device;
    const Surface& surface;
    uint32_t width{0};
    uint32_t height{0};
    Swapchain* oldSwapchain{nullptr};
  };

  explicit Swapchain(const Desc& desc);
  ~Swapchain() override;

  [[nodiscard]] uint32_t getImageCount() const { return mImages.size(); }

  [[nodiscard]] VkFormat getFormat() const { return mImageFormat; }

  [[nodiscard]] VkPresentModeKHR getPresentMode() const { return mPresentMode; }

  [[nodiscard]] VkExtent2D getExtent() const { return mSwapchainExtent; }

  [[nodiscard]] uint32_t getWidth() const { return mSwapchainExtent.width; }

  [[nodiscard]] uint32_t getHeight() const { return mSwapchainExtent.height; }

  [[nodiscard]] const Image& getImage(uint32_t index) const {
    return *mImages[index];
  }

  VkResult acquireNextImage(uint32_t* pImageIndex,
                            const Semaphore& imageAvailableSemaphore);

 private:
  const Surface& mSurface;

  VkExtent2D mWindowExtent{};
  VkExtent2D mSwapchainExtent{};
  VkFormat mImageFormat{};

  // std::vector<VkImage> mImages;
  std::vector<uPtr<Image>> mImages;

  VkPresentModeKHR mPresentMode{VK_PRESENT_MODE_IMMEDIATE_KHR};
  VkSurfaceFormatKHR mSurfaceFormat{};

  std::vector<VkPresentModeKHR> mPresentModes;
  std::vector<VkSurfaceFormatKHR> mSurfaceFormats;

  std::vector<VkPresentModeKHR> mPresentModePriorities = {
      // VK_PRESENT_MODE_IMMEDIATE_KHR,
      VK_PRESENT_MODE_FIFO_KHR,
      VK_PRESENT_MODE_MAILBOX_KHR,
      VK_PRESENT_MODE_IMMEDIATE_KHR};

  std::vector<VkSurfaceFormatKHR> mSurfaceFormatPriorities = {
      {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
};

}  // namespace recore::vulkan
