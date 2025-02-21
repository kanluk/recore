#include <recore/core/application.h>
#include <recore/core/renderer.h>
#include <recore/core/utils.h>

#include <recore/scene/gpu_scene.h>
#include <recore/vulkan/shader_library.h>

#include <recore/passes/accumulator/accumulator.h>
#include <recore/passes/diffuse_pathtracer/diffuse_pathtracer.h>
#include <recore/passes/gbuffer/gbuffer.h>
#include <recore/passes/gui/gui.h>
#include <recore/passes/svgf/svgf.h>
#include <recore/passes/taa/taa.h>
#include <recore/passes/tone_mapping/tone_mapping.h>

#include <recore/vulkan/debug_messenger.h>

namespace recore {

class SimplePathTracerRenderer : public core::Renderer {
 public:
  SimplePathTracerRenderer(const vulkan::Device& device,
                           core::Resolution resolution,
                           uPtr<scene::Scene>&& scene)
      : mDevice{device}, mResolution{resolution} {
    mShaderLibrary = makeUnique<vulkan::ShaderLibrary>(mDevice,
                                                       RECORE_PROJECT_DIR);

    setScene(std::move(scene));
  }

  void update(float deltaTime) override { mScene->update(deltaTime); }

  void render(const vulkan::CommandBuffer& commandBuffer,
              vulkan::RenderFrame& frame) override {
    RECORE_GPU_PROFILE_SCOPE(frame, commandBuffer, "Total");

    mGPUScene->update(commandBuffer, frame);

    {
      RECORE_GPU_PROFILE_SCOPE(frame, commandBuffer, "Rendering");
      commandBuffer.setFramebufferSize(mResolution.width, mResolution.height);

      // Render GBuffer
      mGBufferPass->execute(commandBuffer, frame);
      {
        const auto& gbuffer = mGBufferPass->getGBuffer();
        commandBuffer.transitionImageLayout(
            {
                &*gbuffer.position,
                &*gbuffer.normal,
                &*gbuffer.albedo,
                &*gbuffer.emission,
                &*gbuffer.motion,
            },
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      }

      mDiffusePathTracerPass->execute(commandBuffer, frame);

      {
        commandBuffer.transitionImageLayout(
            mDiffusePathTracerPass->getOutputImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      }

      mSVGFPass->execute(commandBuffer, frame);

      {
        commandBuffer.transitionImageLayout(
            mSVGFPass->getOutputImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      }

      mTAAPass->execute(commandBuffer, frame);

      {
        commandBuffer.transitionImageLayout(
            mTAAPass->getOutputImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      }

      mAccumulatorPass->execute(commandBuffer, frame);

      commandBuffer.transitionImageLayout(
          mAccumulatorPass->getAccumulatorImage(),
          VK_IMAGE_LAYOUT_GENERAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

      mToneMappingPass->execute(commandBuffer, frame);

      commandBuffer.transitionImageLayout(
          mAccumulatorPass->getAccumulatorImage(),
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          VK_IMAGE_LAYOUT_GENERAL,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
  }

  void resize(uint32_t width, uint32_t height) override {
    mResolution = {width, height};

    mScene->getCamera().setAspect(width, height);

    for (auto& pass : mPasses) {
      pass->resize(width, height);
    }

    // Update dependencies
    buildPassDepencencies();
  }

  [[nodiscard]] const vulkan::Image& getResultImage() const override {
    return mToneMappingPass->getOutputImage();
  }

  void setScene(uPtr<scene::Scene>&& scene) {
    vulkan::checkResult(mDevice.waitIdle());
    mScene = std::move(scene);

    mScene->getCamera().setAspect(mResolution.width, mResolution.height);

    mGPUScene = makeUnique<scene::GPUScene>(mDevice, *mScene, true);
    mGPUScene->upload();

    buildPasses();
  }

  [[nodiscard]] scene::Scene& getScene() { return *mScene; }

  void reload() {
    vulkan::checkResult(mDevice.waitIdle());

    mShaderLibrary->reload();
    for (auto& pass : mPasses) {
      pass->reloadShaders(*mShaderLibrary);
    }
  }

 private:
  template <typename T, typename... Args>
    requires std::is_base_of_v<passes::Pass, T>
  [[nodiscard]] uPtr<T> addPass(Args&&... args) {
    auto pass = makeUnique<T>(std::forward<Args>(args)...);
    pass->resize(mResolution.width, mResolution.height);
    pass->reloadShaders(*mShaderLibrary);
    mPasses.push_back(&*pass);
    return pass;
  }

  void buildPasses() {
    mPasses.clear();

    mGBufferPass = addPass<passes::GBufferPass>(mDevice, *mGPUScene);
    mDiffusePathTracerPass = addPass<passes::DiffusePathTracerPass>(mDevice,
                                                                    *mGPUScene);
    mSVGFPass = addPass<passes::SVGFPass>(mDevice);

    mAccumulatorPass = addPass<passes::AccumulatorPass>(mDevice, *mScene);

    mTAAPass = addPass<passes::TAAPass>(mDevice);
    mGPUScene->enableCameraJitter();

    mToneMappingPass = addPass<passes::ToneMappingPass>(mDevice);

    buildPassDepencencies();
  }

  void buildPassDepencencies() {
    const auto& gBuffer = mGBufferPass->getGBuffer();

    mDiffusePathTracerPass->setInput({
        .gPosition = *gBuffer.position,
        .gNormal = *gBuffer.normal,
        .gAlbedo = *gBuffer.albedo,
        .gEmissive = *gBuffer.emission,
    });

    mSVGFPass->setInput({
        .gPosition = *gBuffer.position,
        .gNormal = *gBuffer.normal,
        .gAlbedo = *gBuffer.albedo,
        .gMotion = *gBuffer.motion,
        .noisyImage = mDiffusePathTracerPass->getOutputImage(),
    });

    mTAAPass->setInput({
        .gMotion = *gBuffer.motion,
        .image = mSVGFPass->getOutputImage(),
    });

    // mAccumulatorPass->setInput(mDiffusePathTracerPass->getOutputImage());
    // mAccumulatorPass->setInput(mSVGFPass->getOutputImage());
    mAccumulatorPass->setInput(mTAAPass->getOutputImage());

    mToneMappingPass->setInput(mAccumulatorPass->getAccumulatorImage());
  }

  const vulkan::Device& mDevice;
  core::Resolution mResolution;

  uPtr<vulkan::ShaderLibrary> mShaderLibrary;

  uPtr<scene::Scene> mScene;
  uPtr<scene::GPUScene> mGPUScene;

  // Passes
  uPtr<passes::GBufferPass> mGBufferPass;
  uPtr<passes::DiffusePathTracerPass> mDiffusePathTracerPass;
  uPtr<passes::SVGFPass> mSVGFPass;
  uPtr<passes::AccumulatorPass> mAccumulatorPass;
  uPtr<passes::TAAPass> mTAAPass;
  uPtr<passes::ToneMappingPass> mToneMappingPass;

  std::vector<passes::Pass*> mPasses;
};

class SimplePathTracerGUI : public core::GUI {
 public:
  explicit SimplePathTracerGUI(const vulkan::Device& device,
                               SimplePathTracerRenderer& renderer,
                               const core::Window& window,
                               vulkan::DebugMessenger& debugMessenger,
                               vulkan::RenderContext& renderContext,
                               uint32_t numFramesInFlight)
      : mDevice{device},
        mRenderer{renderer},
        mDebugMessenger{debugMessenger},
        mRenderContext{renderContext},
        mGuiPass{device, window.glfwHandle(), numFramesInFlight} {
    setFramebuffer(mRenderer.getResultImage());
  }

  void update() override {
    mGuiPass.newFrame();

    updateCamera();

    if (ImGui::IsKeyReleased(ImGuiKey_F5)) {
      mRenderer.reload();
    }

    renderMainGUI();

    for (const auto& report : mDebugMessenger.getReports()) {
      if (report.messageSeverity >=
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cout << report.message << std::endl;
      }
    }
    mDebugMessenger.startNextFrame();
  }

  void render(const vulkan::CommandBuffer& commandBuffer,
              vulkan::RenderFrame& frame) override {
    mGuiPass.draw(commandBuffer);
  }

  void setFramebuffer(const vulkan::Image& image) override {
    mGuiPass.setFramebuffer(image);
  }

 private:
  void renderMainGUI() {
    ImGui::Begin("SimplePathTracerGUI");
    ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);

    ImGui::End();
  }

  void updateCamera() {
    auto& camera = mRenderer.getScene().getCamera();

    ImGuiIO io = ImGui::GetIO();
    float dt = io.DeltaTime;
    bool updated = false;

    auto position = camera.getPosition();
    auto rotation = camera.getRotation();

    float sensitivity = glm::radians(0.3f);
    float speed = 5.f * dt;

    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
        !ImGui::IsWindowFocused(ImGuiHoveredFlags_AnyWindow) &&
        !ImGui::IsAnyItemHovered() &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      ImVec2 mouseDelta = io.MouseDelta;
      float dx = mouseDelta.x * sensitivity;
      float dy = mouseDelta.y * sensitivity;

      rotation.x -= dy;
      rotation.x = glm::clamp(
          rotation.x, glm::radians(-90.f), glm::radians(90.f));
      rotation.y -= dx;
      updated = true;
    }

    if (ImGui::GetIO().KeyShift) {
      speed *= 10;
    }

    auto view = glm::mat3(camera.getView());
    auto invView = glm::inverse(view);

    glm::vec3 positionDelta{0.f};

    if (ImGui::IsKeyDown(ImGuiKey_W)) {
      positionDelta.z -= 1.f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_S)) {
      positionDelta.z += 1.f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A)) {
      positionDelta.x -= 1.f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_D)) {
      positionDelta.x += 1.f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_Space)) {
      positionDelta.y += 1.f;
    }
    if (io.KeyCtrl) {
      positionDelta.y -= 1.f;
    }

    if (positionDelta != glm::vec3{0.f}) {
      updated = true;
    }

    position += invView * positionDelta * speed;

    if (updated) {
      camera.setPosition(position);
      camera.setRotation(rotation);
    }
  }

  const vulkan::Device& mDevice;
  SimplePathTracerRenderer& mRenderer;
  vulkan::DebugMessenger& mDebugMessenger;
  vulkan::RenderContext& mRenderContext;

  passes::GUIPass mGuiPass;
};

}  // namespace recore

void launchSimplePathTracer() {
  using namespace recore;
  using namespace recore::core;

  vulkan::DebugMessenger debugMessenger;

  ApplicationSettings appSettings = {
      .title = "Rendering and Compute for Research",
      .resolution = {.width = 800, .height = 600},
      .vulkan = {
          .instance =
              {
                  .enableValidation = true,
                  .debugCallback =
                      [&](VkDebugUtilsMessageSeverityFlagBitsEXT
                              messageSeverity,
                          VkDebugUtilsMessageTypeFlagsEXT messageType,
                          const VkDebugUtilsMessengerCallbackDataEXT*
                              pCallbackData) {
                        (void)messageType;
                        debugMessenger.report(
                            {messageSeverity, pCallbackData->pMessage});
                      },
              },
          .device =
              {
                  .deviceExtensions =
                      {
                          VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                          VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                          VK_KHR_RAY_QUERY_EXTENSION_NAME,
                          VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME,
                      },
                  .features =
                      {
                          .features = {.geometryShader = VK_TRUE,
                                       .shaderInt64 = VK_TRUE},
                          .features12 =
                              {
                                  .descriptorIndexing = VK_TRUE,
                                  .descriptorBindingPartiallyBound = VK_TRUE,
                                  .runtimeDescriptorArray = VK_TRUE,
                                  .scalarBlockLayout = VK_TRUE,
                                  .bufferDeviceAddress = VK_TRUE,
                              },
                          .featureMap =
                              []() {
                                vulkan::FeatureMap featureMap;
                                featureMap.addFeature<
                                    VkPhysicalDeviceAccelerationStructureFeaturesKHR,
                                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>(
                                    &VkPhysicalDeviceAccelerationStructureFeaturesKHR::
                                        accelerationStructure);
                                featureMap.addFeature<
                                    VkPhysicalDeviceRayQueryFeaturesKHR,
                                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR>(
                                    &VkPhysicalDeviceRayQueryFeaturesKHR::
                                        rayQuery);

                                featureMap.addFeatures<
                                    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT,
                                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT>(
                                    {
                                        &VkPhysicalDeviceShaderAtomicFloatFeaturesEXT::
                                            shaderBufferFloat32Atomics,
                                        &VkPhysicalDeviceShaderAtomicFloatFeaturesEXT::
                                            shaderBufferFloat32AtomicAdd,
                                    });
                                return featureMap;
                              }(),
                      },
              },
      }};

  auto app = makeUnique<GUIApplication>(appSettings);

  // Set scene
  auto scene = makeUnique<scene::Scene>();
  {
    scene->loadGLTF({
        .path = "sponza/Sponza.gltf",
        .scale = {0.01f, 0.01f, 0.01f},
        .name = "sponza",
    });

    scene->addLight({
        .position = {0.f, 20.f, 10.f},
        .direction = {0.8f, -2.f, -1.f},
        .color = {1.f, 1.f, 1.f},
        .intensity = 50.f,
    });

    scene->setCamera(
        scene::Camera{{.position = {-9.733329, 6.8592157, -5.1993685},
                       .rotation = {-0.39793512, -2.403319, 0},
                       .fov = 45,
                       .near = 0.1,
                       .far = 100}});
  }

  auto renderer = makeUnique<SimplePathTracerRenderer>(
      app->getDevice(), appSettings.resolution, std::move(scene));

  auto gui = makeUnique<SimplePathTracerGUI>(app->getDevice(),
                                             *renderer,
                                             app->getWindow(),
                                             debugMessenger,
                                             app->getRenderContext(),
                                             3);

  app->setRenderer(&*renderer);
  app->setGui(&*gui);
  app->run();
}

int main(int /*argc*/, char* /*argv*/[]) {
  try {
    launchSimplePathTracer();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
