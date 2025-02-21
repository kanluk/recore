#include "window.h"

#include <span>

#include <recore/vulkan/utils.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace recore::core {

Window::Window(const std::string& title, uint32_t width, uint32_t height)
    : mWidth{width}, mHeight{height} {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
  mWindow = glfwCreateWindow(
      (int)width, (int)height, title.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(mWindow, this);
  glfwSetFramebufferSizeCallback(mWindow, glfwFramebufferSizeCallback);
}

Window::~Window() {
  glfwDestroyWindow(mWindow);
  glfwTerminate();
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
  // TODO: directly construct vulkan surface maybe?
  VkSurfaceKHR surface = nullptr;
  vulkan::checkResult(
      glfwCreateWindowSurface(instance, mWindow, nullptr, &surface));
  return surface;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::vector<std::string> Window::getVulkanExtensions() const {
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = nullptr;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::span<const char*> span{glfwExtensions, glfwExtensionCount};

  std::vector<std::string> extensions{};
  extensions.resize(glfwExtensionCount);

  for (uint32_t i = 0; i < glfwExtensionCount; i++) {
    extensions[i] = span[i];
  }
  return extensions;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
float Window::getCurrentTime() const {
  return static_cast<float>(glfwGetTime());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Window::pollEvents() const {
  glfwPollEvents();
}

bool Window::shouldClose() const {
  return glfwWindowShouldClose(mWindow) != 0;
}

void Window::glfwFramebufferSizeCallback(GLFWwindow* window,
                                         int width,
                                         int height) {
  auto* myWindow = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));

  myWindow->mWidth = static_cast<uint32_t>(width);
  myWindow->mHeight = static_cast<uint32_t>(height);

  myWindow->mResizeCallback(width, height);
}

}  // namespace recore::core
