#pragma once

#include <cstdint>
#include <string>

#include "base.h"
#include "utils.h"
#include "window.h"

#include <recore/vulkan/api/device.h>
#include <recore/vulkan/api/instance.h>
#include <recore/vulkan/context.h>

#include <iostream>

#include "gui.h"
#include "renderer.h"

namespace recore::core {

struct ApplicationSettings {
  std::string title = "Rendering and Compute for Research";

  Resolution resolution;

  struct {
    struct {
      bool enableValidation{true};
      vulkan::Instance::DebugCallback debugCallback;
    } instance;

    struct {
      std::vector<std::string> deviceExtensions;
      vulkan::Device::Features features;
    } device;

    uint32_t numFramesInFlight = 2;
  } vulkan;
};

class Application : public NoCopyMove {
 public:
  explicit Application(const ApplicationSettings& settings)
      : mTitle{settings.title}, mResolution{settings.resolution} {}

  virtual ~Application() = default;

  virtual void terminate() {
    std::cout << "Terminate!" << std::endl;
    vulkan::checkResult(mDevice->waitIdle());
  }

  [[nodiscard]] const vulkan::Instance& getInstance() const {
    return *mInstance;
  }

  [[nodiscard]] const vulkan::Device& getDevice() const { return *mDevice; }

 protected:
  std::string mTitle;

  Resolution mResolution;

  uPtr<vulkan::Instance> mInstance;
  uPtr<vulkan::Device> mDevice;
};

using ApplicationController = std::function<bool()>;

class HeadlessApplication : public Application {
 public:
  explicit HeadlessApplication(const ApplicationSettings& settings);

  ~HeadlessApplication() override = default;

  void update();

  void render();

  void run(const ApplicationController& controller = {});

  void setRenderer(Renderer* renderer) { mRenderer = renderer; }

 protected:
  uPtr<vulkan::HeadlessContext> mRenderContext;

  Renderer* mRenderer{nullptr};
};

class GUIApplication : public Application {
 public:
  explicit GUIApplication(const ApplicationSettings& settings);

  ~GUIApplication() override = default;

  void run();

  void update(float deltaTime);

  void render();

  void resize(uint32_t width, uint32_t height);

  void setRenderer(Renderer* renderer) { mRenderer = renderer; }

  void setGui(GUI* gui) { mGui = gui; }

  [[nodiscard]] const Window& getWindow() const { return *mWindow; }

  [[nodiscard]] vulkan::RenderContext& getRenderContext() {
    return *mRenderContext;
  }

 protected:
  uPtr<Window> mWindow;
  uPtr<vulkan::Surface> mSurface;
  uPtr<vulkan::RenderContext> mRenderContext;

  Renderer* mRenderer{nullptr};
  GUI* mGui{nullptr};
};

}  // namespace recore::core
