#pragma once

#include <imgui.h>

#include <recore/vulkan/api/descriptor.h>
#include <recore/vulkan/context.h>

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace recore::passes {

class GUIPass : public NoCopyMove {
 public:
  GUIPass(const vulkan::Device& device,
          GLFWwindow* window,
          uint32_t numFramesInFlight);

  ~GUIPass();

  void setFramebuffer(const vulkan::Image& image);

  void newFrame();
  void draw(const vulkan::CommandBuffer& commandBuffer);

 private:
  void initImGui(const vulkan::Device& device,
                 GLFWwindow* window,
                 uint32_t numFramesInFlight);

  uPtr<vulkan::DescriptorPool> mDescriptorPool;

  uPtr<vulkan::RenderPass> mRenderPass;
  uPtr<vulkan::Framebuffer> mFramebuffer;
};

}  // namespace recore::passes
