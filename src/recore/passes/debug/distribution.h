#pragma once

#include <recore/passes/pass.h>

#include <recore/scene/camera.h>

namespace recore::passes {

class DistributionVisualizationPass : public Pass {
 public:
  explicit DistributionVisualizationPass(const vulkan::Device& device,
                                         const scene::Camera& camera);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  void setFramebuffer(const vulkan::Image& color, const vulkan::Image& depth) {
    mFramebuffer = makeUnique<vulkan::Framebuffer>({
        .device = mRenderPass->getDevice(),
        .renderPass = *mRenderPass,
        .attachments = {&color.getView(), &depth.getView()},
        .width = color.getWidth(),
        .height = color.getHeight(),
    });
  }

  [[nodiscard]] const vulkan::Buffer& getVertexBuffer() const {
    return *mVertexBuffer;
  }

 private:
  const scene::Camera& mCamera;

  uPtr<vulkan::Buffer> mVertexBuffer;
  uPtr<vulkan::Buffer> mIndexBuffer;

  struct Push {
    glm::mat4 viewProjection;
  } mPush;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::RasterPipeline> mPipeline;

  uPtr<vulkan::RenderPass> mRenderPass;
  uPtr<vulkan::Framebuffer> mFramebuffer;
};

}  // namespace recore::passes
