#include "gui.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <recore/vulkan/debug.h>

namespace recore::passes {

GUIPass::GUIPass(const vulkan::Device& device,
                 GLFWwindow* window,
                 uint32_t numFramesInFlight) {
  mDescriptorPool = makeUnique<vulkan::DescriptorPool>(
      {.device = device, .maxSets = 1000, .multiplier = 1000});

  mRenderPass = makeUnique<vulkan::RenderPass>({
      .device = device,
      .attachments =
          {
              {
                  .format = VK_FORMAT_R8G8B8A8_UNORM,
                  .usage = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
              },
          },
  });

  initImGui(device, window, numFramesInFlight);
}

GUIPass::~GUIPass() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void GUIPass::setFramebuffer(const vulkan::Image& image) {
  uint32_t width = image.getWidth();
  uint32_t height = image.getHeight();

  mFramebuffer = makeUnique<vulkan::Framebuffer>({
      .device = mRenderPass->getDevice(),
      .renderPass = *mRenderPass,
      .attachments = {&image.getView()},
      .width = width,
      .height = height,
  });
}

void GUIPass::initImGui(const vulkan::Device& device,
                        GLFWwindow* window,
                        uint32_t numFramesInFlight) {
  // Initialize ImGui with GLFW & Vulkan.
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  // io.ConfigDockingWithShift = false;
  // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  VkInstance instance = device.getInstance().vkHandle();

  ImGui_ImplVulkan_LoadFunctions(
      VK_API_VERSION_1_3,
      [](const char* function_name, void* vulkan_instance) {
        return vkGetInstanceProcAddr(
            *(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
      },
      const_cast<VkInstance*>(&instance));
  ImGui_ImplGlfw_InitForVulkan(window, true);

  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.Instance = instance;
  initInfo.PhysicalDevice = device.getPhysicalDevice().vkHandle();
  initInfo.Device = device.vkHandle();
  initInfo.Queue = device.getGraphicsQueue().vkHandle();
  initInfo.DescriptorPool = mDescriptorPool->vkHandle();
  initInfo.MinImageCount = numFramesInFlight;
  initInfo.ImageCount = numFramesInFlight;
  initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  initInfo.Subpass = 0;
  initInfo.RenderPass = mRenderPass->vkHandle();

  ImGui_ImplVulkan_Init(&initInfo);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void GUIPass::newFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void GUIPass::draw(const vulkan::CommandBuffer& commandBuffer) {
  ImGui::Render();
  ImDrawData* drawdata = ImGui::GetDrawData();

  RECORE_DEBUG_SCOPE(commandBuffer, "GUIPass::draw");

  commandBuffer.beginRenderPass(*mRenderPass,
                                *mFramebuffer,
                                {
                                    {.color = {0.f, 0.f, 0.f, 0.f}},
                                });

  ImGui_ImplVulkan_RenderDrawData(drawdata, commandBuffer.vkHandle());

  commandBuffer.endRenderPass();
}

}  // namespace recore::passes
