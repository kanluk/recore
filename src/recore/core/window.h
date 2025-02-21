#pragma once

#include "base.h"

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace recore::core {

class Window : public NoCopyMove {
 public:
  using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

  Window(const std::string& title, uint32_t width, uint32_t height);
  ~Window();

  [[nodiscard]] GLFWwindow* glfwHandle() const { return mWindow; }

  [[nodiscard]] VkSurfaceKHR createSurface(VkInstance instance) const;
  [[nodiscard]] std::vector<std::string> getVulkanExtensions() const;

  [[nodiscard]] float getCurrentTime() const;
  void pollEvents() const;
  [[nodiscard]] bool shouldClose() const;

  void setResizeCallback(ResizeCallback&& resizeCallback) {
    mResizeCallback = resizeCallback;
  }

  [[nodiscard]] uint32_t getWidth() const { return mWidth; }

  [[nodiscard]] uint32_t getHeight() const { return mHeight; }

 private:
  GLFWwindow* mWindow{nullptr};
  uint32_t mWidth;
  uint32_t mHeight;

  ResizeCallback mResizeCallback;

  static void glfwFramebufferSizeCallback(GLFWwindow* window,
                                          int width,
                                          int height);
};

}  // namespace recore::core
