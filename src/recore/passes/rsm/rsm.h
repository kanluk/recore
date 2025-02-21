#pragma once

#include <recore/passes/pass.h>

#include <recore/scene/gpu_scene.h>

namespace recore::passes {

struct RSM {
  uPtr<vulkan::Image> position, normal, flux, depth;
};

class RSMPass : public Pass {
 public:
  explicit RSMPass(const vulkan::Device& device,
                   const scene::GPUScene& scene,
                   uint32_t width,
                   uint32_t height);

  void reloadShaders(vulkan::ShaderLibrary& shaderLibrary) override;

  void execute(const vulkan::CommandBuffer& commandBuffer,
               vulkan::RenderFrame& currentFrame) override;

  [[nodiscard]] const RSM& getRSM() const { return mRSM; };

 private:
  const scene::GPUScene& mScene;

  uint32_t mWidth;
  uint32_t mHeight;

  uPtr<vulkan::RenderPass> mRenderPass;

  uPtr<vulkan::PipelineLayout> mPipelineLayout;
  uPtr<vulkan::RasterPipeline> mPipeline;

  RSM mRSM;
  uPtr<vulkan::Framebuffer> mFramebuffer;
};

}  // namespace recore::passes
