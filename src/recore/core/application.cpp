#include "application.h"

#include <chrono>

namespace recore::core {

HeadlessApplication::HeadlessApplication(const ApplicationSettings& settings)
    : Application{settings} {
  std::vector<std::string> instanceExtensions = {
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
  if (settings.vulkan.instance.enableValidation) {
    instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  mInstance = makeUnique<vulkan::Instance>({
      .enableValidation = settings.vulkan.instance.enableValidation,
      .extensions = instanceExtensions,
      .debugCallback = settings.vulkan.instance.debugCallback,
  });

  const auto& gpus = mInstance->getPhysicalDevices();

  auto deviceExtensions = settings.vulkan.device.deviceExtensions;
  mDevice = makeUnique<vulkan::Device>({
      .instance = *mInstance,
      .physicalDevice = gpus[0],
      .extensions = deviceExtensions,
      .features = settings.vulkan.device.features,
  });

  mRenderContext = makeUnique<vulkan::HeadlessContext>({
      .device = *mDevice,
      .width = mResolution.width,
      .height = mResolution.height,
      .numFramesInFlight = settings.vulkan.numFramesInFlight,
  });
}

void HeadlessApplication::update() {
  // TODO: time for headless applications
  // mRenderer->update(0.5f);
  // mRenderer->update(0.25f);
  // mRenderer->update(0.064f);
  mRenderer->update(0.032f);
  // mRenderer->update(0.016f);
  // mRenderer->update(0.001f);
}

void HeadlessApplication::render() {
  auto& frame = mRenderContext->beginFrame();

  mRenderContext->submit([&](const auto& commandBuffer) {
    mRenderer->render(commandBuffer, frame);
  });

  mRenderContext->endFrame(mRenderer->getResultImage());
}

void HeadlessApplication::run(const ApplicationController& controller) {
  bool running = true;

  while (running) {
    update();
    render();

    running = controller();
  }

  terminate();
}

GUIApplication::GUIApplication(const ApplicationSettings& settings)
    : Application{settings} {
  mWindow = makeUnique<Window>(mTitle, mResolution.width, mResolution.height);
  mWindow->setResizeCallback(
      [&](uint32_t width, uint32_t height) { this->resize(width, height); });

  auto instanceExtensions = mWindow->getVulkanExtensions();
  instanceExtensions.emplace_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  if (settings.vulkan.instance.enableValidation) {
    instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  mInstance = makeUnique<vulkan::Instance>({
      .enableValidation = true,
      .extensions = instanceExtensions,
      .debugCallback = settings.vulkan.instance.debugCallback,
  });

  mSurface = makeUnique<vulkan::Surface>(
      *mInstance, mWindow->createSurface(mInstance->vkHandle()));

  const auto& gpus = mInstance->getPhysicalDevices();

  auto deviceExtensions = settings.vulkan.device.deviceExtensions;
  deviceExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  mDevice = makeUnique<vulkan::Device>({
      .instance = *mInstance,
      .physicalDevice = gpus[0],
      .surface = mSurface->vkHandle(),
      .extensions = deviceExtensions,
      .features = settings.vulkan.device.features,
  });

  mRenderContext = makeUnique<vulkan::RenderContext>({
      .device = *mDevice,
      .surface = *mSurface,
      .width = mResolution.width,
      .height = mResolution.height,
  });
}

void GUIApplication::run() {
  bool running = true;
  auto lastTime = std::chrono::high_resolution_clock::now();

  while (running) {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime =
        std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;

    mWindow->pollEvents();

    update(deltaTime);
    render();

    running = running && !mWindow->shouldClose();
  }

  terminate();
}

void GUIApplication::update(float deltaTime) {
  if (mResolution.width == 0 || mResolution.height == 0) {
    return;
  }

  if (mGui != nullptr) {
    mGui->update();
  }

  mRenderer->update(deltaTime);
}

void GUIApplication::render() {
  if (mResolution.width == 0 || mResolution.height == 0) {
    return;
  }

  auto& frame = mRenderContext->beginFrame();

  mRenderContext->submit([&](const auto& commandBuffer) {
    mRenderer->render(commandBuffer, frame);

    if (mGui != nullptr) {
      mGui->render(commandBuffer, frame);
    }
  });

  mRenderContext->endFrame(mRenderer->getResultImage());
}

void GUIApplication::resize(uint32_t width, uint32_t height) {
  std::cout << "Resize: " << width << "x" << height << std::endl;
  mResolution = {width, height};

  vulkan::checkResult(mDevice->waitIdle());

  if (width == 0 || height == 0) {
    return;
  }

  mRenderContext->resize(width, height);

  mRenderer->resize(width, height);

  if (mGui != nullptr) {
    mGui->setFramebuffer(mRenderer->getResultImage());
  }
}

}  // namespace recore::core
